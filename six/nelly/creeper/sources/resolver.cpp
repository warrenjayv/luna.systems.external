#include "resolver.h"
#include <iostream>
#include <algorithm>

Resolver::Resolver() = default;

Resolver::~Resolver() {
    Stop();
}

bool Resolver::Start(uintptr_t startAddress, uintptr_t endAddress, SIZE_T stride, const ResolverConfig& config) {
    Stop(); // Clean up any prior run

    if (endAddress <= startAddress) {
        return false;
    }

    m_startAddress = startAddress;
    m_endAddress = endAddress;
    m_stride = (stride == 0) ? sizeof(float) : stride;
    m_config = config;
    m_processedSamples = 0;

    // Seed all candidate addresses in range
    {
        std::lock_guard<std::mutex> lock(m_candidatesMutex);
        m_candidates.clear();
        for (uintptr_t addr = m_startAddress; addr + sizeof(float) <= m_endAddress; addr += m_stride) {
            TrackedFloat tf;
            tf.address = addr;
            tf.active = true;
            m_candidates.emplace(addr, tf);
        }
    }

    m_hDataEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_hDataEvent) {
        return false;
    }

    m_isRunning = true;
    m_hThread = CreateThread(nullptr, 0, ResolverThreadProc, this, 0, nullptr);
    if (!m_hThread) {
        m_isRunning = false;
        CloseHandle(m_hDataEvent);
        m_hDataEvent = NULL;
        return false;
    }

    return true;
}

void Resolver::Stop() {
    if (m_isRunning) {
        m_isRunning = false;
        if (m_hDataEvent) {
            SetEvent(m_hDataEvent);
        }
    }

    if (m_hThread) {
        WaitForSingleObject(m_hThread, 2000);
        CloseHandle(m_hThread);
        m_hThread = NULL;
    }

    if (m_hDataEvent) {
        CloseHandle(m_hDataEvent);
        m_hDataEvent = NULL;
    }

    std::lock_guard<std::mutex> qLock(m_queueMutex);
    while (!m_sampleQueue.empty()) {
        m_sampleQueue.pop();
    }
}

void Resolver::PushSample(uintptr_t baseAddress, const std::vector<unsigned char>& rawBuffer, SIZE_T validBytes) {
    if (!m_isRunning || validBytes < sizeof(float)) {
        return;
    }

    // Check if we hit the optional sample limit (0 = infinite)
    if (m_config.maxHistorySamples > 0 && m_processedSamples.load() >= m_config.maxHistorySamples) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // Prevent unbounded memory growth if target is sampled faster than evaluated
        if (m_sampleQueue.size() > 64) {
            m_sampleQueue.pop();
        }
        m_sampleQueue.push(SampleSnapshot{baseAddress, rawBuffer, validBytes});
    }

    if (m_hDataEvent) {
        SetEvent(m_hDataEvent);
    }
}

DWORD WINAPI Resolver::ResolverThreadProc(LPVOID lpParam) {
    Resolver* self = static_cast<Resolver*>(lpParam);
    if (self) {
        self->ProcessLoop();
    }
    return 0;
}

void Resolver::ProcessLoop() {
    while (m_isRunning) {
        DWORD waitRes = WaitForSingleObject(m_hDataEvent, 100);
        if (!m_isRunning) break;

        if (waitRes == WAIT_OBJECT_0) {
            while (m_isRunning) {
                SampleSnapshot snap;
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    if (m_sampleQueue.empty()) break;
                    snap = std::move(m_sampleQueue.front());
                    m_sampleQueue.pop();
                }

                EvaluateSnapshot(snap);
                m_processedSamples++;

                if (m_config.maxHistorySamples > 0 && m_processedSamples.load() >= m_config.maxHistorySamples) {
                    m_isRunning = false;
                    break;
                }
            }
        }
    }
}

void Resolver::EvaluateSnapshot(const SampleSnapshot& snap) {
    std::lock_guard<std::mutex> lock(m_candidatesMutex);

    for (auto it = m_candidates.begin(); it != m_candidates.end(); ) {
        TrackedFloat& tf = it->second;

        if (tf.address < snap.baseAddress) {
            ++it;
            continue;
        }

        size_t offset = static_cast<size_t>(tf.address - snap.baseAddress);
        if (offset + sizeof(float) > snap.validBytes || offset + sizeof(float) > snap.data.size()) {
            ++it;
            continue;
        }

        float currentVal = *reinterpret_cast<const float*>(snap.data.data() + offset);

        // 1. Immediately discard NaN, Inf, or denormalized garbage pointers masquerading as floats
        if (!std::isfinite(currentVal)) {
            it = m_candidates.erase(it);
            continue;
        }

        // 2. First sample (T0): establish baseline
        if (tf.sampleCount == 0) {
            tf.initialValue = currentVal;
            tf.lastValue = currentVal;
            tf.sampleCount = 1;
            ++it;
            continue;
        }

        // 3. Calculate step delta |V(t) - V(t-1)| and radius from origin |V(t) - V(0)|
        float stepDelta = std::fabs(currentVal - tf.lastValue);
        float displacementFromOrigin = std::fabs(currentVal - tf.initialValue);

        // Discard if single-tick jump exceeds max allowed delta D (non-continuous / pointer / flag)
        if (stepDelta > m_config.maxStepDelta) {
            it = m_candidates.erase(it);
            continue;
        }

        // Discard if total displacement exceeds allowed radius
        if (displacementFromOrigin > m_config.maxRadius) {
            it = m_candidates.erase(it);
            continue;
        }

        // Track static ticks (values that remain frozen or below minStepDelta)
        if (stepDelta < m_config.minStepDelta) {
            tf.staticTicks++;
            if (tf.staticTicks >= m_config.maxStaticTicks) {
                it = m_candidates.erase(it);
                continue;
            }
        } else {
            // Moved within valid delta D range -> reset static counter & accumulate path distance
            tf.staticTicks = 0;
            tf.totalDistance += stepDelta;
        }

        tf.lastValue = currentVal;
        tf.sampleCount++;
        ++it;
    }
}

std::vector<TrackedFloat> Resolver::GetSurvivingCandidates() const {
    std::vector<TrackedFloat> results;
    std::lock_guard<std::mutex> lock(m_candidatesMutex);
    results.reserve(m_candidates.size());

    for (const auto& pair : m_candidates) {
        if (pair.second.active) {
            results.push_back(pair.second);
        }
    }

    std::sort(results.begin(), results.end(), [](const TrackedFloat& a, const TrackedFloat& b) {
        return a.address < b.address;
    });

    return results;
}

bool Resolver::IsAddressActive(uintptr_t address) const {
    std::lock_guard<std::mutex> lock(m_candidatesMutex);
    return m_candidates.find(address) != m_candidates.end();
}

size_t Resolver::GetActiveCount() const {
    std::lock_guard<std::mutex> lock(m_candidatesMutex);
    return m_candidates.size();
}

size_t Resolver::GetTotalProcessedSamples() const {
    return m_processedSamples.load();
}