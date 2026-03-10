#ifndef WidgetResult_h_
#define WidgetResult_h_

#include <optional>

#include "SimulationCommand.hpp"

namespace gol {

struct WidgetResult {
    std::optional<SimulationCommand> Command{};
    bool FromShortcut = false;
};

} // namespace gol

#endif
