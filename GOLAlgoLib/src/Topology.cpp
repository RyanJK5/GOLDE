#include "Topology.hpp"

namespace Golde {
Topology::Topology(Rect bounds) : m_Bounds(bounds) {}

std::optional<Rect> Topology::GetBounds() const {
    if (m_Bounds == Rect{}) {
        return std::nullopt;
    }
    return m_Bounds;
}
} // namespace Golde