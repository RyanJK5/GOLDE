#ifndef Topology_hpp_
#define Topology_hpp_

#include "BigInt.hpp"
#include "Graphics2D.hpp"
#include "LifeDataStructure.hpp"
#include "LifeNode.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace Golde {
class Topology {
  public:
    Topology(Rect bounds = {});

    virtual ~Topology() = default;

    std::optional<Rect> GetBounds() const;

    virtual std::string_view GetIdentifier() const = 0;

    virtual bool CompatibleWith(LifeDataStructure& data) const = 0;

    virtual std::unique_ptr<Topology> Clone() const = 0;

    // log2 of the maximum number of generations an algorithm can currently
    // step. Returns -1 if hyper speed can be used.
    virtual int32_t Log2MaxIncrement(const BigInt& requestedStep) const = 0;

    // Should be called before the algorithm is performed.
    virtual void PrepareBorderCells(LifeDataStructure& data) = 0;

    // Should be called after the algorithm is performed.
    virtual void CleanupBorderCells(LifeDataStructure& data) = 0;

  private:
    Rect m_Bounds;
};
} // namespace Golde

#endif