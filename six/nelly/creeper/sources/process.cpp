#include "process.h"
#include "resolver.h"

#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <unordered_map>
#include <tlhelp32.h>
#include <cstring>
#include <vector>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// Function pointer signatures for NT native APIs
typedef NTSTATUS(NTAPI* pfnNtOpenProcess)(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId
);

typedef NTSTATUS(NTAPI* pfnNtClose)(
    HANDLE Handle
);

typedef NTSTATUS(NTAPI* pfnNtReadVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T BufferSize,
    PSIZE_T NumberOfBytesRead
);

typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

// Typedef for NtQueryVirtualMemory
typedef NTSTATUS(NTAPI* pfnNtQueryVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    int MemoryInformationClass, // 0 = MemoryBasicInformation
    PVOID MemoryInformation,
    SIZE_T MemoryInformationLength,
    PSIZE_T ReturnLength
);

struct PipeContext {
    std::string name;
    std::wstring pipeNameW;
    HANDLE activeWritePipe = INVALID_HANDLE_VALUE; // Current handle being written to
    std::mutex handleMutex;
    std::atomic<bool> isRunning{true};
};

struct FloatMonitorParams {
    HANDLE hProcess;
    DWORD processId;
    std::string ptyTarget;
    uintptr_t startAddress;
    uintptr_t endAddress;
    int intervalMs;
    SIZE_T stride;
};

// Global control flags for the background monitor worker
static std::atomic<bool> g_isMonitoring(false);
static std::thread g_monitorThread;
static std::atomic<bool> g_isFloatMonitoring(false);
static std::thread g_floatMonitorThread;
static std::mutex g_pipeMutex;
static HANDLE g_hFloatMonitorThread = NULL;
static std::atomic<bool> g_isFloatFiltering{false};
static HANDLE g_hFloatFilterThread = NULL;

struct ActivePipe {
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    std::string name;
    std::thread connectionThread;
};

// active data structures
static std::map<std::string, std::unique_ptr<PipeContext>> g_activePipes;

