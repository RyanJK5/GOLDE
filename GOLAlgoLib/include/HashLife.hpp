#ifndef HashLife_hpp_
#define HashLife_hpp_

#include "LifeAlgorithm.hpp"

namespace gol {
class HashLife : public LifeAlgorithm, AlgorithmRegistrator<HashLife> {
  public:
    static std::string_view Identifier;

    HashLife();

    BigInt Step(LifeDataStructure& data, const BigInt& numSteps,
                std::stop_token stopToken = {}) override;
};
} // namespace gol

#endif