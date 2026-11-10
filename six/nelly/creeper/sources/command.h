#pragma once
#include <iostream>
#include <string>
#include <windows.h>

class CommandHandler {
public:
    static void PrintUsage(const char* programName);
    static int StartInteractiveShell(DWORD initialPid);
};