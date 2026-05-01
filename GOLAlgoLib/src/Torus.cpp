#include "Torus.hpp"
#include "HashQuadtree.hpp"

namespace Golde {
Torus::Torus(Rect bounds) : Topology(bounds) {}

std::string_view Torus::GetIdentifier() const { return "Torus"; }

bool Torus::CompatibleWith(LifeDataStructure& data) const {
    return typeid(data) == typeid(HashQuadtree);
}

std::unique_ptr<Topology> Torus::Clone() const {
    auto bounds = GetBounds();
    return std::make_unique<Torus>(bounds ? *bounds : Rect{});
}

int32_t Torus::Log2MaxIncrement(const BigInt& requestedStep) const {
    if (GetBounds()) {
        return 0;
    }
    if (requestedStep.is_zero()) {
        return -1;
    }

    return static_cast<int32_t>(boost::multiprecision::msb(requestedStep));
}

void Torus::PrepareBorderCells(LifeDataStructure& data) {
    auto bounds = GetBounds();
    if (!bounds) {
        return;
    }

    auto& hashQuadtree = dynamic_cast<HashQuadtree&>(data);
    const auto boundingBox = hashQuadtree.FindBoundingBox();

    const bool wrapX = bounds->Width > 0;
    const bool wrapY = bounds->Height > 0;

    if (wrapY) {
        const auto startX = wrapX ? 0 : boundingBox.X;
        const auto endX =
            wrapX ? bounds->Width : boundingBox.X + boundingBox.Width;
        for (auto x = startX; x < endX; x++) {
            if (hashQuadtree.Get({x, 0})) {
                hashQuadtree.Set({x, bounds->Height}, true);
            }

            if (hashQuadtree.Get({x, bounds->Height - 1})) {
                hashQuadtree.Set({x, -1}, true);
            }
        }
    }

    if (wrapX) {
        const auto startY = wrapY ? 0 : boundingBox.Y;
        const auto endY =
            wrapY ? bounds->Height : boundingBox.Y + boundingBox.Height;
        for (auto y = startY; y < endY; y++) {
            if (hashQuadtree.Get({0, y})) {
                hashQuadtree.Set({bounds->Width, y}, true);
            }

            if (hashQuadtree.Get({bounds->Width - 1, y})) {
                hashQuadtree.Set({-1, y}, true);
            }
        }
    }

    if (wrapX && wrapY) {
        // bottom-right -> top-left border
        if (hashQuadtree.Get({bounds->Width - 1, bounds->Height - 1})) {
            hashQuadtree.Set({-1, -1}, true);
        }

        // bottom-left -> top-right border
        if (hashQuadtree.Get({0, bounds->Height - 1})) {
            hashQuadtree.Set({bounds->Width, -1}, true);
        }

        // top-right -> bottom-left border
        if (hashQuadtree.Get({bounds->Width - 1, 0})) {
            hashQuadtree.Set({-1, bounds->Height}, true);
        }

        // top-left -> bottom-right border
        if (hashQuadtree.Get({0, 0})) {
            hashQuadtree.Set({bounds->Width, bounds->Height}, true);
        }
    }
}

void Torus::CleanupBorderCells(LifeDataStructure& data) {
    auto bounds = GetBounds();
    if (!bounds) {
        return;
    }

    auto& hashQuadtree = dynamic_cast<HashQuadtree&>(data);

    const auto newData = hashQuadtree.Extract(*bounds);
    hashQuadtree.OverwriteData(newData.Data(), newData.CalculateDepth());
}
} // namespace Golde