#ifndef Torus_hpp_
#define Torus_hpp_

#include "Topology.hpp"

namespace Golde {
class Torus : public Topology {
  public:
    Torus(Rect bounds = {});

    std::string_view GetIdentifier() const override;

    bool CompatibleWith(LifeDataStructure& data) const override;

    std::unique_ptr<Topology> Clone() const override;

    int32_t Log2MaxIncrement(const BigInt& requestedStep) const override;

    void PrepareBorderCells(LifeDataStructure& data) override;

    void CleanupBorderCells(LifeDataStructure& data) override;
};
} // namespace Golde

#endif