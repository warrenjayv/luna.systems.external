#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <cstdlib>
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

HANDLE SafeOpenProcessRead(DWORD processId) {
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

// Native memory range reading function wrapping NtReadVirtualMemory
bool ReadMemoryRangeNative(HANDLE hProcess, uintptr_t startAddress, uintptr_t finishAddress, unsigned char* outBuffer, SIZE_T bufferSize, SIZE_T* totalBytesRead) {
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

// New function to read virtual memory using NtReadVirtualMemory
bool ReadProcessMemoryNative(HANDLE hProcess, LPVOID baseAddress, LPVOID buffer, SIZE_T size, SIZE_T* bytesReadOut = nullptr) {
    HMODULE hNtdll = GetNtdllHandle();
    if (!hNtdll) return false;

    pfnNtReadVirtualMemory NtReadVirtualMemory = (pfnNtReadVirtualMemory)FindExport(hNtdll, "NtReadVirtualMemory");
    if (!NtReadVirtualMemory) return false;

    SIZE_T bytesRead = 0;
    NTSTATUS status = NtReadVirtualMemory(hProcess, baseAddress, buffer, size, &bytesRead);
    
    if (bytesReadOut) {
        *bytesReadOut = bytesRead;
    }

    // NT_SUCCESS check (status >= 0) and ensure we read some data
    return (status >= 0);
}

// Updated Hex Dump function writing output to a file named 'dump'
bool PrintHexDumpToFile(const unsigned char* buffer, SIZE_T size, uintptr_t baseAddress, const std::string& filename = "dump") {
    std::ofstream outFile(filename, std::ios::out | std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "[-] Failed to open file '" << filename << "' for writing.\n";
        return false;
    }

    for (SIZE_T i = 0; i < size; i += 16) {
        outFile << "  0x" << std::hex << std::setw(8) << std::setfill('0') << (baseAddress + i) << ": ";
        
        // Print hex bytes
        for (SIZE_T j = 0; j < 16; j++) {
            if (i + j < size) {
                outFile << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i + j] << " ";
            } else {
                outFile << "   ";
            }
        }

        outFile << " | ";

        // Print ASCII characters
        for (SIZE_T j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = buffer[i + j];
            outFile << (char)(std::isprint(c) ? c : '.');
        }
        outFile << std::dec << "\n";
    }

    outFile.close();
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "[!] Usage: " << argv[0] << " <Target PID>\n";
        return 1;
    }

    DWORD targetPid = static_cast<DWORD>(std::stoul(argv[1]));
    std::cout << "[*] Attempting to safely open PID " << targetPid << " for reading...\n";

    // Set default target address to 0x4000, or allow custom user input via argv[2]
    LPVOID targetAddress = (LPVOID)0x4000;
    if (argc >= 3) {
        targetAddress = (LPVOID)std::strtoull(argv[2], nullptr, 0);
    }

    // Default range: 0x4000 to 0x4100 (256 bytes)
    uintptr_t startAddress = 0x4000;
    uintptr_t finishAddress = 0x4100;

    if (argc >= 4) {
        startAddress = std::strtoull(argv[2], nullptr, 0);
        finishAddress = std::strtoull(argv[3], nullptr, 0);
    }

    if (finishAddress <= startAddress) {
        std::cerr << "[-] Error: Finish address must be greater than start address.\n";
        return 1;
    }

    HANDLE hProcess = SafeOpenProcessRead(targetPid);
    if (hProcess) {
        std::cout << "[+] Successfully opened process! Handle value: " << hProcess << "\n";

        // Attempt to read memory from the specified address
        const SIZE_T readSize = 16;
        unsigned char buffer[readSize] = { 0 };
        SIZE_T bytesRead = 0;

        std::cout << "[*] Reading " << readSize << " bytes from address " << targetAddress << "...\n";
        if (ReadProcessMemoryNative(hProcess, targetAddress, buffer, readSize, &bytesRead)) {
            std::cout << "[+] Successfully read " << bytesRead << " bytes:\n    ";
            for (SIZE_T i = 0; i < bytesRead; i++) {
                std::printf("%02X ", buffer[i]);
            }
            std::printf("\n");
        } else {
            std::cerr << "[-] Failed to read memory from " << targetAddress << " (address might be unmapped or inaccessible).\n";
        }

        std::cout << "[+] Process opened successfully. Reading range 0x" 
                  << std::hex << startAddress << " to 0x" << finishAddress << std::dec << "...\n";

        //\ read specified memory range and output to file
        SIZE_T rangeSize = finishAddress - startAddress;
        unsigned char* memoryBuffer = new unsigned char[rangeSize]();
        bytesRead = 0;

        if (ReadMemoryRangeNative(hProcess, startAddress, finishAddress, memoryBuffer, rangeSize, &bytesRead)) {
            std::cout << "[+] Successfully read " << bytesRead << " bytes.\n";
            
            if (PrintHexDumpToFile(memoryBuffer, bytesRead, startAddress, "dump")) {
                std::cout << "[+] Hex dump successfully written to file 'dump'.\n";
            }
        } else {
            std::cerr << "[-] Failed to read memory range.\n";
        }

        delete[] memoryBuffer;

        // Clean up the handle using NtClose from ntdll (avoiding kernel32 CloseHandle)
        HMODULE hNtdll = GetNtdllHandle();
        pfnNtClose NtClose = (pfnNtClose)FindExport(hNtdll, "NtClose");
        if (NtClose) {
            NtClose(hProcess);
            std::cout << "[*] Handle closed successfully via NtClose.\n";
        }
    } else {
        std::cerr << "[-] Failed to open process. Ensure the PID is correct and you have sufficient permissions.\n";
        return 1;
    }

    return 0;
}
