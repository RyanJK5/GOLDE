#ifndef SimulationSettings_hpp_
#define SimulationSettings_hpp_

#include <cstdint>

#include "LifeAlgorithm.hpp"

namespace Golde {

struct SimulationSettings {
    BigInt StepCount = BigOne;
    int32_t TickDelayMs = 1;
    bool GridLines = false;
};

} // namespace Golde

#endif