namespace ProcessHelper {
    HANDLE CreateNewPipeInstance(const std::wstring& pipeNameW) {
        return CreateNamedPipeW(
            pipeNameW.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536,
            65536,
            0,
            nullptr
        );
    } //\ CreateNewPipeInstance

// Native Windows thread procedure for listening to pipe connections
    DWORD WINAPI PipeListenerThreadProc(LPVOID lpParam) {
        PipeContext* ctx = static_cast<PipeContext*>(lpParam);

        while (ctx->isRunning) {
            // 1. Always spin up a fresh, idle pipe instance ready for the next reader
            HANDLE hNextPipe = CreateNewPipeInstance(ctx->pipeNameW);
            if (hNextPipe == INVALID_HANDLE_VALUE) {
                std::cerr << "[-] Pipe '" << ctx->name << "' instance error: " << GetLastError() << "\n";
                Sleep(500);
                continue;
            }

            // 2. Wait for a reader (cat) to attach to this fresh instance
            BOOL connected = ConnectNamedPipe(hNextPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (connected && ctx->isRunning) {
                std::cout << "\n[+] Reader connected to pipe '" << ctx->name << "'!\n" << std::flush;

                // 3. Swap the newly connected handle in as the active output target, closing any stale one
                std::lock_guard<std::mutex> lock(ctx->handleMutex);
                if (ctx->activeWritePipe != INVALID_HANDLE_VALUE) {
                    DisconnectNamedPipe(ctx->activeWritePipe);
                    CloseHandle(ctx->activeWritePipe);
                }
                ctx->activeWritePipe = hNextPipe;
                // Loop immediately repeats, creating a new waiting instance so the pipe is NEVER busy
            } else {
                CloseHandle(hNextPipe);
                break;
            }
        }
        return 0;
    } //\ PipeListenerThreadProc

    bool StartCustomPipe(const std::string& pipeName) {
        std::lock_guard<std::mutex> lock(g_pipeMutex);
        if (g_activePipes.find(pipeName) != g_activePipes.end()) {
            std::cout << "[-] Pipe '" << pipeName << "' already exists.\n";
            return false;
        }

        auto ctx = std::make_unique<PipeContext>();
        ctx->name = pipeName;
        ctx->pipeNameW = L"\\\\.\\pipe\\" + std::wstring(pipeName.begin(), pipeName.end());
        ctx->isRunning = true;

        // Pre-warm 3 concurrent pipe listener threads so the kernel ALWAYS has idle instances ready
        for (int i = 0; i < 3; ++i) {
            HANDLE hThread = CreateThread(nullptr, 0, PipeListenerThreadProc, ctx.get(), 0, nullptr);
            if (hThread) {
                CloseHandle(hThread);
            }
        }

        g_activePipes.emplace(pipeName, std::move(ctx));
        std::cout << "[+] Custom pipe '" << pipeName << "' registered (3x pool) on //./pipe/" << pipeName << "\n";
        return true;
    } //\ StartCustomPipe

    // Helper to locate the first active thread ID for a target process
    DWORD GetFirstThreadId(DWORD processId) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

        THREADENTRY32 te;
        te.dwSize = sizeof(THREADENTRY32);
        DWORD targetThreadId = 0;

        if (Thread32First(hSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == processId) {
                    targetThreadId = te.th32ThreadID;
                    break;
                }
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
        return targetThreadId;
    }

    // Generic Background Worker for Register/State Monitoring
    void GenericMonitorWorker(DWORD processId) {
        std::cout << "\n[*] Floating Registers Monitor active for PID " << processId 
                  << " -> Streaming to console. Type 'stop' to halt.\n" << std::flush;

        DWORD threadId = GetFirstThreadId(processId);
        if (!threadId) {
            std::cerr << "[-] Error: No active threads found for PID " << processId << "\n";
            g_isMonitoring = false;
            return;
        }

        while (g_isMonitoring) {
            HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
            if (hThread) {
                SuspendThread(hThread);

                CONTEXT ctx;
                std::memset(&ctx, 0, sizeof(CONTEXT));
                ctx.ContextFlags = CONTEXT_FLOATING_POINT; // Targets FPU & XMM registers
                
                if (GetThreadContext(hThread, &ctx)) {
                    // \r returns to start, \033[2K completely clears the current console line
                    std::cout << "\r\033[2K[PID:" << processId << "] ";
                    
                    for (int i = 0; i < 4; i++) {
                        std::cout << "XMM" << i << ":[0x" << std::hex << std::setfill('0') 
                                  << std::setw(16) << ctx.VectorRegister[i].High 
                                  << std::setw(16) << ctx.VectorRegister[i].Low << std::dec << "] ";
                    }
                    std::cout << std::flush;
                }

                ResumeThread(hThread);
                CloseHandle(hThread);
            }

            // ~4 FPS refresh rate to keep console responsive
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        std::cout << "\n[+] Floating register monitor stopped.\n" << std::flush;
        
    } // GenericMonitorWorker

    bool StartMonitor(HANDLE hProcess, DWORD processId) {
        // Suppress unused parameter warning since the thread worker only requires the PID
        (void)hProcess;
        
        if (g_isMonitoring) {
            std::cout << "[-] A monitor task is already active. Type 'stop' first.\n";
            return false;
        }

        g_isMonitoring = true;
        // Launch background worker printing directly to the local terminal
        g_monitorThread = std::thread(GenericMonitorWorker, processId);
        return true;
    } //\ StartMonitor

    void StopMonitor() {
        if (!g_isMonitoring) {
            std::cout << "[-] No active monitor running.\n";
            return;
        }

        g_isMonitoring = false;
        if (g_monitorThread.joinable()) {
            g_monitorThread.join(); // Safely join the background thread back to main
        }
        std::cout << "\n[+] Monitor successfully stopped.\n";
    } // StopMonitor

    // Helper to manually find functions in ntdll.dll export table without kernel32
    LPVOID FindExport(HMODULE hModule, const char* lpFunctionName) {
        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);
        
        PIMAGE_EXPORT_DIRECTORY pExportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hModule + 
            pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

        DWORD* pAddressOfFunctions = (DWORD*)((BYTE*)hModule + pExportDir->AddressOfFunctions);
        DWORD* pAddressOfNames = (DWORD*)((BYTE*)hModule + pExportDir->AddressOfNames);
        WORD* pAddressOfNameOrdinals = (WORD*)((BYTE*)hModule + pExportDir->AddressOfNameOrdinals);

        for (DWORD i = 0; i < pExportDir->NumberOfNames; i++) {
            char* functionName = (char*)hModule + pAddressOfNames[i];
            if (strcmp(functionName, lpFunctionName) == 0) {
                WORD ordinal = pAddressOfNameOrdinals[i];
                return (LPVOID)((BYTE*)hModule + pAddressOfFunctions[ordinal]);
            }
        }
        return nullptr;
    }

    // Helper to get ntdll.dll handle directly from the Process Environment Block (PEB)
    HMODULE GetNtdllHandle() {
    #if defined(_WIN64)
        PPEB pPeb = (PPEB)__readgsqword(0x60);
    #else
        PPEB pPeb = (PPEB)__readfsdword(0x30);
    #endif
        
        PPEB_LDR_DATA pLdr = pPeb->Ldr;
        PLIST_ENTRY pListHead = &pLdr->InMemoryOrderModuleList;
        PLIST_ENTRY pListCurrent = pListHead->Flink;

        // Second entry in InMemoryOrder is typically ntdll.dll
        PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListCurrent, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        pEntry = CONTAINING_RECORD(pEntry->InMemoryOrderLinks.Flink, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

        return (HMODULE)pEntry->DllBase;
    }

    HANDLE OpenProcessRead(DWORD processId) {
        HMODULE hNtdll = GetNtdllHandle();
        if (!hNtdll) return NULL;

        pfnNtOpenProcess NtOpenProcess = (pfnNtOpenProcess)FindExport(hNtdll, "NtOpenProcess");
        if (!NtOpenProcess) return NULL;

        HANDLE hProcess = NULL;
        OBJECT_ATTRIBUTES objAttr;
        InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);

        CLIENT_ID clientId;
        clientId.UniqueProcess = (HANDLE)(ULONG_PTR)processId;
        clientId.UniqueThread = NULL;

        // Least-privilege access: read-only memory and query flags
        ACCESS_MASK desiredAccess = PROCESS_VM_READ | PROCESS_QUERY_INFORMATION;

        NTSTATUS status = NtOpenProcess(&hProcess, desiredAccess, &objAttr, &clientId);
        if (status >= 0) { // NT_SUCCESS check
            return hProcess;
        }

        return NULL;
    }

    void CloseHandleNative(HANDLE hHandle) {
        if (!hHandle || hHandle == INVALID_HANDLE_VALUE) return;
        HMODULE hNtdll = GetNtdllHandle();
        if (!hNtdll) return;

        pfnNtClose NtClose = (pfnNtClose)FindExport(hNtdll, "NtClose");
        if (NtClose) {
            NtClose(hHandle);
        }
    }

    uintptr_t GetProcessBaseAddress(HANDLE hProcess) {
        HMODULE hNtdll = GetNtdllHandle();
        if (!hNtdll) return 0;

        pfnNtQueryInformationProcess NtQueryInformationProcess = 
            (pfnNtQueryInformationProcess)FindExport(hNtdll, "NtQueryInformationProcess");
        pfnNtReadVirtualMemory NtReadVirtualMemory = 
            (pfnNtReadVirtualMemory)FindExport(hNtdll, "NtReadVirtualMemory");

        if (!NtQueryInformationProcess || !NtReadVirtualMemory) return 0;

        PROCESS_BASIC_INFORMATION pbi;
        ULONG returnLength = 0;

        NTSTATUS status = NtQueryInformationProcess(
            hProcess, 
            0, 
            &pbi, 
            sizeof(pbi), 
            &returnLength
        );

        if (status < 0 || !pbi.PebBaseAddress) {
            return 0;
        }

        uintptr_t imageBasePtrLocation = (uintptr_t)pbi.PebBaseAddress + 
    #if defined(_WIN64)
            0x10;
    #else
            0x8;
    #endif

        uintptr_t imageBaseAddress = 0;
        SIZE_T bytesRead = 0;
        status = NtReadVirtualMemory(hProcess, (PVOID)imageBasePtrLocation, &imageBaseAddress, sizeof(imageBaseAddress), &bytesRead);

        if (status >= 0 && bytesRead == sizeof(imageBaseAddress)) {
            return imageBaseAddress;
        }

        return 0;
    }

    bool ReadMemoryRange(HANDLE hProcess, uintptr_t startAddress, uintptr_t finishAddress, unsigned char* outBuffer, SIZE_T bufferSize, SIZE_T* totalBytesRead) {
        if (finishAddress <= startAddress || bufferSize == 0) return false;

        HMODULE hNtdll = GetNtdllHandle();
        if (!hNtdll) return false;

        pfnNtReadVirtualMemory NtReadVirtualMemory = (pfnNtReadVirtualMemory)FindExport(hNtdll, "NtReadVirtualMemory");
        if (!NtReadVirtualMemory) return false;

        uintptr_t currentAddress = startAddress;
        SIZE_T totalRead = 0;
        SIZE_T bytesRemaining = finishAddress - startAddress;

        while (currentAddress < finishAddress && totalRead < bufferSize) {
            SIZE_T bytesToRead = (bytesRemaining < (bufferSize - totalRead)) ? bytesRemaining : (bufferSize - totalRead);
            SIZE_T bytesReadThisChunk = 0;

            NTSTATUS status = NtReadVirtualMemory(
                hProcess, 
                (PVOID)currentAddress, 
                outBuffer + totalRead, 
                bytesToRead, 
                &bytesReadThisChunk
            );

            if (status >= 0 && bytesReadThisChunk > 0) {
                totalRead += bytesReadThisChunk;
                currentAddress += bytesReadThisChunk;
                bytesRemaining -= bytesReadThisChunk;
            } else {
                outBuffer[totalRead] = 0; 
                totalRead++;
                currentAddress++;
                bytesRemaining--;
            }
        }

        if (totalBytesRead) {
            *totalBytesRead = totalRead;
        }

        return (totalRead > 0);
    }

    bool DumpToFile(const unsigned char* buffer, SIZE_T size, uintptr_t baseAddress, const std::string& filename) {
        std::ofstream outFile(filename, std::ios::out | std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "[-] Failed to open file '" << filename << "' for writing.\n";
            return false;
        }

        for (SIZE_T i = 0; i < size; i += 16) {
            outFile << "  0x" << std::hex << std::setw(8) << std::setfill('0') << (baseAddress + i) << ": ";
            
            for (SIZE_T j = 0; j < 16; j++) {
                if (i + j < size) {
                    outFile << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i + j] << " ";
                } else {
                    outFile << "   ";
                }
            }

            outFile << " | ";

            for (SIZE_T j = 0; j < 16 && i + j < size; j++) {
                unsigned char c = buffer[i + j];
                outFile << (char)(std::isprint(c) ? c : '.');
            }
            outFile << std::dec << "\n";
        }

        outFile.close();
        return true;
    }

