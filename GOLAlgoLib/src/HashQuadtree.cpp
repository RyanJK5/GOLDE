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

namespace gol {
// Returns the exponent of the greatest power of two less than `stepSize`.
constexpr static int32_t Log2MaxAdvanceOf(const BigInt& stepSize) {
    if (stepSize.is_zero())
        return 0;

    return static_cast<int32_t>(boost::multiprecision::msb(stepSize));
}

constexpr BigInt BigPow2(int32_t exponent) { return BigOne << exponent; }

HashLifeCache::HashLifeCache() {
    NodeMap.reserve(
        1 << 20); // Reserve space for 1 million nodes to avoid rehashing
                  // during early stages of the simulation.
    SlowCache.reserve(1 << 20);
    EmptyNodeCache.resize(64, nullptr);
}

thread_local HashLifeCache HashQuadtree::s_Cache{};

namespace {
// ============================================================================
// Base case infrastructure for the 8x8 HashLife leaf computation.
//
// Bit layout for a 4x4 grid stored as a 16-bit value:
//   Row 0 (top):    bits 15, 14, 13, 12
//   Row 1:          bits 11, 10,  9,  8
//   Row 2:          bits  7,  6,  5,  4
//   Row 3 (bottom): bits  3,  2,  1,  0
//
// The 2x2 quadrants within this layout:
//   NW = bits {15,14,11,10} = 0xCC00
//   NE = bits {13,12, 9, 8} = 0x3300
//   SW = bits { 7, 6, 3, 2} = 0x00CC
//   SE = bits { 5, 4, 1, 0} = 0x0033
// ============================================================================

// Total number of possible 4x4 cell configurations.
constexpr uint32_t NumLeafPatterns = 65536;

// Bitmasks for extracting 2x2 quadrants from a 16-bit 4x4 grid.
constexpr uint16_t MaskNW = 0xCC00;
constexpr uint16_t MaskNE = 0x3300;
constexpr uint16_t MaskSW = 0x00CC;
constexpr uint16_t MaskSE = 0x0033;

// Returns the bit position (0-15) for a cell at (col, row) in a 4x4 grid.
constexpr int BitPosition(int col, int row) {
    return (3 - row) * 4 + (3 - col);
}

// Precomputed lookup table for Conway's Game of Life (B3/S23).
// Maps each 16-bit 4x4 pattern to the next-generation state of the center 2x2,
// encoded in bits {5, 4, 1, 0} (the SE quadrant positions). This encoding
// allows the results of 9 overlapping lookups to be efficiently assembled
// via shifts into a full 4x4 result.
auto BuildRuleTable() {
    // The four center cells of a 4x4 grid whose next state we compute.
    constexpr std::array centerCol{1, 2, 1, 2};
    constexpr std::array centerRow{1, 1, 2, 2};
    // Where each center cell's result is placed in the output.
    constexpr std::array resultBit{5, 4, 1, 0};

    std::array<uint16_t, NumLeafPatterns> table{};
    for (uint32_t pattern = 0; pattern < NumLeafPatterns; ++pattern) {
        uint16_t result = 0;
        for (int cell = 0; cell < 4; ++cell) {
            const int col = centerCol[cell];
            const int row = centerRow[cell];
            int neighborCount = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0)
                        continue;
                    neighborCount += static_cast<int>(
                        (pattern >> BitPosition(col + dx, row + dy)) & 1);
                }
            }
            const bool isAlive = ((pattern >> BitPosition(col, row)) & 1) != 0;
            const bool survives = isAlive && neighborCount == 2;
            const bool born = neighborCount == 3;
            if (survives || born)
                result |= static_cast<uint16_t>(1 << resultBit[cell]);
        }
        table[pattern] = result;
    }
    return table;
}

const auto RuleTable = BuildRuleTable();

// Directly encodes a level-1 quadrant's cells into the known bit positions
// for each 2x2 sub-quadrant of the 4x4 grid.
uint16_t EncodeQuadrantNW(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 15);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 14);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 11);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 10);
    return bits;
}
uint16_t EncodeQuadrantNE(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 13);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 12);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 9);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 8);
    return bits;
}
uint16_t EncodeQuadrantSW(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 7);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 6);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 3);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 2);
    return bits;
}
uint16_t EncodeQuadrantSE(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 5);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 4);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 1);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 0);
    return bits;
}

