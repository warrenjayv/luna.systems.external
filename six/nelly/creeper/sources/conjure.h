#pragma once

#include <windows.h>
#include <string>
#include <optional>

namespace Conjure {
    struct RangeRecord {
        uintptr_t startAddress;
        uintptr_t endAddress;
        SIZE_T size;
    };

    // Save or update an address range by key
    bool SaveRange(HANDLE hProcess, const std::string& key, uintptr_t startAddress, uintptr_t endAddress);

    // Retrieve a range record using only the key
    std::optional<RangeRecord> GetRange(const std::string& key);
}