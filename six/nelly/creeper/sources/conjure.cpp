#include "conjure.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <optional>
#include <vector>
#include <psapi.h>
#include <cstring>

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
typedef NTSTATUS(WINAPI* pfnNtQueryVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    ULONG MemoryInformationClass,
    PVOID MemoryInformation,
    SIZE_T MemoryInformationLength,
    PSIZE_T ReturnLength
);
typedef NTSTATUS(WINAPI* pfnNtReadVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToRead,
    PSIZE_T NumberOfBytesRead
);

namespace Conjure {

    const std::string kRegistryFile = "config/conjure_registry.cfg";
    const std::string kRecordsFile  = "config/records";

    struct ModuleSpan {
            std::string name;
            uintptr_t base;
            uintptr_t end;
    }; //\ ModuleSpan

    struct NtMemoryApis {
            pfnNtQueryVirtualMemory QueryVirtualMemory = nullptr;
            pfnNtReadVirtualMemory  ReadVirtualMemory  = nullptr;
            bool valid = false;
    }; //\ NtMemoryApis

    NtMemoryApis ResolveNtMemoryApis() {
            NtMemoryApis apis;
            HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
            if (!hNtdll) return apis;

            apis.QueryVirtualMemory = reinterpret_cast<pfnNtQueryVirtualMemory>(
                GetProcAddress(hNtdll, "NtQueryVirtualMemory"));
            apis.ReadVirtualMemory = reinterpret_cast<pfnNtReadVirtualMemory>(
                GetProcAddress(hNtdll, "NtReadVirtualMemory"));

            apis.valid = (apis.QueryVirtualMemory != nullptr && apis.ReadVirtualMemory != nullptr);
            return apis;
    } //\ ResolveNtMemoryApis

    std::string GetBaseName(const std::string& fullPath) {
            size_t pos = fullPath.find_last_of("\\/");
            return (pos == std::string::npos) ? fullPath : fullPath.substr(pos + 1);
    } // GetBaseName

    std::vector<ModuleSpan> EnumerateStaticModules(HANDLE hProcess, DWORD processId) {
            std::vector<ModuleSpan> modules;
            (void)processId;

            HMODULE mods[1024];
            DWORD cbNeeded = 0;
            if (!EnumProcessModulesEx(hProcess, mods, sizeof(mods), &cbNeeded, LIST_MODULES_ALL)) {
                return modules;
            }

            size_t count = cbNeeded / sizeof(HMODULE);
            for (size_t i = 0; i < count; ++i) {
                MODULEINFO info{};
                char pathBuf[MAX_PATH] = {0};
                if (GetModuleInformation(hProcess, mods[i], &info, sizeof(info)) &&
                    GetModuleFileNameExA(hProcess, mods[i], pathBuf, MAX_PATH) > 0) {
                    uintptr_t base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                    uintptr_t end  = base + static_cast<uintptr_t>(info.SizeOfImage);
                    modules.push_back({ GetBaseName(pathBuf), base, end });
                }
            }
            return modules;
    } //\ EnumerateStaticModules

    void ScanModuleForPointers (
                HANDLE hProcess,
                const NtMemoryApis& ntMem,
                const ModuleSpan& mod,
                uintptr_t targetAddress,
                ptrdiff_t radius,
                const std::string& mode,
                std::vector<PointerScanMatch>& outMatches)
    {
                if (!ntMem.valid || mod.end <= mod.base) return;

                const ULONG kMemoryBasicInformation = 0; // MemoryBasicInformation
                uintptr_t cursor = mod.base;

                while (cursor < mod.end) {
                    MEMORY_BASIC_INFORMATION mbi{};
                    SIZE_T returnLength = 0;

                    NTSTATUS qStatus = ntMem.QueryVirtualMemory(
                        hProcess,
                        reinterpret_cast<PVOID>(cursor),
                        kMemoryBasicInformation,
                        &mbi,
                        sizeof(mbi),
                        &returnLength);

                    if (qStatus != STATUS_SUCCESS || mbi.RegionSize == 0) {
                        cursor += 0x1000;
                        continue;
                    }

                    uintptr_t regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                    uintptr_t regionEnd   = regionStart + mbi.RegionSize;
                    if (regionEnd > mod.end) regionEnd = mod.end;

                    bool readable = (mbi.State == MEM_COMMIT) &&
                                    !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));

