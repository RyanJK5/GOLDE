#ifndef SimulationSettings_hpp_
#define SimulationSettings_hpp_

#include <cstdint>

#include "LifeAlgorithm.hpp"

namespace gol {

struct SimulationSettings {
    BigInt StepCount = BigOne;
    std::string_view AlgorithmIdentifier = "HashLife";
    int32_t TickDelayMs = 1;
    bool HyperSpeed = false;
    bool GridLines = false;
};

} // namespace gol

#endif
