#include "HashQuadtree.hpp"

namespace gol {
bool HashQuadtree::Iterator::IsWithinBounds(Vec2L pos) const {
    if (!m_UseBounds) {
        return true;
    }

    const auto left = static_cast<int64_t>(m_Bounds.X);
    const auto top = static_cast<int64_t>(m_Bounds.Y);
    const auto right = left + m_Bounds.Width;
    const auto bottom = top + m_Bounds.Height;
    return pos.X >= left && pos.X < right && pos.Y >= top && pos.Y < bottom;
}

bool HashQuadtree::Iterator::IntersectsBounds(Vec2L pos, int32_t level) const {
    constexpr static auto minBound = std::numeric_limits<int32_t>::min();
    constexpr static auto maxBound = std::numeric_limits<int32_t>::max();

    if (pos.X < minBound || pos.X > maxBound || pos.Y < minBound ||
        pos.Y > maxBound) {
        return false;
    }
    if (!m_UseBounds) {
        return true;
    }

    const auto regionRight = pos.X + Pow2(level);
    const auto regionBottom = pos.Y + Pow2(level);

    const auto left = static_cast<int64_t>(m_Bounds.X);
    const auto top = static_cast<int64_t>(m_Bounds.Y);
    const auto right = left + m_Bounds.Width;
    const auto bottom = top + m_Bounds.Height;

    return !(regionRight <= left || pos.X >= right || regionBottom <= top ||
             pos.Y >= bottom);
}

HashQuadtree::Iterator::Iterator(const LifeNode* root, Vec2L offset,
                                 int32_t level, bool isEnd, const Rect* bounds)
    : m_Bounds(bounds ? *bounds : Rect{}), m_Current(), m_IsEnd(isEnd),
      m_UseBounds(bounds != nullptr) {
    if (!isEnd && root != FalseNode) {
        if (!m_UseBounds || IntersectsBounds(offset, level)) {
            m_Stack.push({root, offset, level, 0});
            AdvanceToNext();
        } else {
            m_IsEnd = true;
        }
    }
}

void HashQuadtree::Iterator::AdvanceToNext() {
    while (!m_Stack.empty()) {
        auto& frame = m_Stack.top();

        // If we're at a leaf (size == 1)
        if (frame.Level == 0) {
            if (frame.Node == TrueNode) {
                if (IsWithinBounds(frame.Position)) {
                    m_Current = Vec2{static_cast<int32_t>(frame.Position.X),
                                     static_cast<int32_t>(frame.Position.Y)};
                    m_Stack.pop();
                    return; // Found a live cell within bounds
                }
            }
            m_Stack.pop();
            continue;
        }

        // Process next quadrant
        if (frame.Quadrant >= 4) {
            m_Stack.pop();
            continue;
        }

        const auto childLevel = frame.Level - 1;
        const auto halfSize = Pow2(childLevel);
        const LifeNode* child = nullptr;
        auto childPos = frame.Position;

        switch (frame.Quadrant++) {
        case 0:
            child = frame.Node->NorthWest;
            break;
        case 1:
            child = frame.Node->NorthEast;
            childPos.X += halfSize;
            break;
        case 2:
            child = frame.Node->SouthWest;
            childPos.Y += halfSize;
            break;
        case 3:
            child = frame.Node->SouthEast;
            childPos.X += halfSize;
            childPos.Y += halfSize;
            break;
        }
        m_Stack.top().Quadrant = frame.Quadrant;

        if (child != FalseNode && !child->IsEmpty &&
            IntersectsBounds(childPos, childLevel)) {
            m_Stack.push({child, childPos, childLevel, 0});
        }
    }

    m_IsEnd = true; // No more live cells
}

HashQuadtree::Iterator& HashQuadtree::Iterator::operator++() {
    AdvanceToNext();
    return *this;
}

HashQuadtree::Iterator HashQuadtree::Iterator::operator++(int) {
    auto copy = *this;
    AdvanceToNext();
    return copy;
}

bool HashQuadtree::Iterator::operator==(const Iterator& other) const {
    return m_IsEnd == other.m_IsEnd &&
           (m_IsEnd || m_Current == other.m_Current);
}

bool HashQuadtree::Iterator::operator!=(const Iterator& other) const {
    return !(*this == other);
}

typename HashQuadtree::Iterator::value_type
HashQuadtree::Iterator::operator*() const {
    return m_Current;
}

typename HashQuadtree::Iterator::pointer
HashQuadtree::Iterator::operator->() const {
    return &m_Current;
}
} // namespace gol