                    if (readable && regionEnd > regionStart + sizeof(uintptr_t)) {
                        SIZE_T regionSize = static_cast<SIZE_T>(regionEnd - regionStart);
                        std::vector<BYTE> buf(regionSize);
                        SIZE_T bytesRead = 0;

                        NTSTATUS rStatus = ntMem.ReadVirtualMemory(
                            hProcess,
                            reinterpret_cast<PVOID>(regionStart),
                            buf.data(),
                            regionSize,
                            &bytesRead);

                        if (rStatus == STATUS_SUCCESS && bytesRead >= sizeof(uintptr_t)) {
                            for (SIZE_T off = 0; off + sizeof(uintptr_t) <= bytesRead; off += sizeof(uintptr_t)) {
                                uintptr_t val = 0;
                                std::memcpy(&val, buf.data() + off, sizeof(uintptr_t));

                                if (val == 0) continue;

                                ptrdiff_t diff = static_cast<ptrdiff_t>(targetAddress) - static_cast<ptrdiff_t>(val);
                                ptrdiff_t absDiff = (diff < 0) ? -diff : diff;

                                bool matched = (radius == 0) ? (diff == 0) : (absDiff <= radius);
                                if (matched) {
                                    uintptr_t ptrAddr = regionStart + off;
                                    PointerScanMatch match{
                                        mode,
                                        mod.name,
                                        ptrAddr,
                                        ptrAddr - mod.base,
                                        val,
                                        targetAddress,
                                        diff
                                    };

                                    bool duplicate = false;
                                    for (const auto& existing : outMatches) {
                                        if (existing.pointerAddress == match.pointerAddress &&
                                            existing.pointedValue == match.pointedValue) {
                                            duplicate = true;
                                            break;
                                        }
                                    }
                                    if (!duplicate) {
                                        outMatches.push_back(match);
                                    }
                                }
                            }
                        }
                    }

