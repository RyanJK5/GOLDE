#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <stop_token>
#include <type_traits>
#include <vector>

#include "Graphics2D.hpp"
#include "HashQuadtree.hpp"
#include "LifeAlgorithm.hpp"
#include "LifeHashSet.hpp"
#include "LifeRule.hpp"

namespace gol {
constexpr static auto ViewportMaxLevel = 31;

HashLifeCache::HashLifeCache() {
    EmptyNodeCache.resize(64, nullptr);
    NodeMap.reserve(1UZ << 20UZ);
}

thread_local HashLifeCache HashQuadtree::s_Cache{};

// Mixes the node's precomputed hash with MaxAdvance. The node hash is already
// well-distributed via splitmix64, so a single round of xor-shift mixing with
// the advance count is sufficient.
size_t SlowHash::operator()(SlowKey key) const noexcept {
    // Use a non-zero seed to prevent (0,0) from hashing to 0
    // This is a prime or a large constant like 0x9e3779b9
    auto h = key.Node ? key.Node->Hash : 0xD3212C32483522FBULL;

    const auto advanceHash = static_cast<uint64_t>(key.AdvanceLevel);

    // 1. Multiplicative combine using the Golden Ratio
    // We shift 'h' to ensure the nodeHash and advanceHash don't
    // just sit in the same bit-lanes before the multiply.
    constexpr static auto GoldenRatio = 0x9E3779B97F4A7C15ULL;
    h ^= (advanceHash * GoldenRatio) + 0x9e3779b9 + (h << 6) + (h >> 2);

    // 2. The "Avalanche": A faster, lighter version of Murmur's mixer.
    // This ensures that changes in AdvanceLevel propagate
    // across the entire 64-bit result.
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;

    return static_cast<size_t>(h);
}

HashQuadtree::HashQuadtree(std::span<const Vec2> data, Vec2 offset) {
    if (data.empty())
        return;

    m_Root = BuildTree(data);
    m_SeedOffset +=
        {static_cast<int64_t>(offset.X), static_cast<int64_t>(offset.Y)};
}

const LifeNode* HashQuadtree::Data() const { return m_Root; }

void HashQuadtree::OverwriteData(const LifeNode* root, int32_t level) {
    m_Root = root;
    m_Depth = level;
}

int32_t HashQuadtree::CalculateDepth() const { return m_Depth; }

// TODO: Improve efficiency of equality comparison
bool HashQuadtree::operator==(const HashQuadtree& other) const {
    if (m_Root == other.m_Root && m_SeedOffset == other.m_SeedOffset) {
        return true;
    }

    const auto hashSet1 = *this | std::ranges::to<LifeHashSet>();
    const auto hashSet2 = other | std::ranges::to<LifeHashSet>();

    return hashSet1 == hashSet2;
}

bool HashQuadtree::operator!=(const HashQuadtree& other) const {
    return !(*this == other);
}

void HashQuadtree::Set(Vec2 targetPos, bool alive) {
    const auto expansionNeeded = [&] {
        if (m_Depth == 0) {
            return true;
        }

        const auto half = Pow2(m_Depth - 1);
        const auto minX = m_SeedOffset.X - half;
        const auto minY = m_SeedOffset.Y - half;
        const auto maxX = minX + Pow2(m_Depth) - 1;
        const auto maxY = minY + Pow2(m_Depth) - 1;
        return targetPos.X < minX || targetPos.X > maxX || targetPos.Y < minY ||
               targetPos.Y > maxY;
    };

    while (expansionNeeded()) {
        m_Root = ExpandUniverse(m_Root, m_Depth);
        m_Depth++;
    }

    const auto insertLevel = std::min(m_Depth, ViewportMaxLevel);
    const auto [node, offset] = GetCenteredNode(ViewportMaxLevel);
    const auto* centered = SetImpl(node, offset, targetPos, insertLevel, alive);
    m_Root = SetCenteredNode(m_Root, m_Depth, centered, insertLevel);
}

void HashQuadtree::Insert(const HashQuadtree& other, Vec2 offset) {
    if (other.m_Root == FalseNode || other.m_Root->IsEmpty) {
        return;
    }

    const auto sourceLevel = std::min(other.m_Depth, ViewportMaxLevel);
    const auto [sourceNode, sourceOffset] =
        other.GetCenteredNode(ViewportMaxLevel);
    const auto sourceTopLeft =
        Vec2L{sourceOffset.X + offset.X, sourceOffset.Y + offset.Y};
    const auto sourceSize = Pow2(sourceLevel);

    const auto containsSource = [&] {
        const auto insertLevel = std::min(m_Depth, ViewportMaxLevel);
        const auto [node, topLeft] = GetCenteredNode(ViewportMaxLevel);
        const auto size = Pow2(insertLevel);
        return sourceTopLeft.X >= topLeft.X && sourceTopLeft.Y >= topLeft.Y &&
               sourceTopLeft.X + sourceSize <= topLeft.X + size &&
               sourceTopLeft.Y + sourceSize <= topLeft.Y + size;
    };

    while (!containsSource()) {
        m_Root = ExpandUniverse(m_Root, m_Depth);
        m_Depth++;
    }

    const auto insertLevel = std::min(m_Depth, ViewportMaxLevel);
    const auto [centeredRoot, rootTopLeft] = GetCenteredNode(ViewportMaxLevel);
    const auto* centered =
        InsertNodeImpl(centeredRoot, insertLevel, rootTopLeft, sourceNode,
                       sourceLevel, sourceTopLeft);
    m_Root = SetCenteredNode(m_Root, m_Depth, centered, insertLevel);
}

const LifeNode* HashQuadtree::OverlayNodes(const LifeNode* a, const LifeNode* b,
                                           int32_t level) const {
    if (a == FalseNode || a->IsEmpty)
        return b;
    if (b == FalseNode || b->IsEmpty)
        return a;
    if (level == 0)
        return TrueNode;

    return FindOrCreate(OverlayNodes(a->NorthWest, b->NorthWest, level - 1),
                        OverlayNodes(a->NorthEast, b->NorthEast, level - 1),
                        OverlayNodes(a->SouthWest, b->SouthWest, level - 1),
                        OverlayNodes(a->SouthEast, b->SouthEast, level - 1));
}

std::optional<const LifeNode*>
HashQuadtree::TryOverlayAlignedImpl(const LifeNode* destNode, int32_t destLevel,
                                    Vec2L destPos, const LifeNode* srcNode,
                                    int32_t srcLevel, Vec2L srcPos) const {
    if (srcLevel > destLevel) {
        return std::nullopt;
    }

    const auto srcSize = Pow2(srcLevel);
    const auto destSize = Pow2(destLevel);

    if (srcPos.X < destPos.X || srcPos.Y < destPos.Y ||
        srcPos.X + srcSize > destPos.X + destSize ||
        srcPos.Y + srcSize > destPos.Y + destSize) {
        return std::nullopt;
    }

    if (((srcPos.X - destPos.X) % srcSize) != 0 ||
        ((srcPos.Y - destPos.Y) % srcSize) != 0) {
        return std::nullopt;
    }

    if (destLevel == srcLevel) {
        if (destPos != srcPos) {
            return std::nullopt;
        }
        return OverlayNodes(destNode, srcNode, destLevel);
    }

    const auto* source =
        (destNode == FalseNode) ? EmptyTree(destLevel) : destNode;
    const auto half = Pow2(destLevel - 1);
    const auto midX = destPos.X + half;
    const auto midY = destPos.Y + half;

    const bool west = (srcPos.X + srcSize) <= midX;
    const bool east = srcPos.X >= midX;
    const bool north = (srcPos.Y + srcSize) <= midY;
    const bool south = srcPos.Y >= midY;

    if (!(west || east) || !(north || south)) {
        return std::nullopt;
    }

    const auto* nw = source->NorthWest;
    const auto* ne = source->NorthEast;
    const auto* sw = source->SouthWest;
    const auto* se = source->SouthEast;

    if (north && west) {
        const auto overlaid =
            TryOverlayAlignedImpl(nw, destLevel - 1, {destPos.X, destPos.Y},
                                  srcNode, srcLevel, srcPos);
        if (!overlaid)
            return std::nullopt;
        nw = *overlaid;
    } else if (north && east) {
        const auto overlaid = TryOverlayAlignedImpl(
            ne, destLevel - 1, {midX, destPos.Y}, srcNode, srcLevel, srcPos);
        if (!overlaid)
            return std::nullopt;
        ne = *overlaid;
    } else if (south && west) {
        const auto overlaid = TryOverlayAlignedImpl(
            sw, destLevel - 1, {destPos.X, midY}, srcNode, srcLevel, srcPos);
        if (!overlaid)
            return std::nullopt;
        sw = *overlaid;
    } else {
        const auto overlaid = TryOverlayAlignedImpl(
            se, destLevel - 1, {midX, midY}, srcNode, srcLevel, srcPos);
        if (!overlaid)
            return std::nullopt;
        se = *overlaid;
    }

    return FindOrCreate(nw, ne, sw, se);
}

const LifeNode* HashQuadtree::InsertNodeImpl(const LifeNode* destNode,
                                             int32_t destLevel, Vec2L destPos,
                                             const LifeNode* srcNode,
                                             int32_t srcLevel,
                                             Vec2L srcPos) const {
    if (srcNode == FalseNode || srcNode->IsEmpty) {
        return destNode;
    }

    const auto overlaid = TryOverlayAlignedImpl(destNode, destLevel, destPos,
                                                srcNode, srcLevel, srcPos);
    if (overlaid.has_value()) {
        return *overlaid;
    }

    if (srcLevel == 0) {
        return destNode;
    }

    const auto childHalf = Pow2(srcLevel - 1);
    auto* updated = destNode;
    updated = InsertNodeImpl(updated, destLevel, destPos, srcNode->NorthWest,
                             srcLevel - 1, srcPos);
    updated = InsertNodeImpl(updated, destLevel, destPos, srcNode->NorthEast,
                             srcLevel - 1, {srcPos.X + childHalf, srcPos.Y});
    updated = InsertNodeImpl(updated, destLevel, destPos, srcNode->SouthWest,
                             srcLevel - 1, {srcPos.X, srcPos.Y + childHalf});
    updated = InsertNodeImpl(updated, destLevel, destPos, srcNode->SouthEast,
                             srcLevel - 1,
                             {srcPos.X + childHalf, srcPos.Y + childHalf});
    return updated;
}

const LifeNode* HashQuadtree::SetImpl(const LifeNode* node, Vec2L pos,
                                      Vec2 targetPos, int32_t level,
                                      bool alive) {
    if (level == 0) {
        return alive ? TrueNode : FalseNode;
    }

    const auto halfSize = Pow2(level - 1);

    const Vec2L northeastCorner{pos.X + halfSize, pos.Y};
    const Vec2L southwestCorner{pos.X, pos.Y + halfSize};
    const Vec2L southeastCorner{pos.X + halfSize, pos.Y + halfSize};
    const Size2L size{halfSize, halfSize};

    if (RectL{pos, size}.InBounds(targetPos)) {
        return FindOrCreate(
            SetImpl(node->NorthWest, pos, targetPos, level - 1, alive),
            node->NorthEast, node->SouthWest, node->SouthEast);
    } else if (RectL{northeastCorner, size}.InBounds(targetPos)) {
        return FindOrCreate(node->NorthWest,
                            SetImpl(node->NorthEast, northeastCorner, targetPos,
                                    level - 1, alive),
                            node->SouthWest, node->SouthEast);
    } else if (RectL{southwestCorner, size}.InBounds(targetPos)) {
        return FindOrCreate(node->NorthWest, node->NorthEast,
                            SetImpl(node->SouthWest, southwestCorner, targetPos,
                                    level - 1, alive),
                            node->SouthEast);
    } else {
        return FindOrCreate(node->NorthWest, node->NorthEast, node->SouthWest,
                            SetImpl(node->SouthEast, southeastCorner, targetPos,
                                    level - 1, alive));
    }
}

void HashQuadtree::Clear(Rect region) {
    const auto [node, offset] = GetCenteredNode(ViewportMaxLevel);
    const auto* centered =
        ClearImpl(node, offset, region, std::min(m_Depth, ViewportMaxLevel));
    m_Root = SetCenteredNode(m_Root, m_Depth, centered, ViewportMaxLevel);
}

const LifeNode* HashQuadtree::ClearImpl(const LifeNode* node, Vec2L pos,
                                        Rect region, int32_t level) {
    if (node == FalseNode || node->IsEmpty) {
        return node;
    }

    const auto size = Pow2(level);
    const RectL nodeRect{pos.X, pos.Y, size, size};

    // If this node is fully contained, wipe it entirely
    if (region.Contains(nodeRect)) {
        return EmptyTree(level);
    }

    // If there's no overlap at all, nothing to do
    if (!region.Intersects(nodeRect)) {
        return node;
    }

    if (level == 0) {
        return FalseNode;
    }

    const auto halfSize = Pow2(level - 1);
    const auto* nw = node->NorthWest;
    const auto* ne = node->NorthEast;
    const auto* sw = node->SouthWest;
    const auto* se = node->SouthEast;

    return FindOrCreate(
        ClearImpl(nw, {pos.X, pos.Y}, region, level - 1),
        ClearImpl(ne, {pos.X + halfSize, pos.Y}, region, level - 1),
        ClearImpl(sw, {pos.X, pos.Y + halfSize}, region, level - 1),
        ClearImpl(se, {pos.X + halfSize, pos.Y + halfSize}, region, level - 1));
}

const LifeNode* HashQuadtree::ExtractImpl(const LifeNode* node, Vec2L pos,
                                          Rect region, int32_t level) const {
    if (node == FalseNode || node->IsEmpty) {
        return EmptyTree(level);
    }

    const auto size = Pow2(level);
    const bool unconstrainedX = (region.Width == 0);
    const bool unconstrainedY = (region.Height == 0);

    // No overlap — return empty
    const bool overlapX =
        unconstrainedX ||
        (pos.X < static_cast<int64_t>(region.X) + region.Width &&
         pos.X + size > static_cast<int64_t>(region.X));
    const bool overlapY =
        unconstrainedY ||
        (pos.Y < static_cast<int64_t>(region.Y) + region.Height &&
         pos.Y + size > static_cast<int64_t>(region.Y));
    if (!overlapX || !overlapY) {
        return EmptyTree(level);
    }

    // Fully contained on each axis independently
    const bool containedX =
        unconstrainedX ||
        (pos.X >= static_cast<int64_t>(region.X) &&
         pos.X + size <= static_cast<int64_t>(region.X) + region.Width);
    const bool containedY =
        unconstrainedY ||
        (pos.Y >= static_cast<int64_t>(region.Y) &&
         pos.Y + size <= static_cast<int64_t>(region.Y) + region.Height);

    if (containedX && containedY) {
        return node;
    }

    if (level == 0) {
        return node;
    }

    const auto half = Pow2(level - 1);

    return FindOrCreate(
        ExtractImpl(node->NorthWest, {pos.X, pos.Y}, region, level - 1),
        ExtractImpl(node->NorthEast, {pos.X + half, pos.Y}, region, level - 1),
        ExtractImpl(node->SouthWest, {pos.X, pos.Y + half}, region, level - 1),
        ExtractImpl(node->SouthEast, {pos.X + half, pos.Y + half}, region,
                    level - 1));
}

HashQuadtree HashQuadtree::Extract(Rect region) const {
    HashQuadtree result{};
    result.m_SeedOffset = m_SeedOffset - Vec2L{region.X, region.Y};

    if (m_Root == FalseNode)
        return result;

    const auto [node, offset] = GetCenteredNode(ViewportMaxLevel);
    result.m_Root =
        ExtractImpl(node, offset, region, std::min(m_Depth, ViewportMaxLevel));
    result.m_Depth = std::min(m_Depth, ViewportMaxLevel);
    return result;
}

static int64_t FindExtentImpl(const LifeNode* node, Vec2L pos, int32_t level,
                              bool returnX, bool findLeast) {
    constexpr static auto hasLiveCells = [](const LifeNode* n) {
        return n != FalseNode && !n->IsEmpty;
    };

    if (!hasLiveCells(node)) {
        return returnX ? pos.X : pos.Y;
    }

    if (level == 0)
        return returnX ? pos.X : pos.Y;

    const auto half = Pow2(level - 1);

    const auto* nw = node->NorthWest;
    const auto* ne = node->NorthEast;
    const auto* sw = node->SouthWest;
    const auto* se = node->SouthEast;

    const auto reduce = [&](int64_t a, int64_t b) {
        return findLeast ? std::min(a, b) : std::max(a, b);
    };

    const auto recurseNW = [&] {
        return FindExtentImpl(nw, {pos.X, pos.Y}, level - 1, returnX,
                              findLeast);
    };
    const auto recurseNE = [&] {
        return FindExtentImpl(ne, {pos.X + half, pos.Y}, level - 1, returnX,
                              findLeast);
    };
    const auto recurseSW = [&] {
        return FindExtentImpl(sw, {pos.X, pos.Y + half}, level - 1, returnX,
                              findLeast);
    };
    const auto recurseSE = [&] {
        return FindExtentImpl(se, {pos.X + half, pos.Y + half}, level - 1,
                              returnX, findLeast);
    };

    // Greedy side selection: prefer the half that can contain the target
    // extreme, then resolve between the two quadrants in that half.
    if (returnX) {
        if (findLeast) {
            const bool hasWest = hasLiveCells(nw) || hasLiveCells(sw);
            if (hasWest) {
                if (hasLiveCells(nw) && hasLiveCells(sw))
                    return reduce(recurseNW(), recurseSW());
                return hasLiveCells(nw) ? recurseNW() : recurseSW();
            }
            if (hasLiveCells(ne) && hasLiveCells(se))
                return reduce(recurseNE(), recurseSE());
            return hasLiveCells(ne) ? recurseNE() : recurseSE();
        }

        const bool hasEast = hasLiveCells(ne) || hasLiveCells(se);
        if (hasEast) {
            if (hasLiveCells(ne) && hasLiveCells(se))
                return reduce(recurseNE(), recurseSE());
            return hasLiveCells(ne) ? recurseNE() : recurseSE();
        }
        if (hasLiveCells(nw) && hasLiveCells(sw))
            return reduce(recurseNW(), recurseSW());
        return hasLiveCells(nw) ? recurseNW() : recurseSW();
    }

    if (findLeast) {
        const bool hasNorth = hasLiveCells(nw) || hasLiveCells(ne);
        if (hasNorth) {
            if (hasLiveCells(nw) && hasLiveCells(ne))
                return reduce(recurseNW(), recurseNE());
            return hasLiveCells(nw) ? recurseNW() : recurseNE();
        }
        if (hasLiveCells(sw) && hasLiveCells(se))
            return reduce(recurseSW(), recurseSE());
        return hasLiveCells(sw) ? recurseSW() : recurseSE();
    }

    const bool hasSouth = hasLiveCells(sw) || hasLiveCells(se);
    if (hasSouth) {
        if (hasLiveCells(sw) && hasLiveCells(se))
            return reduce(recurseSW(), recurseSE());
        return hasLiveCells(sw) ? recurseSW() : recurseSE();
    }
    if (hasLiveCells(nw) && hasLiveCells(ne))
        return reduce(recurseNW(), recurseNE());
    return hasLiveCells(nw) ? recurseNW() : recurseNE();
}

Rect HashQuadtree::FindBoundingBox() const {
    if (m_Root == FalseNode || m_Root->IsEmpty)
        return {0, 0, 0, 0};

    const auto [node, offset] = GetCenteredNode(ViewportMaxLevel);
    const auto level = std::min(m_Depth, ViewportMaxLevel);

    const auto minX = FindExtentImpl(node, offset, level, true, true);
    const auto minY = FindExtentImpl(node, offset, level, false, true);
    const auto maxX = FindExtentImpl(node, offset, level, true, false);
    const auto maxY = FindExtentImpl(node, offset, level, false, false);

    constexpr static auto clampToInt32 = [](int64_t num) {
        return static_cast<int32_t>(
            std::clamp(num, int64_t{std::numeric_limits<int32_t>::min()},
                       int64_t{std::numeric_limits<int32_t>::max()}));
    };

    return {clampToInt32(minX), clampToInt32(minY),
            clampToInt32(maxX - minX + 1), clampToInt32(maxY - minY + 1)};
}

bool HashQuadtree::Get(Vec2 targetPos) const {
    const auto [node, offset] = GetCenteredNode(ViewportMaxLevel);
    return GetImpl(node, offset, targetPos,
                   std::min(m_Depth, ViewportMaxLevel));
}

bool HashQuadtree::GetImpl(const LifeNode* node, Vec2L pos, Vec2 targetPos,
                           int32_t level) const {
    if (level == 0) {
        return node == TrueNode;
    }

    if (node->IsEmpty) {
        return false;
    }

    const auto halfSize = Pow2(level - 1);

    const Vec2L northeastCorner{pos.X + halfSize, pos.Y};
    const Vec2L southwestCorner{pos.X, pos.Y + halfSize};
    const Vec2L southeastCorner{pos.X + halfSize, pos.Y + halfSize};
    const Size2L quadrantSize{halfSize, halfSize};

    if (RectL{pos, quadrantSize}.InBounds(targetPos)) {
        return GetImpl(node->NorthWest, pos, targetPos, level - 1);
    } else if (RectL{northeastCorner, quadrantSize}.InBounds(targetPos)) {
        return GetImpl(node->NorthEast, northeastCorner, targetPos, level - 1);
    } else if (RectL{southwestCorner, quadrantSize}.InBounds(targetPos)) {
        return GetImpl(node->SouthWest, southwestCorner, targetPos, level - 1);
    } else if (RectL{southeastCorner, quadrantSize}.InBounds(targetPos)) {
        return GetImpl(node->SouthEast, southeastCorner, targetPos, level - 1);
    } else {
        return false;
    }
}

int64_t HashQuadtree::CalculateTreeSize() const {
    if (m_Root == FalseNode) {
        return 0;
    }
    return Pow2(m_Depth);
}

bool HashQuadtree::empty() const { return m_Root == EmptyTree(m_Depth); }

BigInt HashQuadtree::PopulationOf(const LifeNode* node) {
    if (node == FalseNode) {
        return BigZero;
    }
    if (node == TrueNode) {
        return BigOne;
    }

    if (auto it = s_Cache.PopulationCache.find(node);
        it != s_Cache.PopulationCache.end()) {
        return it->second;
    }

    // 4. Insert and return a copy
    return s_Cache.PopulationCache[node] =
               PopulationOf(node->NorthWest) + PopulationOf(node->NorthEast) +
               PopulationOf(node->SouthWest) + PopulationOf(node->SouthEast);
}

int64_t HashQuadtree::PopulationOf(const LifeNode* node, bool) {
    if (node == FalseNode) {
        return int64_t{0};
    }
    if (node == TrueNode) {
        return int64_t{1};
    }

    if (auto it = s_Cache.SmallPopulationCache.find(node);
        it != s_Cache.SmallPopulationCache.end()) {
        return it->second;
    }

    // 4. Insert and return a copy
    return s_Cache.SmallPopulationCache[node] =
               PopulationOf(node->NorthWest, false) +
               PopulationOf(node->NorthEast, false) +
               PopulationOf(node->SouthWest, false) +
               PopulationOf(node->SouthEast, false);
}

HashQuadtree::CenteredNodeResult
HashQuadtree::GetCenteredNode(int32_t level) const {
    if (m_Depth <= level) {
        // Tree already fits, return root directly at its own offset
        const auto half = (m_Depth == 0 ? 0 : Pow2(m_Depth - 1));
        return {.Node = m_Root,
                .Offset = {m_SeedOffset.X - half, m_SeedOffset.Y - half}};
    }

    const LifeNode* northwest = m_Root->NorthWest;
    const LifeNode* northeast = m_Root->NorthEast;
    const LifeNode* southwest = m_Root->SouthWest;
    const LifeNode* southeast = m_Root->SouthEast;
    for (auto i = m_Depth; i > level; i--) {
        northwest = northwest->SouthEast;
        northeast = northeast->SouthWest;
        southwest = southwest->NorthEast;
        southeast = southeast->NorthWest;
    }

    const auto size = Pow2(level - 1);
    return {.Node = FindOrCreate(northwest, northeast, southwest, southeast),
            .Offset = {m_SeedOffset.X - size, m_SeedOffset.Y - size}};
}

const LifeNode* HashQuadtree::ReplaceAlongPath(const LifeNode* node,
                                               int32_t level, Quadrant quadrant,
                                               const LifeNode* value,
                                               int32_t targetLevel) const {
    if (level <= targetLevel) {
        return value;
    }

    const auto* source = (node == FalseNode) ? EmptyTree(level) : node;

    const auto* nw = source->NorthWest;
    const auto* ne = source->NorthEast;
    const auto* sw = source->SouthWest;
    const auto* se = source->SouthEast;

    switch (quadrant) {
    case Quadrant::NW:
        nw = ReplaceAlongPath(nw, level - 1, quadrant, value, targetLevel);
        break;
    case Quadrant::NE:
        ne = ReplaceAlongPath(ne, level - 1, quadrant, value, targetLevel);
        break;
    case Quadrant::SW:
        sw = ReplaceAlongPath(sw, level - 1, quadrant, value, targetLevel);
        break;
    case Quadrant::SE:
        se = ReplaceAlongPath(se, level - 1, quadrant, value, targetLevel);
        break;
    }

    return FindOrCreate(nw, ne, sw, se);
}

const LifeNode* HashQuadtree::SetCenteredNode(const LifeNode* outer,
                                              int32_t outerLevel,
                                              const LifeNode* toInsert,
                                              int32_t insertLevel) const {
    if (outer == FalseNode) {
        outer = EmptyTree(outerLevel);
    }

    if (outerLevel <= insertLevel) {
        return toInsert;
    }

    // Undefined for inserting a single leaf into a larger even-sized node.
    if (insertLevel == 0) {
        return outer;
    }

    const auto childTargetLevel = insertLevel - 1;

    const auto* nw =
        ReplaceAlongPath(outer->NorthWest, outerLevel - 1, Quadrant::SE,
                         toInsert->NorthWest, childTargetLevel);
    const auto* ne =
        ReplaceAlongPath(outer->NorthEast, outerLevel - 1, Quadrant::SW,
                         toInsert->NorthEast, childTargetLevel);
    const auto* sw =
        ReplaceAlongPath(outer->SouthWest, outerLevel - 1, Quadrant::NE,
                         toInsert->SouthWest, childTargetLevel);
    const auto* se =
        ReplaceAlongPath(outer->SouthEast, outerLevel - 1, Quadrant::NW,
                         toInsert->SouthEast, childTargetLevel);

    return FindOrCreate(nw, ne, sw, se);
}

BigInt HashQuadtree::Population() const { return PopulationOf(m_Root); }

HashQuadtree::Iterator HashQuadtree::begin() const {
    if (m_Root == FalseNode) {
        return end();
    }

    const auto [node, offset] = GetCenteredNode(ViewportMaxLevel);
    return Iterator{node, offset, std::min(m_Depth, ViewportMaxLevel), false,
                    nullptr};
}

HashQuadtree::Iterator HashQuadtree::end() const { return Iterator{}; }

HashQuadtree::Iterator HashQuadtree::begin(Rect bounds) const {
    if (m_Root == FalseNode)
        return end();

    const auto [node, offset] = GetCenteredNode(ViewportMaxLevel);
    return Iterator{node, offset, std::min(m_Depth, ViewportMaxLevel), false,
                    &bounds};
}

const LifeNode* HashQuadtree::FindOrCreate(const LifeNode* nw,
                                           const LifeNode* ne,
                                           const LifeNode* sw,
                                           const LifeNode* se) const {
    LifeNodeKey key{nw, ne, sw, se};
    if (const auto itr = s_Cache.NodeMap.find(key);
        itr != s_Cache.NodeMap.end()) {
        return itr->first;
    }

    auto* node = s_Cache.NodeStorage.emplace(nw, ne, sw, se);
    s_Cache.NodeMap[node] = nullptr;
    return node;
}

std::optional<const LifeNode*> HashQuadtree::Find(const LifeNode* node) const {
    auto it = s_Cache.NodeMap.find(node);
    if (it == s_Cache.NodeMap.end() || it->second == nullptr) {
        return std::nullopt;
    }
    return it->second;
}

void HashQuadtree::CacheResult(const LifeNode* key,
                               const LifeNode* value) const {
    s_Cache.NodeMap[key] = value;
}

void HashQuadtree::ClearCache() const { s_Cache.NodeMap.clear(); }

const LifeNode* HashQuadtree::ExpandUniverse(const LifeNode* node,
                                             int32_t level) const {
    if (node == FalseNode)
        return EmptyTree(level + 1);

    if (node == TrueNode)
        return FindOrCreate(FalseNode, FalseNode, FalseNode, TrueNode);

    const auto* empty = level > 0 ? EmptyTree(level - 1) : FalseNode;
    const auto* expandedNW = FindOrCreate(empty, empty, empty, node->NorthWest);
    const auto* expandedNE = FindOrCreate(empty, empty, node->NorthEast, empty);
    const auto* expandedSW = FindOrCreate(empty, node->SouthWest, empty, empty);
    const auto* expandedSE = FindOrCreate(node->SouthEast, empty, empty, empty);

    return FindOrCreate(expandedNW, expandedNE, expandedSW, expandedSE);
}

const LifeNode* HashQuadtree::EmptyTree(int32_t level) const {
    if (level <= 0) {
        return FalseNode;
    }

    if (level >= static_cast<int32_t>(s_Cache.EmptyNodeCache.size())) {
        s_Cache.EmptyNodeCache.resize(level + 1, nullptr);
    } else if (s_Cache.EmptyNodeCache[level] != nullptr) {
        return s_Cache.EmptyNodeCache[level];
    }

    const auto* child = EmptyTree(level - 1);
    const auto* result = FindOrCreate(child, child, child, child);
    s_Cache.EmptyNodeCache[level] = result;
    return result;
}

const LifeNode* HashQuadtree::BuildTreeRegion(std::span<Vec2L> cells, Vec2L pos,
                                              int32_t level) {
    if (cells.empty())
        return EmptyTree(level);

    if (level == 0)
        return TrueNode;

    const auto half = Pow2(level - 1);
    const auto midY = pos.Y + half;
    const auto midX = pos.X + half;

    using std::ranges::partition;

    const auto itY =
        partition(cells, [midY](Vec2L v) { return v.Y < midY; }).begin();
    const std::span<Vec2L> north{cells.begin(), itY};
    const std::span<Vec2L> south{itY, cells.end()};

    const auto itNorthX =
        partition(north, [midX](Vec2L v) { return v.X < midX; }).begin();
    const auto itSouthX =
        partition(south, [midX](Vec2L v) { return v.X < midX; }).begin();

    return FindOrCreate(
        BuildTreeRegion({north.begin(), itNorthX}, {pos.X, pos.Y}, level - 1),
        BuildTreeRegion({itNorthX, north.end()}, {pos.X + half, pos.Y},
                        level - 1),
        BuildTreeRegion({south.begin(), itSouthX}, {pos.X, pos.Y + half},
                        level - 1),
        BuildTreeRegion({itSouthX, south.end()}, {pos.X + half, pos.Y + half},
                        level - 1));
}

const LifeNode* HashQuadtree::BuildTree(std::span<const Vec2> cells) {
    if (cells.empty()) {
        m_SeedOffset = {0, 0};
        return FalseNode;
    }

    // Single pass to find bounding box
    auto minXv = std::numeric_limits<int32_t>::max();
    auto maxXv = std::numeric_limits<int32_t>::min();
    auto minYv = std::numeric_limits<int32_t>::max();
    auto maxYv = std::numeric_limits<int32_t>::min();
    for (const auto cell : cells) {
        if (cell.X < minXv)
            minXv = cell.X;
        if (cell.X > maxXv)
            maxXv = cell.X;
        if (cell.Y < minYv)
            minYv = cell.Y;
        if (cell.Y > maxYv)
            maxYv = cell.Y;
    }

    const auto neighborhoodSize =
        std::max(static_cast<int64_t>(maxXv) - minXv,
                 static_cast<int64_t>(maxYv) - minYv) +
        1;
    const auto gridExponent =
        static_cast<int32_t>(std::ceil(std::log2(neighborhoodSize)));

    const Vec2L offset{static_cast<int64_t>(minXv),
                       static_cast<int64_t>(minYv)};

    m_SeedOffset = {offset.X + (gridExponent == 0 ? 0 : Pow2(gridExponent - 1)),
                    offset.Y +
                        (gridExponent == 0 ? 0 : Pow2(gridExponent - 1))};

    m_Depth = gridExponent;

    // Promote cells to int64_t types
    std::vector<Vec2L> cellVec{};
    cellVec.reserve(cells.size());
    for (const auto cell : cells)
        cellVec.emplace_back(static_cast<int64_t>(cell.X),
                             static_cast<int64_t>(cell.Y));

    const auto* result = BuildTreeRegion(cellVec, offset, gridExponent);
    EmptyTree(ViewportMaxLevel);
    return result;
}
} // namespace gol
