#include <iostream>
#include <cstdlib>

#include "sources/command.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "[!] Usage: " << argv[0] << " <Target PID>\n";
        return 1;
    }

    DWORD targetPid = static_cast<DWORD>(std::stoul(argv[1]));
    return CommandHandler::StartInteractiveShell(targetPid);
}