#include "command.h"
#include "process.h"
#include "network.h"
#include "conjure.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>

void CommandHandler::PrintUsage(const char* /*programName*/) {
    std::cout << "\n[+] Available Interactive Commands:\n"
              << " pipe <pipe_name>                                   - Create a custom named pipe for data streaming\n"
              << " getbase                                            - Get remote process base address\n"
              << " modules                                            - List all loaded modules and base addresses\n" 
              << " readaddr <HexAddress> [Size]                       - Read specific memory address\n"
              << " readmem <StartHex> <FinishHex> [IP] [Port]         - Read memory range and dump\n"
              << " readfloat <HexAddress> [Count] [Filename]          - Read and parse memory as floats/doubles and dump\n"
              << " heaps                                              - List all heaps in the target process\n" 
              << " monitor pty<n>                                     - Stream floating registers live to PTY target (e.g. monitor pty1)\n"
              << " stopmonitor                                        - Stop the floating register monitor\n"
              << " floats <pty target> <start_address> <end_address> [interval_ms] [stride] \n      - Monitor a memory range for float values and log to file\n"
              << " filter <pty target> <start_address> <end_address> [interval_ms] [stride] \n      - Filter a memory range for float values and log to file\n"
              << " stopfloats                                         - Stop the float range monitor\n"
              << " stopfilter                                         - Stop the float filter\n"
              << " scanfloat <min_float> <max_float> [output_file] [stride] \n       - Scan entire user-space memory for floats in range\n"
              << " pid <NewPID>                                       - Switch to a different target PID\n"
              << " memmap [Filename]                                  - Print memory map of the target process and optionally save to file\n"
              << " save <key> <start_hex> <end_hex>                   - Save an address range to the central registry by key\n"
              << " getrange <key>                                     - Retrieve a saved address range by key\n"
              << " exit                                               - Close handle and exit session\n\n";
}

