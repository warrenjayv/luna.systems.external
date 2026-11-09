#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <cstdlib>

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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "[!] Usage: " << argv[0] << " <Target PID>\n";
        return 1;
    }

    DWORD targetPid = static_cast<DWORD>(std::stoul(argv[1]));
    std::cout << "[*] Attempting to safely open PID " << targetPid << " for reading...\n";

    HANDLE hProcess = SafeOpenProcessRead(targetPid);
    if (hProcess) {
        std::cout << "[+] Successfully opened process! Handle value: " << hProcess << "\n";

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
