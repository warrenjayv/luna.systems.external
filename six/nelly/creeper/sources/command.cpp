#include "command.h"
#include "process.h"
#include "network.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>

void CommandHandler::PrintUsage(const char* /*programName*/) {
    std::cout << "\n[+] Available Interactive Commands:\n"
              << "    getbase                                          - Get remote process base address\n"
              << "    modules                                          - List all loaded modules and base addresses\n" 
              << "    readaddr <HexAddress> [Size]                     - Read specific memory address\n"
              << "    readmem <StartHex> <FinishHex> [IP] [Port]       - Read memory range and dump\n"
              << "    readfloat <HexAddress> [Count] [Filename]        - Read and parse memory as floats/doubles and dump\n"
              << "    heaps                                            - List all heaps in the target process\n" 
              << "    monitor pty<n>                                   - Stream floating registers live to PTY target (e.g. monitor pty1)\n"
              << "    stopmonitor                                      - Stop the floating register monitor\n"
              << "    pid <NewPID>                                     - Switch to a different target PID\n"
              << "    memmap [Filename]                                - Print memory map of the target process and optionally save to file\n"
              << "    exit                                             - Close handle and exit session\n\n";
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
            if (args.empty()) {
                std::cout << "[!] Usage: monitor pty<n> (e.g., monitor pty1)\n";
                continue;
            }

            std::string ptyArg = args[0];
            if (ptyArg.rfind("pty", 0) == 0) {
                try {
                    int ptyNum = std::stoi(ptyArg.substr(3));
                    // Start the background PTY register monitor
                    ProcessHelper::StartMonitorPty(hProcess, targetPid, ptyNum);
                } catch (...) {
                    std::cerr << "[-] Invalid PTY format. Use 'monitor pty1', 'monitor pty2', etc.\n";
                }
            } else {
                std::cout << "[!] Unknown monitor target. Usage: monitor pty<n>\n";
            }
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
        else {
            std::cout << "[-] Unknown command: " << cmd << ". Type 'help' for options.\n";
        }
    } // End of interactive shell loop

    // Clean up process handle on exit
    ProcessHelper::CloseHandleNative(hProcess);
    return 0;
}