// Encodes a level-2 node (4x4 grid of leaf cells) as a 16-bit value.
uint16_t EncodeLevel2(const LifeNode* node) {
    if (node == FalseNode ||
        (node->NorthEast == FalseNode && node->NorthWest == FalseNode &&
         node->SouthEast == FalseNode && node->SouthWest == FalseNode))
        return 0;
    return EncodeQuadrantNW(node->NorthWest) |
           EncodeQuadrantNE(node->NorthEast) |
           EncodeQuadrantSW(node->SouthWest) |
           EncodeQuadrantSE(node->SouthEast);
}

// Converts a single bit to a leaf cell pointer.
const LifeNode* BitToCell(uint16_t bits, int position) {
    return ((bits >> position) & 1) != 0 ? TrueNode : FalseNode;
}

// Decodes a 16-bit 4x4 value back to a level-2 LifeNode.
// The result is a node with four level-1 quadrant children, each
// containing four leaf cells.
template <typename FindOrCreateFn>
const LifeNode* DecodeLevel2(uint16_t bits, FindOrCreateFn&& findOrCreate) {
    const auto* quadrantNW =
        findOrCreate(BitToCell(bits, 15), BitToCell(bits, 14),
                     BitToCell(bits, 11), BitToCell(bits, 10));
    const auto* quadrantNE =
        findOrCreate(BitToCell(bits, 13), BitToCell(bits, 12),
                     BitToCell(bits, 9), BitToCell(bits, 8));
    const auto* quadrantSW =
        findOrCreate(BitToCell(bits, 7), BitToCell(bits, 6), BitToCell(bits, 3),
                     BitToCell(bits, 2));
    const auto* quadrantSE =
        findOrCreate(BitToCell(bits, 5), BitToCell(bits, 4), BitToCell(bits, 1),
                     BitToCell(bits, 0));
    return findOrCreate(quadrantNW, quadrantNE, quadrantSW, quadrantSE);
}
} // namespace

// Free function form of calling HashQuadtree::Advance, with the added bonus of
// supporting an exact `stepCount` rather than just a `maxAdvance`.
BigInt HashLife(HashQuadtree& data, const BigInt& numSteps,
                std::stop_token stopToken) {
    if (numSteps.is_zero()) // Hyper speed
        return BigPow2(data.Advance(-1, stopToken));

    BigInt generation{};
    while (generation < numSteps) {
        const auto advanceLevel = Log2MaxAdvanceOf(numSteps - generation);

        const auto gens = BigPow2(data.Advance(advanceLevel, stopToken));
        if (stopToken.stop_requested())
            return generation;
        generation += gens;
    }

    return generation;
}

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