int CommandHandler::StartInteractiveShell(DWORD targetPid) {
    std::cout << "[*] Attaching to target PID " << targetPid << "...\n";
    HANDLE hProcess = ProcessHelper::OpenProcessRead(targetPid);
    
    if (!hProcess) {
        std::cerr << "[-] Failed to open process PID " << targetPid << ". Ensure it exists and you have permissions.\n";
        return 1;
    }

    std::cout << "[+] Successfully attached! Type 'help' for available commands.\n";

    std::string line;
    while (true) {
        std::cout << "nelly[PID:" << targetPid << "]> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        // Parse line into arguments
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        std::vector<std::string> args;
        std::string arg;
        while (ss >> arg) {
            args.push_back(arg);
        }

        if (cmd == "exit") {
            std::cout << "[*] Closing handle and exiting...\n";
            break;
        } 
        else if (cmd == "help" || cmd == "?") {
            PrintUsage("nelly");
        }
        else if (cmd == "pid") {
            if (args.empty()) {
                std::cout << "[!] Current PID: " << targetPid << "\n";
                continue;
            }
            DWORD newPid = static_cast<DWORD>(std::stoul(args[0]));
            std::cout << "[*] Switching to PID " << newPid << "...\n";
            
            ProcessHelper::CloseHandleNative(hProcess);
            hProcess = ProcessHelper::OpenProcessRead(newPid);
            
            if (hProcess) {
                targetPid = newPid;
                std::cout << "[+] Successfully attached to new PID.\n";
            } else {
                std::cerr << "[-] Failed to attach to PID " << newPid << ". Session closing.\n";
                break;
            }
        }
        else if (cmd == "getbase") {
            uintptr_t baseAddr = ProcessHelper::GetProcessBaseAddress(hProcess);
            if (baseAddr) {
                std::cout << "[+] Process Base Address: 0x" << std::hex << baseAddr << std::dec << "\n";
            } else {
                std::cerr << "[-] Could not resolve process base address via PEB.\n";
            }
        }
        else if (cmd == "readaddr") {
            if (args.empty()) {
                std::cout << "[!] Usage: readaddr <HexAddress> [Size]\n";
                continue;
            }

            uintptr_t address = std::strtoull(args[0].c_str(), nullptr, 0);
            SIZE_T size = (args.size() >= 2) ? static_cast<SIZE_T>(std::stoul(args[1])) : 16;

            unsigned char* buffer = new unsigned char[size]();
            SIZE_T bytesRead = 0;

            std::cout << "[*] Reading " << size << " bytes from 0x" << std::hex << address << std::dec << "...\n";
            if (ProcessHelper::ReadMemoryRange(hProcess, address, address + size, buffer, size, &bytesRead)) {
                std::cout << "[+] Successfully read " << bytesRead << " bytes:\n";
                ProcessHelper::PrintHexDump(buffer, bytesRead, address);
            } else {
                std::cerr << "[-] Read failed.\n";
            }
            delete[] buffer;
        }
        else if (cmd == "readmem") {
            if (args.size() < 2) {
                std::cout << "[!] Usage: readmem <StartHex> <FinishHex> [IP] [Port]\n";
                continue;
            }

            uintptr_t startAddress = std::strtoull(args[0].c_str(), nullptr, 0);
            uintptr_t finishAddress = std::strtoull(args[1].c_str(), nullptr, 0);

            if (finishAddress <= startAddress) {
                std::cerr << "[-] Error: Finish address must be greater than start address.\n";
                continue;
            }

            SIZE_T rangeSize = finishAddress - startAddress;
            unsigned char* memoryBuffer = new unsigned char[rangeSize]();
            SIZE_T bytesRead = 0;

            std::cout << "[*] Dumping memory range 0x" << std::hex << startAddress 
                      << " to 0x" << finishAddress << std::dec << "...\n";

            if (ProcessHelper::ReadMemoryRange(hProcess, startAddress, finishAddress, memoryBuffer, rangeSize, &bytesRead)) {
                std::cout << "[+] Read " << bytesRead << " bytes.\n";
                ProcessHelper::DumpToFile(memoryBuffer, bytesRead, startAddress, "dump");
                std::cout << "[+] Saved output to file 'dump'.\n";

                if (args.size() >= 4) {
                    std::string ip = args[2];
                    int port = std::stoi(args[3]);
                    std::cout << "[*] Connecting to " << ip << ":" << port << " to send dump...\n";

                    NetworkClient client;
                    if (client.Initialize() && client.ConnectToServer(ip, port)) {
                        client.SendData(memoryBuffer, bytesRead);
                        std::cout << "[+] Dump sent over network successfully.\n";
                        client.Close();
                    } else {
                        std::cerr << "[-] Network transmission failed.\n";
                    }
                }
            } else {
                std::cerr << "[-] Memory range read failed.\n";
            }
            delete[] memoryBuffer;
        }
        else if (cmd == "modules") {
            ProcessHelper::PrintProcessModules(hProcess);
        }
        else if (cmd == "heaps") {
            ProcessHelper::PrintProcessHeaps(hProcess);
        }
        else if (cmd == "readfloat") {
            if (args.empty()) {
                std::cout << "[!] Usage: readfloat <HexAddress> [count] [filename]\n";
                continue;
            }

            uintptr_t address = std::strtoull(args[0].c_str(), nullptr, 0);
            int count = (args.size() >= 2) ? std::stoi(args[1]) : 4;
            std::string filename = (args.size() >= 3) ? args[2] : "";

            if (!ProcessHelper::ReadMemoryRangeFloats(hProcess, address, count, filename)) {
                std::cerr << "[-] Failed to read or parse floating-point memory.\n";
            }
        }
        else if (cmd == "monitor") {
            if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
                std::cout << "[!] No active process attached. Open a process first.\n";
                continue;
            }
            // Launch the background floating register monitor on the current process and PID
            ProcessHelper::StartMonitor(hProcess, targetPid);
        }
        else if (cmd == "memmap") {
            std::string filename = "";
            if (!args.empty()) {
                filename = args[0]; // Optional custom filename passed by user
            }
            ProcessHelper::PrintProcessMemoryMap(hProcess, filename);
        }
        else if (cmd == "stopmonitor") {
            ProcessHelper::StopMonitor();
        }
else if (cmd == "floats" || cmd == "monitorfloats") {
            if (args.size() < 3) {
                std::cout << "[-] Usage: floats <pty_num|/dev/ptyN> <start_hex> <end_hex> [interval_ms] [stride]\n";
                std::cout << "    Example: floats /dev/pty3 0x22e3002a6d0 0x22e3002a900 100 4\n";
                std::cout << "    Example: floats 3 0x22e3002a6d0 0x22e3002a900 100 4\n";
                continue;
            }

            std::string ptyTarget = args[0];
            std::string startHex = args[1];
            std::string endHex = args[2];
            int intervalMs = 100;
            SIZE_T stride = sizeof(float);

            if (args.size() >= 4) {
                try {
                    intervalMs = std::stoi(args[3]);
                    if (intervalMs < 10) intervalMs = 10;
                } catch (const std::exception&) {
                    std::cout << "[-] Invalid interval_ms format.\n";
                    continue;
                }
            }

            if (args.size() >= 5) {
                try {
                    stride = static_cast<SIZE_T>(std::stoull(args[4]));
                    if (stride == 0) stride = sizeof(float);
                } catch (const std::exception&) {
                    std::cout << "[-] Invalid stride format.\n";
                    continue;
                }
            }

            if (ptyTarget.find("/dev/") != 0) {
                ptyTarget = "/dev/pty" + ptyTarget;
            }

            uintptr_t startAddr = 0;
            uintptr_t endAddr = 0;
            try {
                startAddr = std::stoull(startHex, nullptr, 16);
                endAddr = std::stoull(endHex, nullptr, 16);
            } catch (const std::exception&) {
                std::cout << "[-] Invalid hex address format.\n";
                continue;
            }

            if (endAddr <= startAddr) {
                std::cout << "[-] End address must be greater than start address.\n";
                continue;
            }

            ProcessHelper::StartFloatMonitor(hProcess, targetPid, ptyTarget, startAddr, endAddr, intervalMs, stride);
        }
        else if (cmd == "stopfloats") {
            ProcessHelper::StopFloatMonitor();
        }
        else if (cmd == "filter" || cmd == "filterfloats") {
            if (args.size() < 3) {
                std::cout << "[-] Usage: floatfilter <pty_num|/dev/ptyN> <start_hex> <end_hex> [interval_ms] [stride]\n";
                std::cout << "    Example: floatfilter 3 0x1FF3002A6D0 0x1FF3002A800 100 4\n";
                continue;
            }

            std::string ptyTarget = args[0];
            std::string startHex = args[1];
            std::string endHex = args[2];
            int intervalMs = 100;
            SIZE_T stride = sizeof(float);

            if (args.size() >= 4) {
                try {
                    intervalMs = std::stoi(args[3]);
                    if (intervalMs < 10) intervalMs = 10;
                } catch (const std::exception&) {
                    std::cout << "[-] Invalid interval_ms format.\n";
                    continue;
                }
            }

            if (args.size() >= 5) {
                try {
                    stride = static_cast<SIZE_T>(std::stoull(args[4]));
                    if (stride == 0) stride = sizeof(float);
                } catch (const std::exception&) {
                    std::cout << "[-] Invalid stride format.\n";
                    continue;
                }
            }

            if (ptyTarget.find("/dev/") != 0) {
                ptyTarget = "/dev/pty" + ptyTarget;
            }

            uintptr_t startAddr = 0;
            uintptr_t endAddr = 0;
            try {
                startAddr = std::stoull(startHex, nullptr, 16);
                endAddr = std::stoull(endHex, nullptr, 16);
            } catch (const std::exception&) {
                std::cout << "[-] Invalid hex address format.\n";
                continue;
            }

            if (endAddr <= startAddr) {
                std::cout << "[-] End address must be greater than start address.\n";
                continue;
            }

            ProcessHelper::StartFloatFilter(hProcess, targetPid, ptyTarget, startAddr, endAddr, intervalMs, stride);
        }
        else if (cmd == "stopfilter") {
            ProcessHelper::StopFloatFilter();
        }
        else if (cmd == "pipe") {
                if (args.empty()) {
                    std::cout << "[!] Usage: pipe <pipe_name>\n"
                              << "    Example: pipe floats_alpha\n";
                    continue;
                }
                std::string pipeName = args[0];
                ProcessHelper::StartCustomPipe(pipeName);
        }
        else if (cmd == "scanfloat") {
            if (args.size() < 2) {
                std::cout << "[-] Usage: scanfloat <min_float> <max_float> [output_file] [stride]\n";
                std::cout << "    Example: scanfloat 99.5 100.5\n";
                std::cout << "    Example: scanfloat -500.0 500.0 scan_results.csv 4\n";
                continue;
            }

            float minVal = 0.0f;
            float maxVal = 0.0f;
            std::string outFile = (args.size() >= 3) ? args[2] : "";
            SIZE_T stride = sizeof(float);

            try {
                minVal = std::stof(args[0]);
                maxVal = std::stof(args[1]);
                if (args.size() >= 4) {
                    stride = static_cast<SIZE_T>(std::stoull(args[3]));
                }
            } catch (const std::exception&) {
                std::cout << "[-] Invalid numeric argument format.\n";
                continue;
            }

            ProcessHelper::StartFloatScan(hProcess, targetPid, minVal, maxVal, outFile, stride);
        }
        else if (cmd == "stopscan") {
            ProcessHelper::StopFloatScan();
        }
        else if (cmd == "save") {
            if (args.size() < 3) {
                std::cout << "[-] Usage: save <key> <start_hex> <end_hex>\n";
                std::cout << "    Example: save player_pos 0x1FF3002A6D0 0x1FF3002A800\n";
                continue;
            }

            std::string key = args[0];
            uintptr_t startAddr = 0;
            uintptr_t endAddr = 0;

            try {
                startAddr = std::stoull(args[1], nullptr, 16);
                endAddr = std::stoull(args[2], nullptr, 16);
            } catch (const std::exception&) {
                std::cout << "[-] Invalid hex address format.\n";
                continue;
            }

            Conjure::SaveRange(hProcess, key, startAddr, endAddr);
        }
        else if (cmd == "getrange") {
            if (args.empty()) {
                std::cout << "[-] Usage: getrange <key>\n";
                std::cout << "    Example: getrange player_pos\n";
                continue;
            }

            std::string key = args[0];
            auto record = Conjure::GetRange(key);
            if (record.has_value()) {
                std::cout << "[+] Found Range for Key [" << key << "]:\n";
                std::cout << "    Start : 0x" << std::hex << std::setw(12) << std::setfill('0') << record->startAddress << "\n";
                std::cout << "    End   : 0x" << std::setw(12) << std::setfill('0') << record->endAddress << "\n";
                std::cout << "    Size  : " << std::dec << record->size << " bytes\n" << std::flush;
            } else {
                std::cout << "[-] Key '" << key << "' not found in central registry.\n";
            }
        }
        else {
            std::cout << "[-] Unknown command: " << cmd << ". Type 'help' for options.\n";
        }
    } // End of interactive shell loop

    // Clean up process handle on exit
    ProcessHelper::CloseHandleNative(hProcess);
    return 0;
}