#ifndef Plane_hpp_
#define Plane_hpp_

#include "Topology.hpp"

namespace Golde {
class Plane : public Topology {
  public:
    Plane(Rect bounds = {});

    std::string_view GetIdentifier() const override;

    bool CompatibleWith(LifeDataStructure& data) const override;

    std::unique_ptr<Topology> Clone() const override;

    int32_t Log2MaxIncrement(const BigInt& requestedStep) const override;

    void PrepareBorderCells(LifeDataStructure& data) override;

    void CleanupBorderCells(LifeDataStructure& data) override;
};
} // namespace Golde

#endif