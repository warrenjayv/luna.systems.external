#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>
#include <cmath>

struct ResolverConfig {
    float minStepDelta = 0.0001f;   // Minimum change |V(t) - V(t-1)| to count as active movement
    float maxStepDelta = 500.0f;    // Maximum allowed single-tick jump |V(t) - V(t-1)| (filters garbage/NaNs)
    float maxRadius    = 10000.0f;  // Maximum allowed total displacement |V(t) - V(0)| from initial value
    int   maxStaticTicks = 15;      // Discard address if it fails to move by minStepDelta for N consecutive ticks
    size_t maxHistorySamples = 0;   // 0 = run until stopped (time infinity), >0 = stop evaluating after N samples
};

struct TrackedFloat {
    uintptr_t address = 0;
    float initialValue = 0.0f;
    float lastValue = 0.0f;
    float totalDistance = 0.0f;     // Cumulative path distance traveled: sum(|V(t) - V(t-1)|)
    int staticTicks = 0;            // Consecutive ticks without meaningful movement
    size_t sampleCount = 0;
    bool active = true;
};

class Resolver {
public:
    Resolver();
    ~Resolver();

    // Initializes candidate addresses from [startAddress, endAddress) and spawns the worker thread
    bool Start(uintptr_t startAddress, uintptr_t endAddress, SIZE_T stride, const ResolverConfig& config = ResolverConfig{});

    // Stops the background thread and clears state
    void Stop();

    // Non-blocking: called by FloatMonitorWorker to hand off a raw memory snapshot
    void PushSample(uintptr_t baseAddress, const std::vector<unsigned char>& rawBuffer, SIZE_T validBytes);

    // Thread-safe: returns the list of surviving candidate addresses (and optionally their stats)
    std::vector<TrackedFloat> GetSurvivingCandidates() const;

    // Quick check if a specific address is still active
    bool IsAddressActive(uintptr_t address) const;

    size_t GetActiveCount() const;
    size_t GetTotalProcessedSamples() const;

private:
    struct SampleSnapshot {
        uintptr_t baseAddress;
        std::vector<unsigned char> data;
        SIZE_T validBytes;
    };

    static DWORD WINAPI ResolverThreadProc(LPVOID lpParam);
    void ProcessLoop();
    void EvaluateSnapshot(const SampleSnapshot& snap);

    ResolverConfig m_config;
    uintptr_t m_startAddress = 0;
    uintptr_t m_endAddress = 0;
    SIZE_T m_stride = sizeof(float);

    std::atomic<bool> m_isRunning{false};
    std::atomic<size_t> m_processedSamples{0};
    HANDLE m_hThread = NULL;
    HANDLE m_hDataEvent = NULL;

    mutable std::mutex m_candidatesMutex;
    std::unordered_map<uintptr_t, TrackedFloat> m_candidates;

    std::mutex m_queueMutex;
    std::queue<SampleSnapshot> m_sampleQueue;
};