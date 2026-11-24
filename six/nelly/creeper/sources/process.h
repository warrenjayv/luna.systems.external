#pragma once
#include <windows.h>
#include <string>

namespace ProcessHelper {
    HANDLE OpenProcessRead(DWORD processId);
    void CloseHandleNative(HANDLE hHandle);
    uintptr_t GetProcessBaseAddress(HANDLE hProcess);
    bool PrintProcessModules(HANDLE hProcess); 
    bool PrintProcessHeaps(HANDLE hProcess);
    bool ReadMemoryRange(HANDLE hProcess, uintptr_t startAddress, uintptr_t finishAddress, unsigned char* outBuffer, SIZE_T bufferSize, SIZE_T* totalBytesRead);
    bool ReadMemoryRangeFloats(HANDLE hProcess, uintptr_t address, int count, const std::string& outFilename = "");
    bool DumpToFile(const unsigned char* buffer, SIZE_T size, uintptr_t baseAddress, const std::string& filename);
    void PrintHexDump(const unsigned char* buffer, SIZE_T size, uintptr_t baseAddress);
    DWORD GetFirstThreadId(DWORD processId);
    bool StartMonitor(HANDLE hProcess, DWORD processId);
    bool StartFloatFilter(HANDLE hProcess, DWORD processId, const std::string& ptyTarget, uintptr_t startAddress, uintptr_t endAddress, int intervalMs, SIZE_T stride);
    bool StartFloatMonitor(HANDLE hProcess, DWORD processId, const std::string& ptyTarget, uintptr_t startAddress, uintptr_t endAddress, int intervalMs, SIZE_T stride);
    bool PrintProcessMemoryMap(HANDLE hProcess, const std::string& outFilename = "");
    void StopMonitor();
    void StopFloatFilter();
    void StopFloatMonitor();
    bool StartCustomPipe(const std::string& pipeName);
    bool StartFloatScan(HANDLE hProcess, DWORD processId, float minVal, float maxVal, const std::string& outFile = "", SIZE_T stride = sizeof(float));
    void StopFloatScan();
}