                    if (regionEnd <= cursor) break;
                    cursor = regionEnd;
                }
    } //\ ScanModuleForPointers

    std::vector<PointerScanMatch> PointerScan(HANDLE hProcess, DWORD processId, uintptr_t targetAddress, ptrdiff_t fallbackRadius) {
        std::vector<PointerScanMatch> results;
        if (!hProcess) return results;

        NtMemoryApis ntMem = ResolveNtMemoryApis();
        if (!ntMem.valid) return results;

        auto modules = EnumerateStaticModules(hProcess, processId);
        if (modules.empty()) return results;

        // Pass 1: Direct static pointers pointing to targetAddress
        for (const auto& mod : modules) {
            ScanModuleForPointers(hProcess, ntMem, mod, targetAddress, 0, "DIRECT", results);
        }

        // Pass 2: Fallback if no direct static pointer found -> search +/- fallbackRadius
        if (results.empty() && fallbackRadius > 0) {
            for (const auto& mod : modules) {
                ScanModuleForPointers(hProcess, ntMem, mod, targetAddress, fallbackRadius, "NEAR_1000B", results);
            }
        }

        return results;
    } //\ PointerScan

    bool SavePointerRecords(const std::string& label, uintptr_t targetAddress, const std::vector<PointerScanMatch>& matches) {
        CreateDirectoryA("config", nullptr);

        std::ofstream ofs(kRecordsFile, std::ios::out | std::ios::app);
        if (!ofs.is_open()) {
            std::cerr << "[-] Failed to open records file '" << kRecordsFile << "' for writing.\n";
            return false;
        }

        ofs << "# Pointer Scan | Label: " << label
            << " | Target: 0x" << std::hex << targetAddress << std::dec << "\n";
        ofs << "# FORMAT: MODE | MODULE+OFFSET | PTR_ADDR_HEX | POINTED_VAL_HEX | DELTA_FROM_TARGET\n";

        for (const auto& m : matches) {
            ofs << std::left << std::setw(12) << m.mode << " | "
                << m.moduleName << "+0x" << std::hex << m.moduleOffset << " | 0x"
                << std::setw(12) << std::setfill('0') << m.pointerAddress << " | 0x"
                << std::setw(12) << std::setfill('0') << m.pointedValue << std::dec << std::setfill(' ') << " | ";

            if (m.deltaFromTarget >= 0) {
                ofs << "+0x" << std::hex << m.deltaFromTarget << std::dec;
            } else {
                ofs << "-0x" << std::hex << (-m.deltaFromTarget) << std::dec;
            }
            ofs << "\n";
        }
        ofs << "\n";
        ofs.close();

        std::cout << "[+] Appended " << matches.size() << " pointer record(s) to '" << kRecordsFile << "'.\n";
        return true;
    } //\ SavePointerRecords

    // ... (Keep LoadRegistry, SaveRange, GetRange unchanged) ...
    std::vector<RegisterMatch> FindRegisterOffset(HANDLE hProcess, DWORD processId, uintptr_t targetAddress, int maxOffset) {
        std::vector<RegisterMatch> results;
        (void)hProcess;

        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return results;

        auto NtQuerySystemInformation = reinterpret_cast<pfnNtQuerySystemInformation>(
            GetProcAddress(hNtdll, "NtQuerySystemInformation"));
        auto NtSuspendThread = reinterpret_cast<pfnNtSuspendThread>(
            GetProcAddress(hNtdll, "NtSuspendThread"));
        auto NtResumeThread = reinterpret_cast<pfnNtResumeThread>(
            GetProcAddress(hNtdll, "NtResumeThread"));
        auto NtGetContextThread = reinterpret_cast<pfnNtGetContextThread>(
            GetProcAddress(hNtdll, "NtGetContextThread"));

        if (!NtQuerySystemInformation || !NtSuspendThread || !NtResumeThread || !NtGetContextThread) {
            return results;
        }

        const NTSTATUS STATUS_INFO_LENGTH_MISMATCH = static_cast<NTSTATUS>(0xC0000004L);
        ULONG bufferSize = 1024 * 1024;
        std::vector<BYTE> buffer(bufferSize);
        ULONG returnLength = 0;

        NTSTATUS status = NtQuerySystemInformation(5 /* SystemProcessInformation */, buffer.data(), bufferSize, &returnLength);
        while (status == STATUS_INFO_LENGTH_MISMATCH) {
            bufferSize = (returnLength > bufferSize) ? (returnLength + 65536) : (bufferSize * 2);
            buffer.resize(bufferSize);
            status = NtQuerySystemInformation(5 /* SystemProcessInformation */, buffer.data(), bufferSize, &returnLength);
        }

        if (status != STATUS_SUCCESS) {
            return results;
        }

        auto* procInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(buffer.data());
        while (true) {
            if (HandleToULong(procInfo->UniqueProcessId) == processId) {
                auto* threadArray = reinterpret_cast<PSYSTEM_THREAD_INFORMATION>(
                    reinterpret_cast<PBYTE>(procInfo) + sizeof(SYSTEM_PROCESS_INFORMATION));

                for (ULONG i = 0; i < procInfo->NumberOfThreads; ++i) {
                    DWORD threadId = HandleToULong(threadArray[i].ClientId.UniqueThread);
                    if (threadId == GetCurrentThreadId()) continue;

                    HANDLE hThread = OpenThread(
                        THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                        FALSE,
                        threadId);

                    if (hThread) {
                        ULONG prevSuspendCount = 0;
                        if (NtSuspendThread(hThread, &prevSuspendCount) == STATUS_SUCCESS) {
                            alignas(16) CONTEXT ctx{};
                            ctx.ContextFlags = CONTEXT_INTEGER;

                            if (NtGetContextThread(hThread, &ctx) == STATUS_SUCCESS) {
                                struct RegInfo {
                                    const char* name;
                                    uintptr_t val;
                                };

                                RegInfo regs[] = {
                                    { "RAX", ctx.Rax }, { "RBX", ctx.Rbx }, { "RCX", ctx.Rcx }, { "RDX", ctx.Rdx },
                                    { "RSI", ctx.Rsi }, { "RDI", ctx.Rdi }, { "RBP", ctx.Rbp }, { "RSP", ctx.Rsp },
                                    { "R8",  ctx.R8  }, { "R9",  ctx.R9  }, { "R10", ctx.R10 }, { "R11", ctx.R11 },
                                    { "R12", ctx.R12 }, { "R13", ctx.R13 }, { "R14", ctx.R14 }, { "R15", ctx.R15 }
                                };

                                for (const auto& r : regs) {
                                    if (r.val != 0 && targetAddress >= r.val) {
                                        ptrdiff_t diff = static_cast<ptrdiff_t>(targetAddress - r.val);
                                        if (diff >= 0 && diff <= maxOffset) {
                                            std::ostringstream oss;
                                            oss << r.name << "+0x" << std::hex << diff;
                                            std::string expr = oss.str();

                                            bool exists = false;
                                            for (const auto& existing : results) {
                                                if (existing.expression == expr && existing.regValue == r.val) {
                                                    exists = true;
                                                    break;
                                                }
                                            }
                                            if (!exists) {
                                                results.push_back({ r.name, r.val, targetAddress, diff, expr });
                                            }
                                        }
                                    }
                                }
                            }

                            ULONG resumeCount = 0;
                            NtResumeThread(hThread, &resumeCount);
                        }
                        CloseHandle(hThread);
                    }
                }
                break;
            }

            if (procInfo->NextEntryOffset == 0) break;
            procInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(
                reinterpret_cast<PBYTE>(procInfo) + procInfo->NextEntryOffset);
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