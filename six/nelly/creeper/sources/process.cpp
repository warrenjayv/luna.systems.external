#include "process.h"
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>

#include <thread>
#include <atomic>
#include <chrono>
#include <tlhelp32.h>
#include <windows.h>
#include <cstring>

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

// Global control flags for the background monitor worker
static std::atomic<bool> g_isMonitoring(false);
static std::thread g_monitorThread;

namespace ProcessHelper {

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
    void GenericMonitorWorker(DWORD processId, int ptyNum) {
        std::string ptyPath = "/dev/pts/" + std::to_string(ptyNum);
        std::ofstream ptyStream(ptyPath, std::ios::out);
        
        if (!ptyStream.is_open()) {
            std::cerr << "\n[-] Failed to open PTY target: " << ptyPath 
                      << " (Ensure the target terminal window is open and writable)\n";
            g_isMonitoring = false;
            return;
        }

        ptyStream << "[*] Floating Registers Monitor active for PID " << processId 
                  << " -> Streaming to PTY " << ptyNum << "\n" << std::flush;

        DWORD threadId = GetFirstThreadId(processId);
        if (!threadId) {
            ptyStream << "[-] Error: No active threads found for PID " << processId << "\n";
            g_isMonitoring = false;
            return;
        }

        while (g_isMonitoring) {
            HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
            if (hThread) {
                SuspendThread(hThread);

                CONTEXT ctx;
                std::memset(&ctx, 0, sizeof(CONTEXT));
                // CONTEXT_FLOATING_POINT includes the FPU state and XMM0-XMM15 on x64 Windows
                ctx.ContextFlags = CONTEXT_FLOATING_POINT;
                
                if (GetThreadContext(hThread, &ctx)) {
                    // Print header and XMM0 - XMM3 states cleanly on the same line using \r
                    ptyStream << "\r[PTY" << ptyNum << " | PID:" << processId << "] ";
                    
                    for (int i = 0; i < 4; i++) {
                        ULONGLONG lowVal = ctx.VectorRegister[i].Low;
                        ULONGLONG highVal = ctx.VectorRegister[i].High;
                        ptyStream << "XMM" << i << ":[0x" << std::hex << std::setfill('0') 
                                  << std::setw(16) << highVal << std::setw(16) << lowVal << std::dec << "] ";
                    }
                    ptyStream << std::flush;
                }

                ResumeThread(hThread);
                CloseHandle(hThread);
            }

            // ~4 FPS refresh rate to avoid choking the terminal output buffer
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        ptyStream << "\n[+] Floating register monitor on PTY " << ptyNum << " stopped.\n";
        ptyStream.close();
    }

    bool StartMonitorPty(HANDLE hProcess, DWORD processId, int ptyNum) {
        if (g_isMonitoring) {
            std::cout << "[-] A monitor task is already active. Type 'stop' first.\n";
            return false;
        }

        g_isMonitoring = true;
        // Launch background worker dedicated to floating registers
        g_monitorThread = std::thread(GenericMonitorWorker, processId, ptyNum);
        std::cout << "[+] Floating register monitor started on pty" << ptyNum << ". Type 'stop' to halt.\n";
        return true;
    }

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

        std::cout << oss.str() << "\n";

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

        std::cout << "\n[+] Generating Memory Map via NtQueryVirtualMemory...\n";
        std::cout << "------------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(18) << "Base Address" 
                  << " | " << std::setw(14) << "Region Size" 
                  << " | " << std::setw(12) << "State" 
                  << " | " << std::setw(12) << "Protection" 
                  << " | " << std::setw(10) << "Type" << "\n";
        std::cout << "------------------------------------------------------------------------------------\n";

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

                std::cout << "  0x" << std::hex << std::setw(14) << std::setfill('0') << reinterpret_cast<uintptr_t>(mbi.BaseAddress) << std::dec
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

        std::cout << "------------------------------------------------------------------------------------\n";
        std::cout << "[+] Memory map generation completed.\n\n";
        return true;
    } //\ PrintProcessMemoryMap

} //\ ProcessHelper