    void PrintHexDump(const unsigned char* buffer, SIZE_T size, uintptr_t baseAddress) {
        for (SIZE_T i = 0; i < size; i += 16) {
            std::cout << "  0x" << std::hex << std::setw(8) << std::setfill('0') << (baseAddress + i) << ": ";
            
            for (SIZE_T j = 0; j < 16; j++) {
                if (i + j < size) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i + j] << " ";
                } else {
                    std::cout << "   ";
                }
            }

            std::cout << " | ";

            for (SIZE_T j = 0; j < 16 && i + j < size; j++) {
                unsigned char c = buffer[i + j];
                std::cout << (char)(std::isprint(c) ? c : '.');
            }
            std::cout << std::dec << "\n";
        }
    }

    //\ print process modules
    bool PrintProcessModules(HANDLE hProcess) {
        HMODULE hNtdll = GetNtdllHandle();
        if (!hNtdll) return false;

        pfnNtQueryInformationProcess NtQueryInformationProcess = 
            (pfnNtQueryInformationProcess)FindExport(hNtdll, "NtQueryInformationProcess");
        pfnNtReadVirtualMemory NtReadVirtualMemory = 
            (pfnNtReadVirtualMemory)FindExport(hNtdll, "NtReadVirtualMemory");

        if (!NtQueryInformationProcess || !NtReadVirtualMemory) return false;

        PROCESS_BASIC_INFORMATION pbi;
        ULONG returnLength = 0;
        NTSTATUS status = NtQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), &returnLength);
        if (status < 0 || !pbi.PebBaseAddress) return false;

        // Read PEB.Ldr pointer (Offset 0x18 on x64)
        uintptr_t pebLdrPtrLocation = (uintptr_t)pbi.PebBaseAddress + 
#if defined(_WIN64)
            0x18;
#else
            0x0C;
#endif

        PVOID pLdr = nullptr;
        SIZE_T bytesRead = 0;
        status = NtReadVirtualMemory(hProcess, (PVOID)pebLdrPtrLocation, &pLdr, sizeof(pLdr), &bytesRead);
        if (status < 0 || !pLdr) return false;

        // Read PEB_LDR_DATA.InMemoryOrderModuleList head (Offset 0x20 on x64, 0x14 on x86)
        uintptr_t listHeadPtrLocation = (uintptr_t)pLdr + 
#if defined(_WIN64)
            0x20;
#else
            0x14;
#endif

        LIST_ENTRY listHead;
        status = NtReadVirtualMemory(hProcess, (PVOID)listHeadPtrLocation, &listHead, sizeof(listHead), &bytesRead);
        if (status < 0) return false;

        PLIST_ENTRY pCurrentLink = listHead.Flink;
        std::cout << "\n[+] Loaded Modules for Target Process:\n";
        std::cout << "------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(20) << "Base Address" << " | " << "Module Name" << "\n";
        std::cout << "------------------------------------------------------------------------\n";

        int safetyCounter = 0;
        while (pCurrentLink != (PLIST_ENTRY)listHeadPtrLocation && safetyCounter < 500) {
            // InMemoryOrderLinks offset adjustment (0x10 on x64, 0x08 on x86)
            uintptr_t pEntry = (uintptr_t)pCurrentLink - 
#if defined(_WIN64)
                0x10;
#else
                0x08;
#endif

            // DllBase offset (0x30 on x64, 0x18 on x86)
            PVOID dllBase = nullptr;
            status = NtReadVirtualMemory(hProcess, (PVOID)(pEntry + 
#if defined(_WIN64)
                0x30
#else
                0x18
#endif
            ), &dllBase, sizeof(dllBase), &bytesRead);

            // BaseDllName UNICODE_STRING offset (0x58 on x64, 0x2C on x86)
            UNICODE_STRING baseDllName;
            status = NtReadVirtualMemory(hProcess, (PVOID)(pEntry + 
#if defined(_WIN64)
                0x58
#else
                0x2C
#endif
            ), &baseDllName, sizeof(baseDllName), &bytesRead);

            if (status >= 0 && baseDllName.Buffer && baseDllName.Length > 0) {
                wchar_t* wBuf = new wchar_t[baseDllName.Length / sizeof(wchar_t) + 1]();
                status = NtReadVirtualMemory(hProcess, baseDllName.Buffer, wBuf, baseDllName.Length, &bytesRead);
                if (status >= 0) {
                    // Use standard std::cout for the address hex output, and wprintf/wcout conversion for the wide string name
                    std::cout << "0x" << std::hex << std::setw(16) << std::setfill('0') << (uintptr_t)dllBase 
                              << std::dec << " | ";
                    std::wcout << wBuf << "\n";
                }
                delete[] wBuf;
            }

            // Read next link
            LIST_ENTRY currentLinkStruct;
            status = NtReadVirtualMemory(hProcess, pCurrentLink, &currentLinkStruct, sizeof(currentLinkStruct), &bytesRead);
            if (status < 0) break;
            pCurrentLink = currentLinkStruct.Flink;
            safetyCounter++;
        }
        std::cout << "------------------------------------------------------------------------\n\n";
        return true;
    }

bool PrintProcessHeaps(HANDLE hProcess) {
        HMODULE hNtdll = GetNtdllHandle();
        if (!hNtdll) {
            std::cerr << "[-] Failed to get handle for ntdll.dll\n";
            return false;
        }

        pfnNtQueryInformationProcess NtQueryInformationProcess = 
            (pfnNtQueryInformationProcess)FindExport(hNtdll, "NtQueryInformationProcess");
        pfnNtReadVirtualMemory NtReadVirtualMemory = 
            (pfnNtReadVirtualMemory)FindExport(hNtdll, "NtReadVirtualMemory");

        if (!NtQueryInformationProcess || !NtReadVirtualMemory) {
            std::cerr << "[-] Failed to resolve NT internal functions from ntdll.\n";
            return false;
        }

        PROCESS_BASIC_INFORMATION pbi;
        ULONG returnLength = 0;
        NTSTATUS status = NtQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), &returnLength);
        if (status < 0 || !pbi.PebBaseAddress) {
            std::cerr << "[-] NtQueryInformationProcess failed or returned null PEB. Status: 0x" 
                      << std::hex << (unsigned long)status << std::dec << "\n";
            return false;
        }

        // PEB Offsets for x64 Windows (Standard Win10/Win11 layout)
