#ifndef WidgetResult_hpp_
#define WidgetResult_hpp_

#include <optional>

#include "SimulationCommand.hpp"

namespace Golde {

struct WidgetResult {
    std::optional<SimulationCommand> Command{};
    bool FromShortcut = false;
};

} // namespace Golde

#endif
