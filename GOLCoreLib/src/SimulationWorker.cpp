#include "SimulationWorker.hpp"

#include <chrono>
#include <condition_variable>
#include <print>

namespace Golde {

SimulationWorker::SimulationWorker()
    : m_Thread(std::bind_front(&SimulationWorker::ThreadLoop, this)) {}

SimulationWorker::~SimulationWorker() { m_RunStopSource.request_stop(); }

void SimulationWorker::ThreadLoop(std::stop_token threadStopToken) {
    while (true) {
        {
            std::unique_lock lock{m_ResumeMutex};
            m_ResumeCondition.wait(lock, threadStopToken,
                                   [&] { return m_ResumeReady; });

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

void SimulationWorker::SimulationLoop(std::stop_token runStopToken) {
    std::condition_variable_any sleepCondition{};
    std::mutex sleepMutex{};
    auto nextFrame = std::chrono::steady_clock::now();

    while (!runStopToken.stop_requested()) {
        m_LastUpdate.store(std::chrono::steady_clock::now(),
                           std::memory_order_relaxed);

        auto stepCount = [&] {
            std::scoped_lock locK{m_StepCountMutex};
            return m_StepCount;
        }();

        m_WorkGrid->Update(stepCount, runStopToken);

        if (runStopToken.stop_requested()) {
            break;
        }

        {
            std::scoped_lock lock{m_DisplayMutex};
            m_DisplayGrid = m_WorkGrid;
        }

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
}

void SimulationWorker::Start(GameGrid& initialGrid, bool oneStep,
                             const std::function<void()>& onStop) {
    if (m_IsRunning.exchange(true, std::memory_order_acq_rel)) {
        m_RunStopSource.request_stop();
        m_PauseSemaphore.acquire();
    }
    m_RunStopSource = {};

    m_WorkGrid = initialGrid;
    m_DisplayGrid = initialGrid;

    m_LastUpdate.store(std::chrono::steady_clock::now(),
                       std::memory_order_relaxed);

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

    const auto ret = std::move(*m_DisplayGrid);
    m_WorkGrid = std::nullopt;
    m_DisplayGrid = std::nullopt;
    return ret;
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

std::optional<GameGrid> SimulationWorker::GetResult() const {
    std::scoped_lock lock{m_DisplayMutex};
    return m_DisplayGrid;
}

std::chrono::duration<float> SimulationWorker::GetTimeSinceLastUpdate() const {
    return std::chrono::steady_clock::now() -
           m_LastUpdate.load(std::memory_order_relaxed);
}
} // namespace Golde