#if defined(_WIN64)
        uintptr_t numHeapsOffset = 0x118;     
        uintptr_t processHeapsOffset = 0x120; 
#else
        uintptr_t numHeapsOffset = 0x88;
        uintptr_t processHeapsOffset = 0x90;
#endif

        ULONG numberOfHeaps = 0;
        SIZE_T bytesRead = 0;

        // Read NumberOfHeaps from PEB
        status = NtReadVirtualMemory(hProcess, (PVOID)((uintptr_t)pbi.PebBaseAddress + numHeapsOffset), &numberOfHeaps, sizeof(numberOfHeaps), &bytesRead);
        if (status < 0) {
            std::cerr << "[-] Failed to read NumberOfHeaps from PEB offset 0x" << std::hex << numHeapsOffset 
                      << std::dec << ". Status: 0x" << std::hex << (unsigned long)status << std::dec << "\n";
            return false;
        }

        if (numberOfHeaps == 0) {
            std::cout << "[!] NumberOfHeaps reported as 0 in remote PEB.\n";
            return false;
        }

        // Read ProcessHeaps pointer array address from PEB
        PVOID* processHeapsArray = nullptr;
        status = NtReadVirtualMemory(hProcess, (PVOID)((uintptr_t)pbi.PebBaseAddress + processHeapsOffset), &processHeapsArray, sizeof(processHeapsArray), &bytesRead);
        if (status < 0 || !processHeapsArray) {
            std::cerr << "[-] Failed to read ProcessHeaps array pointer from PEB offset 0x" << std::hex << processHeapsOffset 
                      << std::dec << ". Status: 0x" << std::hex << (unsigned long)status << std::dec << "\n";
            return false;
        }

        // SANity Check: Ensure the pointer is in valid user-space range (not garbage)
        if ((uintptr_t)processHeapsArray < 0x10000 || (uintptr_t)processHeapsArray > 0x00007FFFFFFFFFFF) {
            std::cerr << "[-] Invalid ProcessHeaps pointer address read from PEB: 0x" 
                      << std::hex << (uintptr_t)processHeapsArray << std::dec 
                      << " (Check your PEB offsets 0x" << std::hex << numHeapsOffset << "/" << processHeapsOffset << ")\n";
            return false;
        }

        // Allocate local buffer to hold the heap handles array
        PVOID* localHeaps = new PVOID[numberOfHeaps];
        status = NtReadVirtualMemory(hProcess, processHeapsArray, localHeaps, numberOfHeaps * sizeof(PVOID), &bytesRead);
        if (status < 0) {
            std::cerr << "[-] Failed to read remote process heap handles array content. Status: 0x" 
                      << std::hex << (unsigned long)status << std::dec << "\n";
            delete[] localHeaps;
            return false;
        }

        std::cout << "\n[+] Initialized Heaps for Target Process (Total: " << numberOfHeaps << "):\n";
        std::cout << "--------------------------------------------------------\n";
        std::cout << std::left << std::setw(20) << "Heap Handle" << " | " << "Index" << "\n";
        std::cout << "--------------------------------------------------------\n";

        for (ULONG i = 0; i < numberOfHeaps; i++) {
            std::cout << "0x" << std::hex << std::setw(16) << std::setfill('0') << (uintptr_t)localHeaps[i] 
                      << std::dec << " | Heap #" << i << "\n";
        }
        std::cout << "--------------------------------------------------------\n\n";

        delete[] localHeaps;
        return true;
    } // PrintProcessHeaps