// Generic form of HashQuadtree::FindOrCreate that we can also use
// for building a transfer cache.
static const LifeNode* FindOrCreateFromCache(HashLifeCache& cache,
                                             const LifeNode* nw,
                                             const LifeNode* ne,
                                             const LifeNode* sw,
                                             const LifeNode* se) {
    LifeNodeKey key{nw, ne, sw, se};
    if (const auto itr = cache.NodeMap.find(key); itr != cache.NodeMap.end()) {
        return itr->first;
    }

    auto* node = cache.NodeStorage.emplace(nw, ne, sw, se);
    cache.NodeMap[node] = nullptr;
    return node;
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

    const auto insertLevel = std::min(m_Depth, 32);
    const auto [node, offset] = GetCenteredNode(32);
    const auto* centered = SetImpl(node, offset, targetPos, insertLevel, alive);
    m_Root = SetCenteredNode(m_Root, m_Depth, centered, insertLevel);
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
    const auto [node, offset] = GetCenteredNode(32);
    const auto* centered =
        ClearImpl(node, offset, region, std::min(m_Depth, 32));
    m_Root = SetCenteredNode(m_Root, m_Depth, centered, 32);
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

HashQuadtree HashQuadtree::Extract(Rect region) const {
    HashQuadtree result{};
    result.m_SeedOffset = m_SeedOffset - Vec2L{region.X, region.Y};

    if (m_Root == FalseNode)
        return result;

    const auto [node, offset] = GetCenteredNode(32);
    result.m_Root = ExtractImpl(node, offset, region, std::min(m_Depth, 32));
    result.m_Depth = std::min(m_Depth, 32);
    return result;
}

const LifeNode* HashQuadtree::ExtractImpl(const LifeNode* node, Vec2L pos,
                                          Rect region, int32_t level) const {
    if (node == FalseNode || node->IsEmpty) {
        return EmptyTree(level);
    }

    const auto size = Pow2(level);
    const RectL nodeRect{pos.X, pos.Y, size, size};

    // No overlap — return empty
    if (!region.Intersects(nodeRect)) {
        return EmptyTree(level);
    }

    // Fully contained — keep as-is
    if (region.Contains(nodeRect)) {
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

bool HashQuadtree::Get(Vec2 targetPos) const {
    const auto [node, offset] = GetCenteredNode(32);
    return GetImpl(node, offset, targetPos, std::min(m_Depth, 32));
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

    const auto [node, offset] = GetCenteredNode(32);
    return Iterator{node, offset, std::min(m_Depth, 32), false, nullptr};
}

HashQuadtree::Iterator HashQuadtree::end() const { return Iterator{}; }

HashQuadtree::Iterator HashQuadtree::begin(Rect bounds) const {
    if (m_Root == FalseNode)
        return end();

    const auto [node, offset] = GetCenteredNode(32);
    return Iterator{node, offset, std::min(m_Depth, 32), false, &bounds};
}

const LifeNode* HashQuadtree::FindOrCreate(const LifeNode* nw,
                                           const LifeNode* ne,
                                           const LifeNode* sw,
                                           const LifeNode* se) {
    // Defer to more generic function
    return FindOrCreateFromCache(s_Cache, nw, ne, sw, se);
}

const LifeNode* HashQuadtree::CenteredHorizontal(const LifeNode& west,
                                                 const LifeNode& east) const {
    // Imagine these two nodes:
    // west:  east:
    // -----  -----
    // |A|B|  |E|F|
    // -----  -----
    // |C|D|  |G|H|
    // -----  -----
    // We return:
    // -----
    // |B|E|
    // -----
    // |D|G|
    // -----
    return FindOrCreate(west.NorthEast, east.NorthWest, west.SouthEast,
                        east.SouthWest);
}

const LifeNode* HashQuadtree::CenteredVertical(const LifeNode& north,
                                               const LifeNode& south) const {
    // Imagine these two nodes:
    // north: south:
    // -----  -----
    // |A|B|  |E|F|
    // -----  -----
    // |C|D|  |G|H|
    // -----  -----
    // We return:
    // -----
    // |C|D|
    // -----
    // |E|F|
    // -----

    return FindOrCreate(north.SouthWest, north.SouthEast, south.NorthWest,
                        south.NorthEast);
}

const LifeNode* HashQuadtree::CenteredSubNode(const LifeNode& node) const {
    // Imagine this node:
    // ---------
    // |A|B|C|D|
    // ---------
    // |E|F|G|H|
    // ---------
    // |I|J|K|L|
    // ---------
    // |M|N|O|P|
    // ---------
    // We return:
    // -----
    // |F|G|
    // -----
    // |J|K|
    // -----
    return FindOrCreate(node.NorthWest->SouthEast, node.NorthEast->SouthWest,
                        node.SouthWest->NorthEast, node.SouthEast->NorthWest);
}

// Extracts the four 16-bit quadrant encodings from a level-3 node.
struct LeafQuadrants {
    uint16_t nw, ne, sw, se;
};

static LeafQuadrants EncodeLevel3(const LifeNode* node) {
    return {EncodeLevel2(node->NorthWest), EncodeLevel2(node->NorthEast),
            EncodeLevel2(node->SouthWest), EncodeLevel2(node->SouthEast)};
}

// The 8x8 grid formed by 4 quadrants can be sliced into 9 overlapping 4x4
// windows. Each window consists of bits extracted and shifted from adjacent
// quadrants. These helpers construct the indices into the rule table.
//
// Window layout (each window is centered on the grid intersection shown):
//   NW   N    NE
//    W   C    E
//   SW   S    SE
//
// The bitwise shifts correspond to how Golly's `leafres` extracts overlapping
// 4x4 regions from the four 4x4 quadrant values.

static uint16_t WindowN(uint16_t nw, uint16_t ne) {
    return ((nw << 2) & 0xCCCC) | ((ne >> 2) & 0x3333);
}
static uint16_t WindowW(uint16_t nw, uint16_t sw) {
    return ((nw << 8) & 0xFF00) | ((sw >> 8) & 0x00FF);
}
static uint16_t WindowE(uint16_t ne, uint16_t se) {
    return ((ne << 8) & 0xFF00) | ((se >> 8) & 0x00FF);
}
static uint16_t WindowS(uint16_t sw, uint16_t se) {
    return ((sw << 2) & 0xCCCC) | ((se >> 2) & 0x3333);
}
static uint16_t WindowCenter(uint16_t nw, uint16_t ne, uint16_t sw,
                             uint16_t se) {
    return ((nw << 10) & MaskNW) | ((ne << 6) & MaskNE) | ((sw >> 6) & MaskSW) |
           ((se >> 10) & MaskSE);
}

// Computes the 9 first-generation 2x2 results from the overlapping 4x4 windows
// of an 8x8 grid. Each result uses the SE-quadrant bit encoding {5,4,1,0}.
struct FirstGenResults {
    uint16_t nw, n, ne, w, center, e, sw, s, se;
};

static FirstGenResults ComputeFirstGeneration(const LeafQuadrants& q) {
    return {
        RuleTable[q.nw],
        RuleTable[WindowN(q.nw, q.ne)],
        RuleTable[q.ne],
        RuleTable[WindowW(q.nw, q.sw)],
        RuleTable[WindowCenter(q.nw, q.ne, q.sw, q.se)],
        RuleTable[WindowE(q.ne, q.se)],
        RuleTable[q.sw],
        RuleTable[WindowS(q.sw, q.se)],
        RuleTable[q.se],
    };
}

// Assembles a 4x4 result (16 bits) from four 2x2 sub-results.
// Each sub-result occupies the SE-quadrant bits {5,4,1,0} and is shifted
// into its respective quadrant position.
static uint16_t AssembleQuadrants(uint16_t resultNW, uint16_t resultNE,
                                  uint16_t resultSW, uint16_t resultSE) {
    return static_cast<uint16_t>(
        ((resultNW << 10) & MaskNW) | ((resultNE << 8) & MaskNE) |
        ((resultSW << 2) & MaskSW) | (resultSE & MaskSE));
}

// Combines four 2x2 results (in SE-quadrant encoding) into a 16-bit index
// for a second rule table lookup.
static uint16_t Combine2x2ForLookup(uint16_t topLeft, uint16_t topRight,
                                    uint16_t bottomLeft, uint16_t bottomRight) {
    return static_cast<uint16_t>((topLeft << 10) | (topRight << 8) |
                                 (bottomLeft << 2) | bottomRight);
}

// Assembles the 6x6 one-generation result (as a centered 4x4) from the
// nine 2x2 sub-results. Each sub-result encodes a 2x2 in bits {5,4,1,0}.
// This is the "combine9" operation from Golly.
static uint16_t AssembleCentered6x6(const FirstGenResults& gen1) {
    return static_cast<uint16_t>(
        (gen1.nw << 15) | (gen1.n << 13) | ((gen1.ne << 11) & 0x1000) |
        ((gen1.w << 7) & 0x0880) | (gen1.center << 5) |
        ((gen1.e << 3) & 0x0110) | ((gen1.sw >> 1) & 0x0008) | (gen1.s >> 3) |
        (gen1.se >> 5));
}

// 8x8 base case for HashLife. Advances a level-3 node by 2 generations,
// returning a level-2 node (center 4x4).
const LifeNode* HashQuadtree::AdvanceBase(const LifeNode* node) const {
    const auto quadrants = EncodeLevel3(node);
    const auto gen1 = ComputeFirstGeneration(quadrants);

    // Second generation: combine adjacent 2x2 results into four overlapping
    // 4x4 windows, look up each to get a 2x2 result, then assemble.
    const auto secondGenNW =
        RuleTable[Combine2x2ForLookup(gen1.nw, gen1.n, gen1.w, gen1.center)];
    const auto secondGenNE =
        RuleTable[Combine2x2ForLookup(gen1.n, gen1.ne, gen1.center, gen1.e)];
    const auto secondGenSW =
        RuleTable[Combine2x2ForLookup(gen1.w, gen1.center, gen1.sw, gen1.s)];
    const auto secondGenSE =
        RuleTable[Combine2x2ForLookup(gen1.center, gen1.e, gen1.s, gen1.se)];

    const auto resultBits =
        AssembleQuadrants(secondGenNW, secondGenNE, secondGenSW, secondGenSE);

    const auto findOrCreate = [](const LifeNode* nw, const LifeNode* ne,
                                 const LifeNode* sw, const LifeNode* se) {
        return FindOrCreate(nw, ne, sw, se);
    };
    return DecodeLevel2(resultBits, findOrCreate);
}

// 8x8 base case for 1-generation advancement. Advances a level-3 node
// by 1 generation, returning a level-2 node (center 4x4).
const LifeNode* HashQuadtree::AdvanceBaseOneGen(const LifeNode* node) const {
    const auto quadrants = EncodeLevel3(node);
    const auto gen1 = ComputeFirstGeneration(quadrants);
    const auto resultBits = AssembleCentered6x6(gen1);

    const auto findOrCreate = [](const LifeNode* nw, const LifeNode* ne,
                                 const LifeNode* sw, const LifeNode* se) {
        return FindOrCreate(nw, ne, sw, se);
    };
    return DecodeLevel2(resultBits, findOrCreate);
}

NodeUpdateInfo HashQuadtree::AdvanceNode(std::stop_token stopToken,
                                         const LifeNode* node, int32_t level,
                                         int32_t advanceDepth) const {
    if (stopToken.stop_requested())
        return {node, 0};
    if (node == FalseNode || level < 3)
        return {node, 0};

    if (advanceDepth >= 0) {
        if (level - 2 > advanceDepth)
            return AdvanceSlow(stopToken, node, level, advanceDepth);
    }
    return AdvanceFast(stopToken, node, level, advanceDepth);
}

// TODO: Refactor AdvanceSlow
NodeUpdateInfo HashQuadtree::AdvanceSlow(std::stop_token stopToken,
                                         const LifeNode* node, int32_t level,
                                         int32_t advanceLevel) const {
    // At a high level, AdvanceSlow will split `node` into an 8x8 grid of
    // subnodes. Then, we can use overlapping components to take the 4x4 grids
    // aligned with each corner, and then advance them into a 2x2 nodes.
    // Finally, we assemble these 2x2 nodes into the centered 4x4 node, which is
    // the result of advancing the original node. Unlike HashQuadtree, we're
    // only making recursive calls one level down, and thus have more
    // fine-grained control over how many generations we advance.

    if (node == FalseNode)
        return {FalseNode, 0};

    // At the 8x8 base case, choose between 1-gen and 2-gen advancement.
    const auto actualLevel = (advanceLevel >= 1) ? 1 : 0;

    if (level <= 3) {
        const auto* result =
            (advanceLevel >= 1) ? AdvanceBase(node) : AdvanceBaseOneGen(node);

        if (stopToken.stop_requested()) {
            return {node, 0};
        }
        // Store under the requested maxAdvance so the same request hits.
        s_Cache.SlowCache[{node, advanceLevel}] = result;
        // Also store under the actual generations for cross-request reuse.
        return {result, actualLevel};
    }
    if (const auto it = s_Cache.SlowCache.find({node, advanceLevel});
        it != s_Cache.SlowCache.end()) {
        return {it->second, advanceLevel};
    }

    constexpr static auto subdivisions = 8;
    constexpr static auto index = [](int32_t x, int32_t y) {
        return y * subdivisions + x;
    };

    std::array<const LifeNode*, subdivisions * subdivisions> segments{};
    const auto fetchSegment = [&](int32_t x, int32_t y) {
        const auto* current = node;
        for (auto bit = 2; bit >= 0 && current != FalseNode; --bit) {
            const bool east = (x >> bit) & 1;
            const bool south = (y >> bit) & 1;
            if (south) {
                current = east ? current->SouthEast : current->SouthWest;
            } else {
                current = east ? current->NorthEast : current->NorthWest;
            }
            if (current == nullptr) {
                break;
            }
        }
        return current;
    };

    for (auto y = 0; y < subdivisions; ++y) {
        for (auto x = 0; x < subdivisions; ++x) {
            segments[index(x, y)] = fetchSegment(x, y);
        }
    }

    const auto combine2x2 = [&](int32_t startX, int32_t startY) {
        return FindOrCreate(segments[index(startX, startY)],
                            segments[index(startX + 1, startY)],
                            segments[index(startX, startY + 1)],
                            segments[index(startX + 1, startY + 1)]);
    };

    const auto buildWindow = [&](int32_t startX, int32_t startY) {
        const auto* nw = combine2x2(startX, startY);
        const auto* ne = combine2x2(startX + 2, startY);
        const auto* sw = combine2x2(startX, startY + 2);
        const auto* se = combine2x2(startX + 2, startY + 2);
        return FindOrCreate(nw, ne, sw, se);
    };

    const auto* window00 = buildWindow(1, 1);
    const auto* window01 = buildWindow(3, 1);
    const auto* window10 = buildWindow(1, 3);
    const auto* window11 = buildWindow(3, 3);

    const auto result00 =
        AdvanceNode(stopToken, window00, level - 1, advanceLevel);
    const auto result01 =
        AdvanceNode(stopToken, window01, level - 1, advanceLevel);
    const auto result10 =
        AdvanceNode(stopToken, window10, level - 1, advanceLevel);
    const auto result11 =
        AdvanceNode(stopToken, window11, level - 1, advanceLevel);

    const auto newAdvanceLevel = result00.AdvanceLevel;
    const auto* combined = FindOrCreate(result00.Node, result01.Node,
                                        result10.Node, result11.Node);

    if (stopToken.stop_requested()) {
        return {node, 0};
    }

    // Store under the requested maxAdvance so the same request hits next time.
    s_Cache.SlowCache[{node, advanceLevel}] = combined;
    return {combined, newAdvanceLevel};
}

NodeUpdateInfo HashQuadtree::AdvanceFast(std::stop_token stopToken,
                                         const LifeNode* node, int32_t level,
                                         int32_t advanceLevel) const {
    // At a high level, we want to assemble a node that is half the size of
    // `node`, but centered at the same point. By following this logic all the
    // way down the recursion, we are able to safely advance the entire universe
    // without having to worry about any cells on the boundary of the universe.
    // To achieve this end, we create a grid of overlapping cells centered
    // around `node`'s center, and tactically combine them to form the four
    // quadrants of the center node that is half the size. THe key to this
    // process is that the two levels are made by recursively calling
    // AdvanceFast, which allows for logarithmic time progression.

    if (node == FalseNode)
        return {FalseNode, 0};

    if (const auto itr = s_Cache.NodeMap.find(node);
        itr != s_Cache.NodeMap.end() && itr->second != nullptr) {
        return {itr->second, level - 2};
    }

    if (level == 3) {
        const auto* base = AdvanceBase(node);
        s_Cache.NodeMap[node] = base;
        return {base, 1};
    }

    const auto n00 =
        AdvanceNode(stopToken, node->NorthWest, level - 1, advanceLevel);
    const auto n01 = AdvanceNode(
        stopToken, CenteredHorizontal(*node->NorthWest, *node->NorthEast),
        level - 1, advanceLevel);
    const auto n02 =
        AdvanceNode(stopToken, node->NorthEast, level - 1, advanceLevel);
    const auto n10 = AdvanceNode(
        stopToken, CenteredVertical(*node->NorthWest, *node->SouthWest),
        level - 1, advanceLevel);
    const auto n11 =
        AdvanceNode(stopToken, CenteredSubNode(*node), level - 1, advanceLevel);
    const auto n12 = AdvanceNode(
        stopToken, CenteredVertical(*node->NorthEast, *node->SouthEast),
        level - 1, advanceLevel);
    const auto n20 =
        AdvanceNode(stopToken, node->SouthWest, level - 1, advanceLevel);
    const auto n21 = AdvanceNode(
        stopToken, CenteredHorizontal(*node->SouthWest, *node->SouthEast),
        level - 1, advanceLevel);
    const auto n22 =
        AdvanceNode(stopToken, node->SouthEast, level - 1, advanceLevel);

    const auto topLeft = AdvanceNode(
        stopToken, FindOrCreate(n00.Node, n01.Node, n10.Node, n11.Node),
        level - 1, advanceLevel);
    const auto topRight = AdvanceNode(
        stopToken, FindOrCreate(n01.Node, n02.Node, n11.Node, n12.Node),
        level - 1, advanceLevel);
    const auto bottomLeft = AdvanceNode(
        stopToken, FindOrCreate(n10.Node, n11.Node, n20.Node, n21.Node),
        level - 1, advanceLevel);
    const auto bottomRight = AdvanceNode(
        stopToken, FindOrCreate(n11.Node, n12.Node, n21.Node, n22.Node),
        level - 1, advanceLevel);

    const auto* result = FindOrCreate(topLeft.Node, topRight.Node,
                                      bottomLeft.Node, bottomRight.Node);

    if (stopToken.stop_requested()) {
        return {node, 0};
    }

    s_Cache.NodeMap[node] = result;
    return {result, level - 2};
}

bool HashQuadtree::NeedsExpansion(const LifeNode* node, int32_t level) const {
    if (node == FalseNode)
        return false;
    if (level <= 3)
        return true;

    const auto notEmpty = [&](const LifeNode* n) {
        return n != FalseNode && !n->IsEmpty;
    };

    // We simply want to consider if the outer rim of cells is completely empty,
    // and if the next level down is completely empty. This may sometimes cause
    // unnecessary expansion, but it is a fairly low-overhead procedure that may
    // actually result in better performance when hyper speed is enabled.

    const auto* nw = node->NorthWest;
    if (notEmpty(nw)) {
        if (notEmpty(nw->NorthWest) || notEmpty(nw->NorthEast) ||
            notEmpty(nw->SouthWest))
            return true;

        const auto* nwSe = nw->SouthEast;
        if (notEmpty(nwSe) &&
            (notEmpty(nwSe->NorthWest) || notEmpty(nwSe->NorthEast) ||
             notEmpty(nwSe->SouthWest)))
            return true;
    }

    const auto* ne = node->NorthEast;
    if (notEmpty(ne)) {
        if (notEmpty(ne->NorthWest) || notEmpty(ne->NorthEast) ||
            notEmpty(ne->SouthEast))
            return true;

        const auto* neSw = ne->SouthWest;
        if (notEmpty(neSw) &&
            (notEmpty(neSw->NorthWest) || notEmpty(neSw->NorthEast) ||
             notEmpty(neSw->SouthEast)))
            return true;
    }

    const auto* sw = node->SouthWest;
    if (notEmpty(sw)) {
        if (notEmpty(sw->NorthWest) || notEmpty(sw->SouthWest) ||
            notEmpty(sw->SouthEast))
            return true;

        const auto* swNe = sw->NorthEast;
        if (notEmpty(swNe) &&
            (notEmpty(swNe->NorthWest) || notEmpty(swNe->SouthWest) ||
             notEmpty(swNe->SouthEast)))
            return true;
    }

    const auto* se = node->SouthEast;
    if (notEmpty(se)) {
        if (notEmpty(se->NorthEast) || notEmpty(se->SouthWest) ||
            notEmpty(se->SouthEast))
            return true;

        const auto* seNw = se->NorthWest;
        if (notEmpty(seNw) &&
            (notEmpty(seNw->NorthEast) || notEmpty(seNw->SouthWest) ||
             notEmpty(seNw->SouthEast)))
            return true;
    }

    return false;
}

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

int32_t HashQuadtree::Advance(int32_t advanceDepth, std::stop_token stopToken) {
    if (m_Root == FalseNode)
        return {};

    const auto* root = m_Root;

    // Before we can actually advance, we must make sure the universe is
    // sufficiently large so that no data is lost after advancing (remember that
    // HashLife will cut off 75% of the universe's area)

    auto depth = m_Depth;

    // The second condition in this while loop is to prevent freezing when the
    // user asks for a large step size on a small pattern. For example, running
    // hyperspeed on a 2x2 block will not cause it to advance particularly fast
    // since it exhibits no expansion, but if maxAdvance is specified to 2^32,
    // we can make it happen instantly.
    while (NeedsExpansion(root, depth) || (depth - 2 < advanceDepth)) {
        root = ExpandUniverse(root, depth);
        depth++;
    }

    const auto advanced = AdvanceNode(stopToken, root, depth, advanceDepth);

    m_Root = advanced.Node;
    m_Depth = depth - 1; // AdvanceNode returns a node half the size
    return advanced.AdvanceLevel;
}

const LifeNode* HashQuadtree::EmptyTree(int32_t level) {
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
    EmptyTree(32);
    return result;
}
} // namespace gol
