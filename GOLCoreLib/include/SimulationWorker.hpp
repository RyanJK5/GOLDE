#ifndef SimulationWorker_h_
#define SimulationWorker_h_

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "GameGrid.hpp"
#include "HashQuadtree.hpp"

namespace gol {
class SimulationWorker {
  public:
    void Start(GameGrid& initialGrid, bool oneStep = false,
               const std::function<void()>& onStop = {});
    GameGrid Stop();

    void SetStepCount(int64_t stepCount);
    void SetTickDelayMs(int64_t tickDelayMs);

    const GameGrid* GetResult() const;
    std::chrono::duration<float> GetTimeSinceLastUpdate() const;

  private:
    std::atomic<int64_t> m_StepCount = 1;
    std::atomic<int64_t> m_TickDelayMs = 0;

    std::atomic<std::chrono::steady_clock::time_point> m_LastUpdate;

    std::array<GameGrid, 3> m_Buffers{};
    std::atomic<size_t> m_SnapshotIndex;

    std::jthread m_Thread;
};
} // namespace gol

#endif
