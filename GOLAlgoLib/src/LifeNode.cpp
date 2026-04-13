#include "LifeNode.hpp"
#include <bit>
#include <functional>

namespace gol {
bool IsWithinBounds(const RectL& bounds, Vec2L pos) {
    const auto left = bounds.X;
    const auto top = bounds.Y;
    const auto right = left + bounds.Width;
    const auto bottom = top + bounds.Height;
    return pos.X >= left && pos.X < right && pos.Y >= top && pos.Y < bottom;
}

bool IsWithinBounds(Rect bounds, Vec2L pos) {
    return IsWithinBounds(
        RectL{bounds.X, bounds.Y, bounds.Width, bounds.Height}, pos);
}

bool IntersectsBounds(const RectL& bounds, Vec2L pos, int32_t level) {
    const auto regionRight = pos.X + Pow2(level);
    const auto regionBottom = pos.Y + Pow2(level);

    const auto left = bounds.X;
    const auto top = bounds.Y;
    const auto right = left + bounds.Width;
    const auto bottom = top + bounds.Height;

    return !(regionRight <= left || pos.X >= right || regionBottom <= top ||
             pos.Y >= bottom);
}

bool IsWithinBounds(const BigRect& bounds, Vec2L pos) {
    const auto left = bounds.X;
    const auto top = bounds.Y;
    const auto right = left + bounds.Width;
    const auto bottom = top + bounds.Height;
    const BigInt x{pos.X};
    const BigInt y{pos.Y};

    return x >= left && x < right && y >= top && y < bottom;
}

bool IsWithinBounds(const BigRect& bounds, const BigVec2& pos) {
    const auto left = bounds.X;
    const auto top = bounds.Y;
    const auto right = left + bounds.Width;
    const auto bottom = top + bounds.Height;

    return pos.X >= left && pos.X < right && pos.Y >= top && pos.Y < bottom;
}

bool IntersectsBounds(const BigRect& bounds, Vec2L pos, int32_t level) {
    const auto regionRight = BigInt{pos.X} + (BigOne << level);
    const auto regionBottom = BigInt{pos.Y} + (BigOne << level);

    const auto left = bounds.X;
    const auto top = bounds.Y;
    const auto right = left + bounds.Width;
    const auto bottom = top + bounds.Height;

    const BigInt x{pos.X};
    const BigInt y{pos.Y};

    return !(regionRight <= left || x >= right || regionBottom <= top ||
             y >= bottom);
}

bool IntersectsBounds(const BigRect& bounds, const BigVec2& pos,
                      const BigInt& size) {
    const auto regionRight = pos.X + size;
    const auto regionBottom = pos.Y + size;

    const auto left = bounds.X;
    const auto top = bounds.Y;
    const auto right = left + bounds.Width;
    const auto bottom = top + bounds.Height;

    return !(regionRight <= left || pos.X >= right || regionBottom <= top ||
             pos.Y >= bottom);
}

bool IntersectsBounds(Rect bounds, Vec2L pos, int32_t level) {
    return IntersectsBounds(
        RectL{bounds.X, bounds.Y, bounds.Width, bounds.Height}, pos, level);
}

uint64_t LifeNode::ComputeHash(const LifeNode* nw, const LifeNode* ne,
                               const LifeNode* sw, const LifeNode* se) {
    // Shift pointers right by 4 to discard alignment zeros, then mix.
    auto a = std::bit_cast<uint64_t>(nw) >> 4;
    auto b = std::bit_cast<uint64_t>(ne) >> 4;
    auto c = std::bit_cast<uint64_t>(sw) >> 4;
    auto d = std::bit_cast<uint64_t>(se) >> 4;

    // Fast 4-to-1 mix using rotations + xor-fold + splitmix64 finalizer.
    uint64_t h = a ^ std::rotl(b, 16) ^ std::rotl(c, 32) ^ std::rotl(d, 48);
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
    h = h ^ (h >> 31);
    return h;
}

LifeNodeKey::LifeNodeKey(const LifeNode* nw, const LifeNode* ne,
                         const LifeNode* sw, const LifeNode* se)
    : NorthWest(nw), NorthEast(ne), SouthWest(sw), SouthEast(se),
      Hash(LifeNode::ComputeHash(nw, ne, sw, se)) {}

bool LifeNodeEqual::operator()(const LifeNode* lhs, const LifeNode* rhs) const {
    if (lhs == rhs)
        return true;
    if (!lhs || !rhs)
        return false;
    return lhs->NorthWest == rhs->NorthWest &&
           lhs->NorthEast == rhs->NorthEast &&
           lhs->SouthWest == rhs->SouthWest && lhs->SouthEast == rhs->SouthEast;
}

bool LifeNodeEqual::operator()(const LifeNode* lhs,
                               const LifeNodeKey& rhs) const {
    if (!lhs)
        return false;
    return lhs->NorthWest == rhs.NorthWest && lhs->NorthEast == rhs.NorthEast &&
           lhs->SouthWest == rhs.SouthWest && lhs->SouthEast == rhs.SouthEast;
}

bool LifeNodeEqual::operator()(const LifeNodeKey& lhs,
                               const LifeNode* rhs) const {
    return operator()(rhs, lhs);
}

size_t LifeNodeHash::operator()(const LifeNode* node) const {
    if (!node)
        return std::hash<const void*>{}(nullptr);
    return node->Hash;
}

size_t LifeNodeHash::operator()(const LifeNodeKey& key) const {
    return static_cast<size_t>(key.Hash);
}

LifeNode* LifeNodeArena::last() const {
    return m_Blocks.back().get() + (m_Current - 1);
}

void LifeNodeArena::clear() {
    m_Blocks.clear();
    m_Current = BlockCapacity;
}

void LifeNodeArena::BlockDeleter::operator()(LifeNode* p) const {
    ::operator delete(p);
}
} // namespace gol