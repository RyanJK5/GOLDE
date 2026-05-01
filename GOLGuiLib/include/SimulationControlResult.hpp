#ifndef SimulationControlResult_hpp_
#define SimulationControlResult_hpp_

#include <optional>

#include "SimulationCommand.hpp"
#include "SimulationSettings.hpp"

namespace Golde {
struct SimulationControlResult {
    std::optional<SimulationCommand> Command{};
    SimulationSettings Settings{};
    bool FromShortcut = false;
};
} // namespace Golde

#endif
