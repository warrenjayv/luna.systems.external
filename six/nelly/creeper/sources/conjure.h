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

    // Save or update an address range by key
    bool SaveRange(HANDLE hProcess, const std::string& key, uintptr_t startAddress, uintptr_t endAddress);

    // Retrieve a range record using only the key
    std::optional<RangeRecord> GetRange(const std::string& key);

    // Native NT thread scan returning detailed register match records
    std::vector<RegisterMatch> FindRegisterOffset(HANDLE hProcess, DWORD processId, uintptr_t targetAddress, int maxOffset = 0x1000);

    // Saves register match records to config/records
    bool SaveRegisterRecords(const std::string& key, const std::vector<RegisterMatch>& matches);
}