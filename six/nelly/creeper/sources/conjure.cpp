#include "conjure.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <optional>

namespace Conjure {

    const std::string kRegistryFile = "config/conjure_registry.cfg";

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