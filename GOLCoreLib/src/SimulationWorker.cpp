#include "SimulationWorker.hpp"

#include <chrono>
#include <condition_variable>
#include <print>

namespace gol {

SimulationWorker::SimulationWorker(size_t cacheIndex)
    : m_CacheIndex(cacheIndex),
      m_Thread(std::bind_front(&SimulationWorker::ThreadLoop, this)) {}

SimulationWorker::~SimulationWorker() {
    m_RunStopSource.request_stop();
    HashQuadtree::ClearCache();
}

void SimulationWorker::ThreadLoop(std::stop_token threadStopToken) {
    HashQuadtree::SetCacheIndex(m_CacheIndex);
    while (true) {
        {
            std::unique_lock lock{m_ResumeMutex};
            m_ResumeCondition.wait(lock, threadStopToken,
                                   [&] { return m_ResumeReady; });
            if (m_BufferedRule != nullptr) {
                for (auto i = 0UZ; i < 3UZ; i++) {
                    m_Buffers[i].SetRule(*m_BufferedRule);
                }
                m_BufferedRule = nullptr;
            }

            if (threadStopToken.stop_requested()) {
                return;
            }
            m_ResumeReady = false;
        }

        auto runStopToken = m_RunStopSource.get_token();
        SimulationLoop(runStopToken);

        if (m_OneStep && !runStopToken.stop_requested()) {
            m_OnStop();
        }

        m_PauseSemaphore.release();
    }
}

size_t SimulationWorker::SimulationLoop(std::stop_token runStopToken) {
    auto workerIndex = 1UZ;

    std::condition_variable_any sleepCondition{};
    std::mutex sleepMutex{};
    auto nextFrame = std::chrono::steady_clock::now();

    while (!runStopToken.stop_requested()) {
        m_LastUpdate.store(std::chrono::steady_clock::now(),
                           std::memory_order_relaxed);

        // SYNC WITH LATEST SNAPSHOT:
        // This ensures both independent buffers don't desynchronize or maintain
        // separate generation lineages when `stepCount` changes mid-flight.
        auto snapshot = m_SnapshotIndex.load(std::memory_order_acquire);
        m_Buffers[workerIndex] = m_Buffers[snapshot];

        auto stepCount = [&] {
            std::scoped_lock locK{m_StepCountMutex};
            return m_StepCount;
        }();

        m_Buffers[workerIndex].Update(stepCount, runStopToken);

        if (runStopToken.stop_requested()) {
            break;
        }

        // Publish workerIndex as the new snapshot
        m_SnapshotIndex.store(workerIndex, std::memory_order_release);

        // Wait for UI to finish reading the old snapshot.
        // We select the one buffer that is NOT the new snapshot, and NOT the
        // old snapshot. Since we have buffers 0, 1, 2, the remaining is `3 -
        // workerIndex - snapshot`.
        workerIndex = 3UZ - workerIndex - snapshot;

        if (m_OneStep) {
            break;
        }

        const auto tickDelayMs = m_TickDelayMs.load(std::memory_order_relaxed);
        if (tickDelayMs > 0) {
            nextFrame += std::chrono::milliseconds{tickDelayMs};

            std::unique_lock lock{sleepMutex};
            sleepCondition.wait_until(lock, runStopToken, nextFrame,
                                      [] { return false; });
        }
    }

    return workerIndex;
}

void SimulationWorker::Start(GameGrid& initialGrid, bool oneStep,
                             const std::function<void()>& onStop) {
    if (m_IsRunning.exchange(true, std::memory_order_acq_rel)) {
        m_RunStopSource.request_stop();
        m_PauseSemaphore.acquire();
    }
    m_RunStopSource = {};

    m_Buffers[0] = initialGrid;
    m_Buffers[1] = initialGrid;
    m_Buffers[2] = initialGrid;
    m_SnapshotIndex.store(0UZ, std::memory_order_release);

    {
        std::scoped_lock lock{m_ResumeMutex};
        m_OneStep = oneStep;
        m_OnStop = onStop;
        m_ResumeReady = true;
    }
    m_ResumeCondition.notify_one();
}

GameGrid SimulationWorker::Stop() {
    if (m_IsRunning.exchange(false, std::memory_order_acq_rel)) {
        m_RunStopSource.request_stop();
        m_PauseSemaphore.acquire();
    }
    return m_Buffers[m_SnapshotIndex.load(std::memory_order_relaxed)];
}

bool SimulationWorker::IsRunning() {
    return m_IsRunning.load(std::memory_order_acquire);
}

void SimulationWorker::SetStepCount(const BigInt& stepCount) {
    std::scoped_lock lock{m_StepCountMutex};
    m_StepCount = stepCount;
}

void SimulationWorker::SetTickDelayMs(int64_t tickDelayMs) {
    m_TickDelayMs.store(tickDelayMs, std::memory_order_relaxed);
}

const GameGrid* SimulationWorker::GetResult() const {
    if (!m_IsRunning.load(std::memory_order_acquire)) {
        return nullptr;
    }
    return &m_Buffers[m_SnapshotIndex.load(std::memory_order_acquire)];
}

std::chrono::duration<float> SimulationWorker::GetTimeSinceLastUpdate() const {
    return std::chrono::steady_clock::now() -
           m_LastUpdate.load(std::memory_order_relaxed);
}

void SimulationWorker::BufferRule(std::unique_ptr<LifeRule> rule) {
    m_BufferedRule = std::move(rule);
}
} // namespace gol
