#include "HashQuadtree.hpp"
#include "LifeNode.hpp"

namespace Golde {
HashQuadtree::Iterator::Iterator(const LifeNode* root, Vec2L offset,
                                 int32_t level, bool isEnd, const Rect* bounds)
    : m_Bounds(bounds ? *bounds : Rect{}), m_Current(), m_IsEnd(isEnd),
      m_UseBounds(bounds != nullptr) {
    if (!isEnd && root != FalseNode) {
        if (!m_UseBounds || IntersectsBounds(m_Bounds, offset, level)) {
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
                if (!m_UseBounds || IsWithinBounds(m_Bounds, frame.Position)) {
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
            (!m_UseBounds ||
             IntersectsBounds(m_Bounds, childPos, childLevel))) {
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
} // namespace Golde