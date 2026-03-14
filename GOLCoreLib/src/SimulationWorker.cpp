#include "SimulationWorker.hpp"

#include <chrono>
#include <condition_variable>
#include <print>

namespace gol {
void SimulationWorker::Start(GameGrid& initialGrid, bool oneStep,
                             const std::function<void()>& onStop) {
    initialGrid.PrepareCopyBetweenThreads();
    m_Buffers[0] = initialGrid;
    m_Buffers[1] = initialGrid;
    m_Buffers[2] = initialGrid;

    m_SnapshotIndex.store(0, std::memory_order_release);

    m_Thread = std::jthread{[this, oneStep,
                             onStop](std::stop_token stopToken) mutable {
        auto workerIdx = 1UZ;
        auto backIdx = 2UZ;

        std::condition_variable_any sleepCondition{};
        std::mutex sleepMutex{};
        auto nextFrame = std::chrono::steady_clock::now();

        while (true) {
            m_LastUpdate.store(std::chrono::steady_clock::now(),
                               std::memory_order_relaxed);
            m_Buffers[workerIdx].Update(
                m_StepCount.load(std::memory_order_relaxed), stopToken);

            if (stopToken.stop_requested())
                break;

            // Publish workerIdx as the new snapshot, get back the old one
            std::swap(workerIdx, backIdx);
            backIdx =
                m_SnapshotIndex.exchange(workerIdx, std::memory_order_acq_rel);

            if (oneStep)
                break;

            const auto tickDelayMs =
                m_TickDelayMs.load(std::memory_order_relaxed);
            if (tickDelayMs > 0) {
                nextFrame += std::chrono::milliseconds{tickDelayMs};
                std::unique_lock lock{sleepMutex};
                bool stopRequested = sleepCondition.wait_until(
                    lock, stopToken, nextFrame, [] { return false; });
                if (stopRequested)
                    break;
            }
        }

        m_Buffers[m_SnapshotIndex.load(std::memory_order_acquire)]
            .PrepareCopyBetweenThreads();
        if (oneStep && !stopToken.stop_requested())
            onStop();
    }};
}

GameGrid SimulationWorker::Stop() {
    if (m_Thread.joinable()) {
        m_Thread.request_stop();
        m_Thread.join();
    }
    return m_Buffers[m_SnapshotIndex.load(std::memory_order_relaxed)];
}

void SimulationWorker::SetStepCount(int64_t stepCount) {
    m_StepCount.store(stepCount, std::memory_order_relaxed);
}

void SimulationWorker::SetTickDelayMs(int64_t tickDelayMs) {
    m_TickDelayMs.store(tickDelayMs, std::memory_order_relaxed);
}

const GameGrid* SimulationWorker::GetResult() const {
    if (!m_Thread.joinable()) {
        return nullptr;
    }
    return &m_Buffers[m_SnapshotIndex.load(std::memory_order_acquire)];
}

std::chrono::duration<float> SimulationWorker::GetTimeSinceLastUpdate() const {
    return std::chrono::steady_clock::now() -
           m_LastUpdate.load(std::memory_order_relaxed);
}
} // namespace gol
