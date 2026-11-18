#include "conjure.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <optional>
#include <vector>

// Native NT Undocumented Definitions & Structures
typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000)

typedef LONG KPRIORITY;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef struct _SYSTEM_THREAD_INFORMATION {
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG         WaitTime;
    PVOID         StartAddress;
    CLIENT_ID     ClientId;
    KPRIORITY     Priority;
    LONG          BasePriority;
    ULONG         ContextSwitches;
    ULONG         ThreadState;
    ULONG         WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG           NextEntryOffset;
    ULONG           NumberOfThreads;
    LARGE_INTEGER   WorkingSetPrivate;
    ULONG           HardFaultCount;
    ULONG           NumberOfThreadsHighWatermark;
    ULONGLONG       CycleTime;
    LARGE_INTEGER   CreateTime;
    LARGE_INTEGER   UserTime;
    LARGE_INTEGER   KernelTime;
    UNICODE_STRING  ImageName;
    KPRIORITY       BasePriority;
    HANDLE          UniqueProcessId;
    HANDLE          InheritedFromUniqueProcessId;
    ULONG           HandleCount;
    ULONG           SessionId;
    ULONG_PTR       UniqueProcessKey;
    SIZE_T          PeakVirtualSize;
    SIZE_T          VirtualSize;
    ULONG           PageFaultCount;
    SIZE_T          PeakPageWriteCount;
    SIZE_T          PeakWorkingSetSize;
    SIZE_T          WorkingSetSize;
    SIZE_T          QuotaPeakPagedPoolUsage;
    SIZE_T          QuotaPagedPoolUsage;
    SIZE_T          QuotaPeakNonPagedPoolUsage;
    SIZE_T          QuotaNonPagedPoolUsage;
    SIZE_T          PageSize;
    SIZE_T          PageFileUsage;
    SIZE_T          PeakPagePeakPageFileUsage;
    SIZE_T          PrivatePageCount;
    LARGE_INTEGER   LireTime;
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef NTSTATUS(WINAPI* pfnNtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(WINAPI* pfnNtSuspendThread)(HANDLE, PULONG);
typedef NTSTATUS(WINAPI* pfnNtResumeThread)(HANDLE, PULONG);
typedef NTSTATUS(WINAPI* pfnNtGetContextThread)(HANDLE, PCONTEXT);

namespace Conjure {

    const std::string kRegistryFile = "config/conjure_registry.cfg";
    const std::string kRecordsFile  = "config/records";

    // ... (Keep LoadRegistry, SaveRange, GetRange unchanged) ...

    std::vector<RegisterMatch> FindRegisterOffset(HANDLE hProcess, DWORD processId, uintptr_t targetAddress, int maxOffset) {
        std::vector<RegisterMatch> results;
        (void)hProcess; // Unused since we read thread contexts via native NT APIs

        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return results;

        auto NtQuerySystemInformation = (pfnNtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
        auto NtSuspendThread          = (pfnNtSuspendThread)GetProcAddress(hNtdll, "NtSuspendThread");
        auto NtResumeThread           = (pfnNtResumeThread)GetProcAddress(hNtdll, "NtResumeThread");
        auto NtGetContextThread       = (pfnNtGetContextThread)GetProcAddress(hNtdll, "NtGetContextThread");

        if (!NtQuerySystemInformation || !NtSuspendThread || !NtResumeThread || !NtGetContextThread) {
            return results;
        }

        ULONG bufferSize = 1024 * 1024;
        std::vector<BYTE> buffer(bufferSize);
        ULONG returnLength = 0;

        NTSTATUS status = NtQuerySystemInformation(5 /* SystemProcessInformation */, buffer.data(), bufferSize, &returnLength);
        if (status != STATUS_SUCCESS) return results;

        PSYSTEM_PROCESS_INFORMATION procInfo = (PSYSTEM_PROCESS_INFORMATION)buffer.data();
        while (true) {
            if (HandleToULong(procInfo->UniqueProcessId) == processId) {
                PSYSTEM_THREAD_INFORMATION threadArray = (PSYSTEM_THREAD_INFORMATION)((PBYTE)procInfo + sizeof(SYSTEM_PROCESS_INFORMATION));
                
                for (ULONG i = 0; i < procInfo->NumberOfThreads; ++i) {
                    DWORD threadId = HandleToULong(threadArray[i].ClientId.UniqueThread);
                    
                    HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, threadId);
                    if (hThread) {
                        ULONG prevSuspendCount = 0;
                        if (NtSuspendThread(hThread, &prevSuspendCount) == STATUS_SUCCESS) {
                            CONTEXT ctx{};
                            ctx.ContextFlags = CONTEXT_INTEGER;
                            
                            if (NtGetContextThread(hThread, &ctx) == STATUS_SUCCESS) {
                                struct RegInfo { const char* name; uintptr_t val; };
                                RegInfo regs[] = {
                                    { "RAX", ctx.Rax }, { "RBX", ctx.Rbx }, { "RCX", ctx.Rcx }, { "RDX", ctx.Rdx },
                                    { "RSI", ctx.Rsi }, { "RDI", ctx.Rdi }, { "RBP", ctx.Rbp }, { "RSP", ctx.Rsp },
                                    { "R8",  ctx.R8  }, { "R9",  ctx.R9  }, { "R10", ctx.R10 }, { "R11", ctx.R11 },
                                    { "R12", ctx.R12 }, { "R13", ctx.R13 }, { "R14", ctx.R14 }, { "R15", ctx.R15 }
                                };

                                for (const auto& r : regs) {
                                    if (targetAddress >= r.val) {
                                        ptrdiff_t diff = static_cast<ptrdiff_t>(targetAddress - r.val);
                                        if (diff >= 0 && diff <= maxOffset) {
                                            std::ostringstream oss;
                                            oss << r.name << "+0x" << std::hex << diff;
                                            std::string expr = oss.str();
                                            
                                            bool exists = false;
                                            for (const auto& existing : results) {
                                                if (existing.expression == expr) { exists = true; break; }
                                            }
                                            if (!exists) {
                                                results.push_back({r.name, r.val, targetAddress, diff, expr});
                                            }
                                        }
                                    }
                                }
                            }
                            NtResumeThread(hThread, &prevSuspendCount);
                        }
                        CloseHandle(hThread);
                    }
                }
                break;
            }

            if (procInfo->NextEntryOffset == 0) break;
            procInfo = (PSYSTEM_PROCESS_INFORMATION)((PBYTE)procInfo + procInfo->NextEntryOffset);
        }

        return results;
    } //\ FindRegisterOffset

    bool SaveRegisterRecords(const std::string& key, const std::vector<RegisterMatch>& matches) {
        CreateDirectoryA("config", nullptr);

        std::ofstream ofs(kRecordsFile, std::ios::out | std::ios::app);
        if (!ofs.is_open()) {
            std::cerr << "[-] Failed to open records file '" << kRecordsFile << "' for writing.\n";
            return false;
        }

        ofs << "# Record Group: " << key << "\n";
        ofs << "# FORMAT: KEY | REG_NAME | REG_VALUE_HEX | TARGET_ADDR_HEX | OFFSET_HEX | EXPRESSION\n";
        for (const auto& m : matches) {
            ofs << std::left << std::setw(15) << key << " | "
                << std::setw(6) << m.regName << " | 0x"
                << std::hex << std::setw(12) << std::setfill('0') << m.regValue << " | 0x"
                << std::setw(12) << std::setfill('0') << m.targetAddress << " | 0x"
                << std::setw(8) << m.offset << std::dec << std::setfill(' ') << " | "
                << m.expression << "\n";
        }
        ofs << "\n";
        ofs.close();

        std::cout << "[+] Successfully appended " << matches.size() << " register record(s) to '" << kRecordsFile << "'.\n";
        return true;
    } //\ SaveRegisterRecords

    std::map<std::string, RangeRecord> LoadRegistry() {
        std::map<std::string, RangeRecord> registry;
        std::ifstream ifs(kRegistryFile);
        if (!ifs.is_open()) return registry;

        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string key, startStr, endStr, sizeStr;
            
            if (std::getline(iss, key, '|') && 
                std::getline(iss, startStr, '|') && 
                std::getline(iss, endStr, '|') &&
                std::getline(iss, sizeStr)) {
                
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);

                try {
                    uintptr_t start = std::stoull(startStr, nullptr, 16);
                    uintptr_t end = std::stoull(endStr, nullptr, 16);
                    SIZE_T sz = std::stoull(sizeStr);
                    registry[key] = {start, end, sz};
                } catch (...) {}
            }
        }
        return registry;
    }

    bool SaveRange(HANDLE hProcess, const std::string& key, uintptr_t startAddress, uintptr_t endAddress) {
        if (endAddress <= startAddress) {
            std::cerr << "[-] Error: End address must be greater than start address.\n";
            return false;
        }

        CreateDirectoryA("config", nullptr);

        auto registry = LoadRegistry();
        SIZE_T rangeSize = endAddress - startAddress;
        registry[key] = {startAddress, endAddress, rangeSize};

        std::ofstream ofs(kRegistryFile, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            std::cerr << "[-] Failed to open central registry file '" << kRegistryFile << "'.\n";
            return false;
        }

        ofs << "# Nelly Conjure Central Address Registry\n";
        ofs << "# Format: KEY | START_HEX | END_HEX | SIZE_BYTES\n";
        for (const auto& pair : registry) {
            ofs << std::left << std::setw(20) << pair.first << " | 0x" 
                << std::hex << std::setw(12) << std::setfill('0') << pair.second.startAddress 
                << " | 0x" << std::setw(12) << std::setfill('0') << pair.second.endAddress 
                << " | " << std::dec << std::setfill(' ') << pair.second.size << "\n";
        }
        ofs.close();

        std::cout << "[+] Successfully saved range [Key: " << key 
                  << " -> 0x" << std::hex << startAddress << " - 0x" << endAddress 
                  << std::dec << "] to central registry '" << kRegistryFile << "'.\n";
        return true;
    }

    std::optional<RangeRecord> GetRange(const std::string& key) {
        auto registry = LoadRegistry();
        auto it = registry.find(key);
        if (it != registry.end()) {
            return it->second;
        }
        return std::nullopt;
    }

} // namespace Conjure