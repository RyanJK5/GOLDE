#ifndef SimulationWorker_hpp_
#define SimulationWorker_hpp_

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>

#include "BigInt.hpp"
#include "GameGrid.hpp"
#include "HashQuadtree.hpp"

namespace Golde {
class SimulationWorker {
  public:
    SimulationWorker();
    ~SimulationWorker();

    void Start(GameGrid& initialGrid, bool oneStep = false,
               const std::function<void()>& onStop = {});

    GameGrid Stop();

    bool IsRunning();

    void SetStepCount(const BigInt& stepCount);
    void SetTickDelayMs(int64_t tickDelayMs);

    std::optional<GameGrid> GetResult() const;
    std::chrono::duration<float> GetTimeSinceLastUpdate() const;

  private:
    void ThreadLoop(std::stop_token threadStopToken);

    void SimulationLoop(std::stop_token runStopToken);

  private:
    std::optional<GameGrid> m_WorkGrid;
    std::optional<GameGrid> m_DisplayGrid;
    mutable std::mutex m_DisplayMutex;

    std::mutex m_StepCountMutex;
    BigInt m_StepCount = 1;

    std::atomic<int64_t> m_TickDelayMs = 0;

    std::atomic<std::chrono::steady_clock::time_point> m_LastUpdate{};

    std::function<void()> m_OnStop;
    bool m_OneStep = false;

    std::stop_source m_RunStopSource;

    bool m_ResumeReady = false;
    std::mutex m_ResumeMutex;
    std::condition_variable_any m_ResumeCondition;

    std::atomic<bool> m_IsRunning{};
    std::binary_semaphore m_PauseSemaphore{0};

    std::jthread m_Thread;
};
} // namespace Golde

#endif
