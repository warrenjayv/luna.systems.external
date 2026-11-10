#pragma once
#include <windows.h>
#include <string>

namespace ProcessHelper {
    HANDLE OpenProcessRead(DWORD processId);
    void CloseHandleNative(HANDLE hHandle);
    uintptr_t GetProcessBaseAddress(HANDLE hProcess);
    bool PrintProcessModules(HANDLE hProcess); // <-- New function
    bool ReadMemoryRange(HANDLE hProcess, uintptr_t startAddress, uintptr_t finishAddress, unsigned char* outBuffer, SIZE_T bufferSize, SIZE_T* totalBytesRead);
    bool DumpToFile(const unsigned char* buffer, SIZE_T size, uintptr_t baseAddress, const std::string& filename);
    void PrintHexDump(const unsigned char* buffer, SIZE_T size, uintptr_t baseAddress);
}