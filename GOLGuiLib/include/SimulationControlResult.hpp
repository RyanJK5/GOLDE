#ifndef SimulationControlResult_h_
#define SimulationControlResult_h_

#include <optional>

#include "SimulationCommand.hpp"
#include "SimulationSettings.hpp"

namespace gol {
struct SimulationControlResult {
    std::optional<SimulationCommand> Command{};
    SimulationSettings Settings{};
    bool FromShortcut = false;
};
} // namespace gol

#endif