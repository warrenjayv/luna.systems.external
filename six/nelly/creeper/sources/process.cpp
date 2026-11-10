#include "process.h"
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <fstream>
#include <iomanip>

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

namespace ProcessHelper {

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

}