bool ReadMemoryRangeFloats(HANDLE hProcess, uintptr_t address, int count, const std::string& outFilename) {
        SIZE_T stride = 32; // 32 bytes (0x20 hex) per chunk
        SIZE_T bytesToRead = count * stride;
        unsigned char* buffer = new unsigned char[bytesToRead]();
        SIZE_T bytesRead = 0;

        std::cout << "[*] Reading " << count << " float values (" << stride << " bytes apart) from 0x" << std::hex << address << std::dec << "...\n";
        
        if (!ReadMemoryRange(hProcess, address, address + bytesToRead, buffer, bytesToRead, &bytesRead)) {
            std::cerr << "[-] Failed to read memory for floating-point conversion.\n";
            delete[] buffer;
            return false;
        }

        std::ostringstream oss;
        oss << "[+] 32-bit Float Interpretations (Base Address: 0x" << std::hex << address << std::dec << "):\n";
        oss << "--------------------------------------------------\n";
        oss << std::left << std::setw(18) << "Address" 
            << " | " << std::setw(15) << "Float (32-bit)" << "\n";
        oss << "--------------------------------------------------\n";

        for (int i = 0; i < count; i++) {
            // Ensure strict additive stride without any shifting or scaling
            uintptr_t currentAddr = address + (i * stride);
            size_t offset = i * stride;

            if (offset + sizeof(float) <= bytesRead) {
                float fVal = *reinterpret_cast<float*>(buffer + offset);

                oss << "  0x" << std::hex << currentAddr << std::dec << " | " << std::setw(15) << std::fixed << std::setprecision(4) << fVal << "\n";
            }
        }
        oss << "--------------------------------------------------\n";

        //\ MUTING std::cout << oss.str() << "\n";

        std::string targetFile = outFilename;
        if (targetFile.empty()) {
            std::ostringstream nameBuilder;
            nameBuilder << "dump/floats_0x" << std::hex << address << ".txt";
            targetFile = nameBuilder.str();
        } else {
            targetFile = "dump/" + targetFile;
        }

        std::filesystem::create_directories("dump");

        std::ofstream outFile(targetFile, std::ios::out);
        if (outFile.is_open()) {
            outFile << oss.str();
            outFile.close();
            std::cout << "[+] Successfully saved float dump to " << targetFile << "\n\n";
        } else {
            std::cerr << "[-] Warning: Failed to write dump file to '" << targetFile << "'.\n\n";
        }

        delete[] buffer;
        return true;
    } //\ ReadMemoryRangeFloats

    bool PrintProcessMemoryMap(HANDLE hProcess, const std::string& outFilename) {
        if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
            std::cerr << "[-] Invalid process handle passed to PrintProcessMemoryMap.\n";
            return false;
        }

        // Resolve NtQueryVirtualMemory dynamically from ntdll.dll
        HMODULE hNtdll = GetNtdllHandle();
        auto NtQueryVirtualMemory = reinterpret_cast<pfnNtQueryVirtualMemory>(
            FindExport(hNtdll, "NtQueryVirtualMemory")
        );

        if (!NtQueryVirtualMemory) {
            std::cerr << "[-] Failed to resolve NtQueryVirtualMemory from ntdll.dll.\n";
            return false;
        }

        uintptr_t currentAddress = 0;
        MEMORY_BASIC_INFORMATION mbi;

        // Use string stream to capture the entire table output for both console and file writing
        std::ostringstream oss;
        oss << "\n[+] Generating Memory Map via NtQueryVirtualMemory...\n";
        oss << "------------------------------------------------------------------------------------\n";
        oss << std::left << std::setw(18) << "Base Address" 
                << " | " << std::setw(14) << "Region Size" 
                << " | " << std::setw(12) << "State" 
                << " | " << std::setw(12) << "Protection" 
                << " | " << std::setw(10) << "Type" << "\n";
        oss << "------------------------------------------------------------------------------------\n";

        // Traverse the 64-bit user-space address space
        while (true) {
            SIZE_T returnLength = 0;
            NTSTATUS status = NtQueryVirtualMemory(
                hProcess,
                reinterpret_cast<PVOID>(currentAddress),
                0, // MemoryBasicInformation
                &mbi,
                sizeof(mbi),
                &returnLength
            );

            if (status != STATUS_SUCCESS) {
                break; // Reached end of address space or failed query
            }

            // Filter for committed memory regions (active allocations)
            if (mbi.State == MEM_COMMIT) {
                std::string stateStr = "Commit";
                std::string protStr = "";
                std::string typeStr = "";

                // Decode protection flags (strip modifiers like PAGE_GUARD)
                DWORD prot = mbi.Protect & 0xFF; 
                if (prot == PAGE_READONLY) protStr = "READONLY";
                else if (prot == PAGE_READWRITE) protStr = "READWRITE";
                else if (prot == PAGE_EXECUTE_READ) protStr = "EXEC_READ";
                else if (prot == PAGE_EXECUTE_READWRITE) protStr = "EXEC_RW";
                else if (prot == PAGE_EXECUTE) protStr = "EXECUTE";
                else protStr = "PROTECT_0x" + std::to_string(prot);

                // Decode allocation type
                if (mbi.Type == MEM_PRIVATE) typeStr = "Private";
                else if (mbi.Type == MEM_IMAGE) typeStr = "Image";
                else if (mbi.Type == MEM_MAPPED) typeStr = "Mapped";
                else typeStr = "Unknown";

                oss << "  0x" << std::hex << std::setw(14) << std::setfill('0') << reinterpret_cast<uintptr_t>(mbi.BaseAddress) << std::dec
                    << " | " << std::setw(10) << (mbi.RegionSize / 1024) << " KB"
                    << " | " << std::setw(12) << stateStr
                    << " | " << std::setw(12) << protStr
                    << " | " << std::setw(10) << typeStr << "\n";
            }

            // Calculate next region address safely to prevent overflow
            uintptr_t nextAddress = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (nextAddress <= currentAddress) {
                break; 
            }
            currentAddress = nextAddress;

            // Stop at user-space / kernel-space boundary on x64 Windows
            if (currentAddress >= 0x00007FFFFFFFFFFF) {
                break;
            }
        }

        oss << "------------------------------------------------------------------------------------\n";
        oss << "[+] Memory map generation completed.\n\n";

        // Print to console standard output
        //\ MUTING std::cout << oss.str();

        // Determine target file path (auto-generate name if empty)
        std::string targetFile = outFilename;
        if (targetFile.empty()) {
            targetFile = "dump/memory_map.txt";
        } else {
            targetFile = "dump/" + targetFile;
        }

        // Ensure dump directory exists and save to file
        std::filesystem::create_directories("dump");

        std::ofstream outFile(targetFile, std::ios::out);
        if (outFile.is_open()) {
            outFile << oss.str();
            outFile.close();
            std::cout << "[+] Successfully saved memory map to " << targetFile << "\n\n";
        } else {
            std::cerr << "[-] Warning: Failed to write memory map file to '" << targetFile << "'.\n\n";
        }

        return true;
    } //\ PrintProcessMemoryMap



    void FloatMonitorWorker(HANDLE hProcess, DWORD processId, std::string ptyTarget, uintptr_t startAddress, uintptr_t endAddress, int intervalMs, SIZE_T stride) {
        std::string ptyPath = ptyTarget;
        if (ptyPath.find("/dev/") != 0) {
            ptyPath = "/dev/pty" + ptyTarget;
        }

        std::string shellCmd = "sh -c \"cat > " + ptyPath + "\"";
        FILE* ptyOut = _popen(shellCmd.c_str(), "wb");
        if (!ptyOut) {
            std::cerr << "[-] Failed to open shell stream to '" << ptyPath << "'.\n";
            g_isFloatMonitoring = false;
            return;
        }

        SIZE_T totalBytes = endAddress - startAddress;
        int count = static_cast<int>(totalBytes / stride);
        if (count <= 0) count = 1;

        // Dynamically choose 1, 2, or 3 side-by-side columns based on item count
        int numCols = 1;
        if (count > 60) {
            numCols = 3;
        } else if (count > 25) {
            numCols = 2;
        }

        int numRows = (count + numCols - 1) / numCols;

        std::cout << "\n[+] Float Monitor streaming (" << numCols << "-col layout) to '" << ptyPath 
                  << "' (0x" << std::hex << startAddress << " -> 0x" << endAddress << std::dec << ").\n" << std::flush;

        while (g_isFloatMonitoring) {
            std::vector<unsigned char> buffer(totalBytes, 0);
            SIZE_T bytesRead = 0;

            MEMORY_BASIC_INFORMATION mbi;
            bool pageValid = false;
            if (VirtualQueryEx(hProcess, (LPCVOID)startAddress, &mbi, sizeof(mbi)) == sizeof(mbi)) {
                if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                    pageValid = true;
                }
            }

            bool readOk = pageValid && ReadProcessMemory(hProcess, (LPCVOID)startAddress, buffer.data(), totalBytes, &bytesRead);

            // Helper lambda to format a single cell cleanly to a fixed width (29 chars)
            auto formatCell = [&](int idx) -> std::string {
                if (idx >= count) {
                    return std::string(29, ' ');
                }

                uintptr_t currentAddr = startAddress + (idx * stride);
                size_t offset = idx * stride;

                std::ostringstream cell;
                cell << "0x" << std::hex << std::setw(12) << std::setfill('0') << currentAddr
                     << std::dec << std::setfill(' ') << " | ";

                if (readOk && offset + sizeof(float) <= bytesRead) {
                    float fVal = *reinterpret_cast<float*>(buffer.data() + offset);
                    cell << std::setw(13) << std::fixed << std::setprecision(4) << fVal;
                } else {
                    cell << std::setw(13) << "--------";
                }
                return cell.str();
            };

            std::string divider(numCols * 33 - 4, '-');

            std::ostringstream oss;
            oss << "\033[2J\033[H";
            oss << "[+] Float Monitor -> " << ptyPath << " (PID: " << processId 
                << " | 0x" << std::hex << startAddress << " - 0x" << endAddress << std::dec 
                << " | Items: " << count << " | Cols: " << numCols << ")\n";
            oss << divider << "\n";

            // Header row
            for (int c = 0; c < numCols; ++c) {
                oss << std::left << std::setw(14) << "Address"
                    << " | " << std::setw(13) << "Float";
                if (c < numCols - 1) oss << " || ";
            }
            oss << "\n" << divider << "\n";

            // Render rows in column-major order
            for (int r = 0; r < numRows; ++r) {
                for (int c = 0; c < numCols; ++c) {
                    int idx = c * numRows + r;
                    oss << formatCell(idx);
                    if (c < numCols - 1) {
                        oss << " || ";
                    }
                }
                oss << "\n";
            }
            oss << divider << "\n";

            std::string outStr = oss.str();
            if (fwrite(outStr.data(), 1, outStr.size(), ptyOut) == 0 || fflush(ptyOut) != 0) {
                std::cerr << "\n[-] Lost connection to '" << ptyPath << "' (terminal closed).\n";
                break;
            }

            Sleep(intervalMs);
        }

        _pclose(ptyOut);
        g_isFloatMonitoring = false;
        std::cout << "\n[+] Float monitor on '" << ptyPath << "' stopped.\n" << std::flush;
    } //\ FloatMonitorWorker

    DWORD WINAPI FloatMonitorThreadProc(LPVOID lpParam) {
        std::unique_ptr<FloatMonitorParams> params(static_cast<FloatMonitorParams*>(lpParam));
        FloatMonitorWorker(
            params->hProcess,
            params->processId,
            params->ptyTarget,
            params->startAddress,
            params->endAddress,
            params->intervalMs,
            params->stride
        );
        return 0;
    } //\ FloatMonitorThreadProc

    bool StartFloatMonitor(HANDLE hProcess, DWORD processId, const std::string& ptyTarget, uintptr_t startAddress, uintptr_t endAddress, int intervalMs, SIZE_T stride) {
        if (g_isFloatMonitoring) {
            std::cout << "[-] Float monitor is already running. Type 'stopfloats' first.\n";
            return false;
        }

        if (stride == 0) {
            stride = sizeof(float);
        }

        // Clean up any finished handle from a previous run
        if (g_hFloatMonitorThread != NULL) {
            CloseHandle(g_hFloatMonitorThread);
            g_hFloatMonitorThread = NULL;
        }

        auto params = std::make_unique<FloatMonitorParams>(FloatMonitorParams{
            hProcess,
            processId,
            ptyTarget,
            startAddress,
            endAddress,
            intervalMs,
            stride
        });

        g_isFloatMonitoring = true;
        g_hFloatMonitorThread = CreateThread(nullptr, 0, FloatMonitorThreadProc, params.get(), 0, nullptr);

        if (g_hFloatMonitorThread == NULL) {
            g_isFloatMonitoring = false;
            std::cerr << "[-] Failed to start float monitor thread. Error: " << GetLastError() << "\n";
            return false;
        }

        // Thread now owns the params pointer
        params.release();
        return true;
    } //\ StartFloatMonitor

    void StopFloatMonitor() {
        if (!g_isFloatMonitoring) {
            std::cout << "[-] No active float monitor running.\n";
            return;
        }

        g_isFloatMonitoring = false;

        if (g_hFloatMonitorThread != NULL) {
            WaitForSingleObject(g_hFloatMonitorThread, 2000);
            CloseHandle(g_hFloatMonitorThread);
            g_hFloatMonitorThread = NULL;
        }

        std::cout << "\n[+] Float monitor successfully stopped.\n";
    } //\ StopFloatMonitor

    struct FloatWorkerParams {
        HANDLE hProcess;
        DWORD processId;
        std::string ptyTarget;
        uintptr_t startAddress;
        uintptr_t endAddress;
        int intervalMs;
        SIZE_T stride;
    }; //\ FloatWorkerParams

    void FloatFilterWorker(HANDLE hProcess, DWORD processId, std::string ptyTarget, uintptr_t startAddress, uintptr_t endAddress, int intervalMs, SIZE_T stride) {
        std::string ptyPath = ptyTarget;
        if (ptyPath.find("/dev/") != 0) {
            ptyPath = "/dev/pty" + ptyTarget;
        }

        std::string shellCmd = "sh -c \"cat > " + ptyPath + "\"";
        FILE* ptyOut = _popen(shellCmd.c_str(), "wb");
        if (!ptyOut) {
            std::cerr << "[-] Failed to open shell stream to '" << ptyPath << "'.\n";
            g_isFloatFiltering = false;
            return;
        }

        ResolverConfig cfg;
        cfg.minStepDelta      = 0.001f;
        cfg.maxStepDelta      = 250.0f;
        cfg.maxRadius         = 5000.0f;
        cfg.maxStaticTicks    = 20;
        cfg.maxHistorySamples = 0;

        Resolver resolver;
        resolver.Start(startAddress, endAddress, stride, cfg);

        SIZE_T totalBytes = endAddress - startAddress;
        int zeroCount = 0; // Tracks consecutive ticks with 0 surviving candidates

        std::cout << "\n[+] Float Filter streaming directly to '" << ptyPath 
                  << "' (0x" << std::hex << startAddress << " -> 0x" << endAddress << std::dec << ").\n" << std::flush;

        while (g_isFloatFiltering) {
            std::vector<unsigned char> buffer(totalBytes, 0);
            SIZE_T bytesRead = 0;

            MEMORY_BASIC_INFORMATION mbi;
            bool pageValid = false;
            if (VirtualQueryEx(hProcess, (LPCVOID)startAddress, &mbi, sizeof(mbi)) == sizeof(mbi)) {
                if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                    pageValid = true;
                }
            }

            if (pageValid && ReadProcessMemory(hProcess, (LPCVOID)startAddress, buffer.data(), totalBytes, &bytesRead)) {
                resolver.PushSample(startAddress, buffer, bytesRead);
            }

            auto survivors = resolver.GetSurvivingCandidates();
            int count = static_cast<int>(survivors.size());

            // Auto-rescan / reset resolver if 0 entries persist for 30 ticks (~10-50 range)
            if (count == 0) {
                zeroCount++;
                if (zeroCount >= 10) {
                    resolver.Stop();
                    resolver.Start(startAddress, endAddress, stride, cfg);
                    zeroCount = 0;
                }
            } else {
                zeroCount = 0; // Reset counter whenever survivors are active
            }

            // Dynamically adjust columns (1, 2, or 3) as candidates get pruned over time
            int numCols = 1;
            if (count > 60) {
                numCols = 3;
            } else if (count > 25) {
                numCols = 2;
            }

            int numRows = (count > 0) ? ((count + numCols - 1) / numCols) : 0;

            // Each cell is exactly 56 chars wide:
            // "0x01ff3002a6d0 |      104.2500 |     12.5000 |   45.0000"
            const int kCellWidth = 56;
            auto formatCell = [&](int idx) -> std::string {
                if (idx >= count) {
                    return std::string(kCellWidth, ' ');
                }

                const auto& item = survivors[idx];
                float deltaOrigin = std::fabs(item.lastValue - item.initialValue);

                std::ostringstream cell;
                cell << "0x" << std::hex << std::setw(12) << std::setfill('0') << item.address
                     << std::dec << std::setfill(' ') << " | "
                     << std::setw(11) << std::fixed << std::setprecision(4) << item.lastValue << " | "
                     << std::setw(11) << std::fixed << std::setprecision(4) << deltaOrigin << " | "
                     << std::setw(11) << std::fixed << std::setprecision(4) << item.totalDistance;
                return cell.str();
            };

            std::string divider(numCols * (kCellWidth + 4) - 4, '-');

            std::ostringstream oss;
            oss << "\033[2J\033[H";
            oss << "[+] Float Filter -> " << ptyPath << " (PID: " << processId 
                << " | Surviving: " << count 
                << " | Ticks: " << resolver.GetTotalProcessedSamples()
                << " | Cols: " << numCols << ")\n";
            oss << divider << "\n";

            // Header row
            for (int c = 0; c < numCols; ++c) {
                oss << std::left << std::setw(14) << "Address"
                    << " | " << std::setw(11) << "Current"
                    << " | " << std::setw(11) << "DeltaOrigin"
                    << " | " << std::setw(11) << "PathDist";
                if (c < numCols - 1) oss << " || ";
            }
            oss << "\n" << divider << "\n";

            if (count == 0) {
                oss << "  (No active candidates remaining - auto-rescan pending [" << zeroCount << "/30])\n";
            } else {
                // Render rows in column-major order
                for (int r = 0; r < numRows; ++r) {
                    for (int c = 0; c < numCols; ++c) {
                        int idx = c * numRows + r;
                        oss << formatCell(idx);
                        if (c < numCols - 1) {
                            oss << " || ";
                        }
                    }
                    oss << "\n";
                }
            }
            oss << divider << "\n";

            std::string outStr = oss.str();
            if (fwrite(outStr.data(), 1, outStr.size(), ptyOut) == 0 || fflush(ptyOut) != 0) {
                std::cerr << "\n[-] Lost connection to '" << ptyPath << "' (terminal closed).\n";
                break;
            }

            Sleep(intervalMs);
        }

        resolver.Stop();
        _pclose(ptyOut);
        g_isFloatFiltering = false;
        std::cout << "\n[+] Float filter on '" << ptyPath << "' stopped.\n" << std::flush;
    } //\ FloatFilterWorker

    DWORD WINAPI FloatFilterThreadProc(LPVOID lpParam) {
        FloatWorkerParams* params = static_cast<FloatWorkerParams*>(lpParam);
        if (params) {
            FloatFilterWorker(
                params->hProcess,
                params->processId,
                params->ptyTarget,
                params->startAddress,
                params->endAddress,
                params->intervalMs,
                params->stride
            );
            delete params;
        }
        return 0;
    } //\ FloatFilterThreadProc

    bool StartFloatFilter(HANDLE hProcess, DWORD processId, const std::string& ptyTarget, uintptr_t startAddress, uintptr_t endAddress, int intervalMs, SIZE_T stride) {
        if (g_isFloatFiltering) {
            std::cout << "[-] Float filter is already running. Type 'stopfilter' first.\n";
            return false;
        }

        if (stride == 0) stride = sizeof(float);
        auto* params = new FloatWorkerParams{hProcess, processId, ptyTarget, startAddress, endAddress, intervalMs, stride};

        g_isFloatFiltering = true;
        g_hFloatFilterThread = CreateThread(nullptr, 0, FloatFilterThreadProc, params, 0, nullptr);
        if (!g_hFloatFilterThread) {
            g_isFloatFiltering = false;
            delete params;
            std::cerr << "[-] Failed to spawn FloatFilter thread. Error: " << GetLastError() << "\n";
            return false;
        }
        return true;
    } //\ StartFloatFilter

    void StopFloatFilter() {
        if (!g_isFloatFiltering) {
            std::cout << "[-] No active float filter running.\n";
            return;
        }

        g_isFloatFiltering = false;
        if (g_hFloatFilterThread) {
            WaitForSingleObject(g_hFloatFilterThread, 2000);
            CloseHandle(g_hFloatFilterThread);
            g_hFloatFilterThread = NULL;
        }
    } //\ StopFloatFilter
    
    struct FloatScanMatch {
        uintptr_t address;
        float value;
    }; //\ FloatScanMatch

    struct FloatScanParams {
        HANDLE hProcess;
        DWORD processId;
        float minVal;
        float maxVal;
        std::string outFile;
        SIZE_T stride;
    };

    std::atomic<bool> g_isFloatScanning{false};
    HANDLE g_hFloatScanThread = NULL;

    void FloatScanWorker(HANDLE hProcess, DWORD processId, float minVal, float maxVal, std::string outFile, SIZE_T stride) {
        const uintptr_t kMinUserAddr = 0x000000000000ULL;
        const uintptr_t kMaxUserAddr = 0x7FFFFFFFFFFFULL;

        if (stride == 0) stride = sizeof(float);
        if (minVal > maxVal) std::swap(minVal, maxVal);

        std::cout << "\n[*] Starting 3-Pass Float Scan on PID " << processId
                  << " for range [" << minVal << ", " << maxVal << "] (1-second delay between passes)...\n" << std::flush;

        struct ScanCandidate {
            uintptr_t address;
            float firstVal;
            float lastVal;
            bool changed;
        };

        std::vector<ScanCandidate> candidates;
        candidates.reserve(8192);

        // =====================================================================
        // PASS 1: Full user-space memory scan
        // =====================================================================
        std::cout << "[*] Pass 1/3: Scanning full virtual address space [0x0 -> 0x7FFFFFFFFFFF]...\n" << std::flush;

        uintptr_t currentAddr = kMinUserAddr;
        MEMORY_BASIC_INFORMATION mbi{};
        SIZE_T regionsScanned = 0;
        SIZE_T bytesScanned = 0;

        const SIZE_T kMaxChunkSize = 4 * 1024 * 1024; // 4 MB chunks
        std::vector<unsigned char> buffer(kMaxChunkSize);

        while (g_isFloatScanning && currentAddr < kMaxUserAddr) {
            if (VirtualQueryEx(hProcess, reinterpret_cast<LPCVOID>(currentAddr), &mbi, sizeof(mbi)) != sizeof(mbi)) {
                break;
            }

            uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            SIZE_T regionSize = mbi.RegionSize;
            uintptr_t regionEnd = regionBase + regionSize;

            if (regionEnd <= regionBase || regionEnd > kMaxUserAddr) {
                regionEnd = kMaxUserAddr;
            }

            bool isReadable = (mbi.State == MEM_COMMIT) &&
                              !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                              (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                              PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));

            if (isReadable && regionEnd > regionBase) {
                regionsScanned++;
                uintptr_t chunkAddr = regionBase;

                while (g_isFloatScanning && chunkAddr < regionEnd) {
                    SIZE_T toRead = static_cast<SIZE_T>(regionEnd - chunkAddr);
                    if (toRead > kMaxChunkSize) toRead = kMaxChunkSize;

                    SIZE_T bytesRead = 0;
                    if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(chunkAddr), buffer.data(), toRead, &bytesRead) && bytesRead >= sizeof(float)) {
                        bytesScanned += bytesRead;

                        for (SIZE_T offset = 0; offset + sizeof(float) <= bytesRead; offset += stride) {
                            float val = *reinterpret_cast<const float*>(buffer.data() + offset);
                            if (std::isfinite(val) && val >= minVal && val <= maxVal) {
                                candidates.push_back(ScanCandidate{ chunkAddr + offset, val, val, false });
                            }
                        }
                    }
                    chunkAddr += toRead;
                }
            }

            if (regionEnd >= kMaxUserAddr) break;
            currentAddr = regionEnd;
        }

        if (!g_isFloatScanning) {
            std::cout << "[-] Scan aborted during Pass 1.\n";
            return;
        }

        std::cout << "[+] Pass 1 complete: Found " << candidates.size() << " initial candidates.\n" << std::flush;

        // =====================================================================
        // PASS 2: First targeted re-read (1 second delay)
        // =====================================================================
        if (!candidates.empty() && g_isFloatScanning) {
            std::cout << "[*] Pass 2/3: Waiting 1 second before re-evaluating candidates...\n" << std::flush;
            Sleep(1000); // 1 full second

            for (auto& c : candidates) {
                if (!g_isFloatScanning) break;
                float val = 0.0f;
                SIZE_T bytesRead = 0;
                if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(c.address), &val, sizeof(float), &bytesRead) && bytesRead == sizeof(float)) {
                    if (std::isfinite(val)) {
                        if (val != c.lastVal) {
                            c.changed = true;
                        }
                        c.lastVal = val;
                    } else {
                        c.lastVal = NAN;
                    }
                } else {
                    c.lastVal = NAN;
                }
            }
        }

        // =====================================================================
        // PASS 3: Final validation re-read (1 second delay)
        // =====================================================================
        if (!candidates.empty() && g_isFloatScanning) {
            std::cout << "[*] Pass 3/3: Waiting 1 second for final validation pass...\n" << std::flush;
            Sleep(1000); // 1 full second

            for (auto& c : candidates) {
                if (!g_isFloatScanning) break;
                float val = 0.0f;
                SIZE_T bytesRead = 0;
                if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(c.address), &val, sizeof(float), &bytesRead) && bytesRead == sizeof(float)) {
                    if (std::isfinite(val)) {
                        if (val != c.lastVal) {
                            c.changed = true;
                        }
                        c.lastVal = val;
                    } else {
                        c.lastVal = NAN;
                    }
                } else {
                    c.lastVal = NAN;
                }
            }
        }

        if (!g_isFloatScanning) {
            std::cout << "[-] Scan aborted.\n";
            return;
        }

        // Filter out everything that didn't mutate across the 3 passes
        std::vector<FloatScanMatch> finalMatches;
        finalMatches.reserve(candidates.size());

        for (const auto& c : candidates) {
            if (!std::isfinite(c.lastVal)) continue;

            if (c.changed || c.lastVal != c.firstVal) {
                finalMatches.push_back(FloatScanMatch{ c.address, c.lastVal });
            }
        }

        std::cout << "\n[+] 3-Pass Scan complete! Scanned " << regionsScanned << " regions ("
                  << (bytesScanned / (1024 * 1024)) << " MB). Dynamic matches retained: " 
                  << finalMatches.size() << " (discarded " << (candidates.size() - finalMatches.size()) << " static values).\n";

        // =====================================================================
        // Export Results to dump/ (Dynamic 1 to 4 Column Layout)
        // =====================================================================
        CreateDirectoryA("dump", nullptr);

        std::string targetFile = outFile.empty()
            ? ("scan_" + std::to_string(processId) + ".txt")
            : outFile;

        if (targetFile.find("dump/") != 0 && targetFile.find("dump\\") != 0) {
            targetFile = "dump/" + targetFile;
        }

        std::ofstream ofs(targetFile, std::ios::out | std::ios::trunc);
        if (ofs.is_open()) {
            int count = static_cast<int>(finalMatches.size());

            // Dynamically choose 1 to 4 columns based on match count
            int numCols = 1;
            if (count > 120) {
                numCols = 4;
            } else if (count > 60) {
                numCols = 3;
            } else if (count > 30) {
                numCols = 2;
            }

            int numRows = (count > 0) ? ((count + numCols - 1) / numCols) : 0;

            // Cell width: "0x01ff3002a6d0 |   104.250000" -> 14 + 3 + 13 = 30 chars
            const int kCellWidth = 30;
            auto formatCell = [&](int idx) -> std::string {
                if (idx >= count) {
                    return std::string(kCellWidth, ' ');
                }
                const auto& m = finalMatches[idx];
                std::ostringstream cell;
                cell << "0x" << std::hex << std::setw(12) << std::setfill('0') << m.address
                     << std::dec << std::setfill(' ') << " | "
                     << std::setw(13) << std::fixed << std::setprecision(6) << m.value;
                return cell.str();
            };

            std::string divider(numCols * (kCellWidth + 4) - 4, '-');

            ofs << "[+] 3-Pass Float Scan Results (PID: " << processId 
                << " | Range: [" << minVal << ", " << maxVal << "]"
                << " | Dynamic Matches: " << count
                << " | Cols: " << numCols << ")\n";
            ofs << divider << "\n";

            if (count == 0) {
                ofs << "  (No dynamic matches found)\n";
            } else {
                // Header row
                for (int c = 0; c < numCols; ++c) {
                    ofs << std::left << std::setw(14) << "Address"
                        << " | " << std::setw(13) << "Float";
                    if (c < numCols - 1) ofs << " || ";
                }
                ofs << "\n" << divider << "\n";

                // Column-major layout (top-to-bottom per column)
                for (int r = 0; r < numRows; ++r) {
                    for (int c = 0; c < numCols; ++c) {
                        int idx = c * numRows + r;
                        ofs << formatCell(idx);
                        if (c < numCols - 1) {
                            ofs << " || ";
                        }
                    }
                    ofs << "\n";
                }
            }
            ofs << divider << "\n";
            ofs.close();

            std::cout << "[+] Saved all " << finalMatches.size() << " dynamic results (" << numCols << "-col layout) to '" << targetFile << "'.\n";
        } else {
            std::cerr << "[-] Failed to open output file '" << targetFile << "'.\n";
        }

        std::cout << std::flush;
        g_isFloatScanning = false;
    } //\ FloatScanWorker

    DWORD WINAPI FloatScanThreadProc(LPVOID lpParam) {
        FloatScanParams* params = static_cast<FloatScanParams*>(lpParam);
        if (params) {
            FloatScanWorker(
                params->hProcess,
                params->processId,
                params->minVal,
                params->maxVal,
                params->outFile,
                params->stride
            );
            delete params;
        }
        return 0;
    } //\ FloatScanThreadProc

    bool StartFloatScan(HANDLE hProcess, DWORD processId, float minVal, float maxVal, const std::string& outFile, SIZE_T stride) {
        if (g_isFloatScanning) {
            std::cout << "[-] A float scan is already running. Type 'stopscan' to abort it.\n";
            return false;
        }

        if (stride == 0) stride = sizeof(float);
        auto* params = new FloatScanParams{hProcess, processId, minVal, maxVal, outFile, stride};

        g_isFloatScanning = true;
        g_hFloatScanThread = CreateThread(nullptr, 0, FloatScanThreadProc, params, 0, nullptr);
        if (!g_hFloatScanThread) {
            g_isFloatScanning = false;
            delete params;
            std::cerr << "[-] Failed to spawn FloatScan thread. Error: " << GetLastError() << "\n";
            return false;
        }

        // Detach handle lifecycle since scan self-terminates upon completion
        CloseHandle(g_hFloatScanThread);
        g_hFloatScanThread = NULL;
        return true;
    } //\ StartFloatScan

    void StopFloatScan() {
        if (!g_isFloatScanning) {
            std::cout << "[-] No active float scan running.\n";
            return;
        }
        g_isFloatScanning = false;
        std::cout << "[*] Aborting active float scan...\n";
    } //\ StopFloatScan





} //\ ProcessHelper