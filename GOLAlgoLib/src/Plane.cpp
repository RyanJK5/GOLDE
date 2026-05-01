#include "Plane.hpp"
#include "HashQuadtree.hpp"
#include <iostream>

namespace Golde {
Plane::Plane(Rect bounds) : Topology(bounds) {}

std::string_view Plane::GetIdentifier() const { return "Plane"; }

std::unique_ptr<Topology> Plane::Clone() const {
    auto bounds = GetBounds();
    return std::make_unique<Plane>(bounds ? *bounds : Rect{});
}

bool Plane::CompatibleWith(LifeDataStructure& data) const {
    return typeid(HashQuadtree) == typeid(data);
}

int32_t Plane::Log2MaxIncrement(const BigInt& requestedStep) const {
    if (GetBounds()) {
        return 0;
    }
    if (requestedStep.is_zero()) {
        return -1;
    }

    return static_cast<int32_t>(boost::multiprecision::msb(requestedStep));
}

void Plane::PrepareBorderCells(LifeDataStructure&) {}

void Plane::CleanupBorderCells(LifeDataStructure& data) {
    auto bounds = GetBounds();
    if (!bounds) {
        return;
    }

    auto& hashQuadtree = dynamic_cast<HashQuadtree&>(data);

    const auto newData = hashQuadtree.Extract(*bounds);
    hashQuadtree.OverwriteData(newData.Data(), newData.CalculateDepth());
}
} // namespace Golde