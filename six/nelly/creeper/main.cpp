#include "command.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "[!] Usage: " << argv[0] << " <Target PID>\n";
        return 1;
    }

    DWORD targetPid = static_cast<DWORD>(std::stoul(argv[1]));
    return CommandHandler::StartInteractiveShell(targetPid);
}