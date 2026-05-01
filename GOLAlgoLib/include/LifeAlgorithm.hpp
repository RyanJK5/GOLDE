#ifndef LifeAlgorithm_hpp_
#define LifeAlgorithm_hpp_

#include "BigInt.hpp"
#include "LifeDataStructure.hpp"
#include "LifeRule.hpp"
#include "Topology.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace Golde {
class LifeAlgorithm {
  public:
    virtual ~LifeAlgorithm() = default;

    virtual BigInt Step(LifeDataStructure& data, const BigInt& numSteps,
                        std::stop_token stopToken = {}) = 0;

    virtual void SetTopology(std::unique_ptr<Topology> topology) = 0;

    virtual void SetRule(const LifeRule& rule) = 0;

    virtual bool CompatibleWith(const LifeDataStructure& data) const = 0;

    virtual std::string_view GetIdentifier() const = 0;

    virtual std::unique_ptr<LifeAlgorithm> Clone() const = 0;
};
} // namespace Golde

#endif