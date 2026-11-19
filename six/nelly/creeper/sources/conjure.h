#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace Conjure {
    struct RangeRecord {
        uintptr_t startAddress;
        uintptr_t endAddress;
        SIZE_T size;
    }; //\ RangeRecord

    struct RegisterMatch {
        std::string regName;
        uintptr_t regValue;
        uintptr_t targetAddress;
        ptrdiff_t offset;
        std::string expression;
    }; //\ RegisterMatch

    struct PointerScanMatch {
        std::string mode;          // "DIRECT" or "NEAR_1000B"
        std::string moduleName;    // e.g., "game.exe" or "unityplayer.dll"
        uintptr_t pointerAddress;  // Address in static module memory holding the pointer
        uintptr_t moduleOffset;    // pointerAddress - moduleBase
        uintptr_t pointedValue;    // Value stored at pointerAddress (heap/target address)
        uintptr_t targetAddress;   // Float address passed to pointer_scan
        ptrdiff_t deltaFromTarget; // targetAddress - pointedValue
    }; //\ PointerScanMatch

    // Save or update an address range by key
    bool SaveRange(HANDLE hProcess, const std::string& key, uintptr_t startAddress, uintptr_t endAddress);

    // Retrieve a range record using only the key
    std::optional<RangeRecord> GetRange(const std::string& key);

    // Native NT thread scan returning detailed register match records
    std::vector<RegisterMatch> FindRegisterOffset(HANDLE hProcess, DWORD processId, uintptr_t targetAddress, int maxOffset = 0x1000);

    // searches for static pointers holding addresses within [targetAddress - 1000, targetAddress + 1000].
    std::vector<PointerScanMatch> PointerScan(HANDLE hProcess, DWORD processId, uintptr_t targetAddress, ptrdiff_t fallbackRadius = 1000);

    // Saves register match records to config/records
    bool SaveRegisterRecords(const std::string& key, const std::vector<RegisterMatch>& matches);
    bool SavePointerRecords(const std::string& label, uintptr_t targetAddress, const std::vector<PointerScanMatch>& matches);
}