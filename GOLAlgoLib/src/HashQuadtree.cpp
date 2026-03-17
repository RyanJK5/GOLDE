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
template <std::integral T> constexpr static int64_t Pow2(T exponent) {
    // This file prefers static_cast<int64_t>(1) to 1LL for consistency across
    // platforms.
    return static_cast<int64_t>(1) << exponent;
}

// Returns the greatest power of two less than `stepSize`.
constexpr static int64_t MaxAdvanceOf(int64_t stepSize) {
    if (stepSize == 0)
        return 0;

    auto power = 0LL;
    while (Pow2(power) <= stepSize)
        power++;
    return Pow2(power - static_cast<int64_t>(1));
}

thread_local HashLifeCache HashQuadtree::s_Cache{};

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
constexpr static uint32_t NumLeafPatterns = 65536;

// Bitmasks for extracting 2x2 quadrants from a 16-bit 4x4 grid.
constexpr static uint16_t MaskNW = 0xCC00;
constexpr static uint16_t MaskNE = 0x3300;
constexpr static uint16_t MaskSW = 0x00CC;
constexpr static uint16_t MaskSE = 0x0033;

// Returns the bit position (0-15) for a cell at (col, row) in a 4x4 grid.
constexpr static int BitPosition(int col, int row) {
    return (3 - row) * 4 + (3 - col);
}

// Precomputed lookup table for Conway's Game of Life (B3/S23).
// Maps each 16-bit 4x4 pattern to the next-generation state of the center 2x2,
// encoded in bits {5, 4, 1, 0} (the SE quadrant positions). This encoding
// allows the results of 9 overlapping lookups to be efficiently assembled
// via shifts into a full 4x4 result.
static auto BuildRuleTable() {
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

static const auto RuleTable = BuildRuleTable();

// Directly encodes a level-1 quadrant's cells into the known bit positions
// for each 2x2 sub-quadrant of the 4x4 grid.
static uint16_t EncodeQuadrantNW(const LifeNode* q) {
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
static uint16_t EncodeQuadrantNE(const LifeNode* q) {
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
static uint16_t EncodeQuadrantSW(const LifeNode* q) {
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
static uint16_t EncodeQuadrantSE(const LifeNode* q) {
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
static uint16_t EncodeLevel2(const LifeNode* node) {
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
static const LifeNode* BitToCell(uint16_t bits, int position) {
    return ((bits >> position) & 1) != 0 ? TrueNode : FalseNode;
}

// Decodes a 16-bit 4x4 value back to a level-2 LifeNode.
// The result is a node with four level-1 quadrant children, each
// containing four leaf cells.
template <typename FindOrCreateFn>
static const LifeNode* DecodeLevel2(uint16_t bits,
                                    FindOrCreateFn&& findOrCreate) {
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

// Free function form of calling HashQuadtree::Advance, with the added bonus of
// supporting an exact `stepCount` rather than just a `maxAdvance`.
int64_t HashLife(HashQuadtree& data, int64_t numSteps,
                 std::stop_token stopToken) {
    if (numSteps == 0) // Hyper speed
        return data.Advance(0, stopToken);

    auto generation = static_cast<int64_t>(0);
    while (generation < numSteps) {
        const auto maxAdvance = MaxAdvanceOf(numSteps - generation);

        const auto gens = data.Advance(maxAdvance, stopToken);
        if (gens == 0)
            return generation;
        generation += gens;
    }

    return generation;
}

size_t LifeNodeHash::operator()(const LifeNode* node) const {
    if (!node)
        return std::hash<const void*>{}(nullptr);
    return node->Hash;
}

// Mixes the node's precomputed hash with MaxAdvance. The node hash is already
// well-distributed via splitmix64, so a single round of xor-shift mixing with
// the advance count is sufficient.
size_t SlowHash::operator()(const SlowKey& key) const noexcept {
    const auto nodeHash = key.Node ? key.Node->Hash : uint64_t{0};
    const auto advanceHash = static_cast<uint64_t>(key.MaxAdvance);

    // Combine using a single xor-shift mix (cheaper than full splitmix64
    // since nodeHash is already finalized).
    // Golden ratio constant (2^64 / phi) for multiplicative hashing.
    constexpr uint64_t GoldenRatio = 0x9E3779B97F4A7C15ULL;
    auto mixed = nodeHash ^ (advanceHash * GoldenRatio);
    mixed ^= mixed >> 32;
    return static_cast<size_t>(mixed);
}

template class HashQuadtree::IteratorImpl<Vec2>;
template class HashQuadtree::IteratorImpl<const Vec2>;

HashQuadtree::HashQuadtree(const LifeHashSet& data, Vec2 offset) {
    if (data.empty())
        return;

    m_Root = BuildTree(data);
    m_RootOffset +=
        {static_cast<int64_t>(offset.X), static_cast<int64_t>(offset.Y)};
}

HashQuadtree::HashQuadtree(const HashQuadtree& other) { Copy(other); }

HashQuadtree& HashQuadtree::operator=(const HashQuadtree& other) {
    if (this != &other) {
        Copy(other);
    }
    return *this;
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

// Simple container for using some dynamic programming when preparing
// quadtree copies.
using TransferMap =
    ankerl::unordered_dense::map<const LifeNode*, const LifeNode*>;
static const LifeNode* BuildCache(TransferMap& transferMap,
                                  HashLifeCache& cache,
                                  const LifeNode* original) {
    // Base cases: leaf nodes
    if (original == FalseNode)
        return FalseNode;
    if (original == TrueNode)
        return TrueNode;

    if (const auto it = transferMap.find(original); it != transferMap.end())
        return it->second;

    const auto* nw = BuildCache(transferMap, cache, original->NorthWest);
    const auto* ne = BuildCache(transferMap, cache, original->NorthEast);
    const auto* sw = BuildCache(transferMap, cache, original->SouthWest);
    const auto* se = BuildCache(transferMap, cache, original->SouthEast);

    const auto* ret = FindOrCreateFromCache(cache, nw, ne, sw, se);
    transferMap[original] = ret;
    return ret;
}

void HashQuadtree::Copy(const HashQuadtree& other) {

    m_Root = FalseNode;
    m_RootOffset = other.m_RootOffset;
    m_Depth = other.m_Depth;

    if (other.m_Root == FalseNode) {
        return;
    }

    const bool crossThread = std::this_thread::get_id() != other.m_TransferID;
    if (!crossThread) {
        m_Root = other.m_Root;
        return;
    }

    if (other.m_TransferCache) {
        TransferMap transferMap{};
        m_Root = BuildCache(transferMap, s_Cache, other.m_TransferRoot);
        return;
    }
    assert(false && "Must prepare a transfer cache across threads");
}

void HashQuadtree::PrepareCopyBetweenThreads() {
    TransferMap transferMap{};
    m_TransferCache = std::make_unique<HashLifeCache>();
    m_TransferRoot = BuildCache(transferMap, *m_TransferCache, m_Root);
    m_TransferID = std::this_thread::get_id();
}

int32_t HashQuadtree::CalculateDepth() const { return m_Depth; }

// TODO: Improve efficiency of equality comparison
bool HashQuadtree::operator==(const HashQuadtree& other) const {
    if (m_Root == other.m_Root && m_RootOffset == other.m_RootOffset) {
        return true;
    }

    const auto hashSet1 = *this | std::ranges::to<LifeHashSet>();
    const auto hashSet2 = other | std::ranges::to<LifeHashSet>();

    return hashSet1 == hashSet2;
}

bool HashQuadtree::operator!=(const HashQuadtree& other) const {
    return !(*this == other);
}

int64_t HashQuadtree::CalculateTreeSize() const {
    if (m_Root == FalseNode) {
        return 0;
    }
    return Pow2(m_Depth);
}

bool HashQuadtree::empty() const { return m_Root == EmptyTree(m_Depth); }

BigInt HashQuadtree::PopulationOf(const LifeNode* node) const {
    if (node == FalseNode) {
        return BigZero;
    }
    if (node == TrueNode) {
        return BigOne;
    }

    if (auto it = s_Cache.PopulationCache.find(node);
        it != s_Cache.PopulationCache.end()) {
        std::println("Reading cache");
        return it->second;
    }

    // 4. Insert and return a copy
    return s_Cache.PopulationCache[node] =
               PopulationOf(node->NorthWest) + PopulationOf(node->NorthEast) +
               PopulationOf(node->SouthWest) + PopulationOf(node->SouthEast);
}

BigInt HashQuadtree::Population() const { return PopulationOf(m_Root); }

HashQuadtree::Iterator HashQuadtree::begin() {
    if (m_Root == FalseNode) {
        return end();
    }

    const auto size = CalculateTreeSize();
    return Iterator{m_Root, m_RootOffset, size, false};
}

HashQuadtree::Iterator HashQuadtree::end() { return Iterator{}; }

HashQuadtree::ConstIterator HashQuadtree::begin() const {
    if (m_Root == FalseNode) {
        return end();
    }

    const auto size = CalculateTreeSize();
    return ConstIterator{m_Root, m_RootOffset, size, false};
}

HashQuadtree::Iterator HashQuadtree::begin(Rect bounds) {
    if (m_Root == FalseNode) {
        return end();
    }

    const auto size = CalculateTreeSize();
    return Iterator{m_Root, m_RootOffset, size, false, &bounds};
}

HashQuadtree::ConstIterator HashQuadtree::begin(Rect bounds) const {
    if (m_Root == FalseNode)
        return end();

    const auto size = CalculateTreeSize();
    return ConstIterator{m_Root, m_RootOffset, size, false, &bounds};
}

HashQuadtree::ConstIterator HashQuadtree::end() const {
    return ConstIterator{};
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
                                         int64_t maxAdvance) const {
    if (stopToken.stop_requested())
        return {node, 0};
    if (node == FalseNode || level < 3)
        return {node, 0};

    // Currently GOLExecutable cannot handle depth >= 63, so we bound it here.
    if (maxAdvance > 0 || m_Depth >= 63) {
        const auto span = Pow2(level);
        if ((span / 4) > maxAdvance)
            return AdvanceSlow(stopToken, node, level, maxAdvance);
    }
    return AdvanceFast(stopToken, node, level, maxAdvance);
}

// TODO: Refactor AdvanceSlow
NodeUpdateInfo HashQuadtree::AdvanceSlow(std::stop_token stopToken,
                                         const LifeNode* node, int32_t level,
                                         int64_t maxAdvance) const {
    // At a high level, AdvanceSlow will split `node` into an 8x8 grid of
    // subnodes. Then, we can use overlapping components to take the 4x4 grids
    // aligned with each corner, and then advance them into a 2x2 nodes.
    // Finally, we assemble these 2x2 nodes into the centered 4x4 node, which is
    // the result of advancing the original node. Unlike HashQuadtree, we're
    // only making recursive calls one level down, and thus have more
    // fine-grained control over how many generations we advance.

    if (node == FalseNode)
        return {FalseNode, 0};
    if (level <= 3) {
        // At the 8x8 base case, choose between 1-gen and 2-gen advancement.
        const auto generations = (maxAdvance >= 2) ? int64_t{2} : int64_t{1};
        const auto* result =
            (maxAdvance >= 2) ? AdvanceBase(node) : AdvanceBaseOneGen(node);
        // Store under the requested maxAdvance so the same request hits.
        s_Cache.SlowCache[{node, maxAdvance}] = result;
        // Also store under the actual generations for cross-request reuse.
        if (generations != maxAdvance)
            s_Cache.SlowCache[{node, generations}] = result;
        return {result, generations};
    }
    if (const auto it = s_Cache.SlowCache.find({node, maxAdvance});
        it != s_Cache.SlowCache.end())
        return {it->second, maxAdvance};

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
        AdvanceNode(stopToken, window00, level - 1, maxAdvance);
    const auto result01 =
        AdvanceNode(stopToken, window01, level - 1, maxAdvance);
    const auto result10 =
        AdvanceNode(stopToken, window10, level - 1, maxAdvance);
    const auto result11 =
        AdvanceNode(stopToken, window11, level - 1, maxAdvance);

    const auto generations = result00.Generations;
    const auto* combined = FindOrCreate(result00.Node, result01.Node,
                                        result10.Node, result11.Node);

    // Store under the requested maxAdvance so the same request hits next time.
    s_Cache.SlowCache[{node, maxAdvance}] = combined;
    // Also store under actual generations returned for cross-request reuse.
    if (generations != maxAdvance)
        s_Cache.SlowCache[{node, generations}] = combined;
    return {combined, generations};
}

NodeUpdateInfo HashQuadtree::AdvanceFast(std::stop_token stopToken,
                                         const LifeNode* node, int32_t level,
                                         int64_t maxAdvance) const {
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
        const auto generations = Pow2(level - 2);
        return {itr->second, generations};
    }

    if (level == 3) {
        const auto* base = AdvanceBase(node);
        s_Cache.NodeMap[node] = base;
        return {base, 2};
    }

    const auto n00 =
        AdvanceNode(stopToken, node->NorthWest, level - 1, maxAdvance);
    const auto n01 = AdvanceNode(
        stopToken, CenteredHorizontal(*node->NorthWest, *node->NorthEast),
        level - 1, maxAdvance);
    const auto n02 =
        AdvanceNode(stopToken, node->NorthEast, level - 1, maxAdvance);
    const auto n10 = AdvanceNode(
        stopToken, CenteredVertical(*node->NorthWest, *node->SouthWest),
        level - 1, maxAdvance);
    const auto n11 =
        AdvanceNode(stopToken, CenteredSubNode(*node), level - 1, maxAdvance);
    const auto n12 = AdvanceNode(
        stopToken, CenteredVertical(*node->NorthEast, *node->SouthEast),
        level - 1, maxAdvance);
    const auto n20 =
        AdvanceNode(stopToken, node->SouthWest, level - 1, maxAdvance);
    const auto n21 = AdvanceNode(
        stopToken, CenteredHorizontal(*node->SouthWest, *node->SouthEast),
        level - 1, maxAdvance);
    const auto n22 =
        AdvanceNode(stopToken, node->SouthEast, level - 1, maxAdvance);

    const auto topLeft = AdvanceNode(
        stopToken, FindOrCreate(n00.Node, n01.Node, n10.Node, n11.Node),
        level - 1, maxAdvance);
    const auto topRight = AdvanceNode(
        stopToken, FindOrCreate(n01.Node, n02.Node, n11.Node, n12.Node),
        level - 1, maxAdvance);
    const auto bottomLeft = AdvanceNode(
        stopToken, FindOrCreate(n10.Node, n11.Node, n20.Node, n21.Node),
        level - 1, maxAdvance);
    const auto bottomRight = AdvanceNode(
        stopToken, FindOrCreate(n11.Node, n12.Node, n21.Node, n22.Node),
        level - 1, maxAdvance);

    const auto* result = FindOrCreate(topLeft.Node, topRight.Node,
                                      bottomLeft.Node, bottomRight.Node);
    s_Cache.NodeMap[node] = result;
    const auto generations = Pow2(level - 2);
    return {result, generations};
}

bool HashQuadtree::NeedsExpansion(const LifeNode* node, int32_t level) const {
    if (node == FalseNode)
        return false;
    if (level <= 3)
        return true;

    const auto notEmpty = [&](const LifeNode* n, int32_t nodeLevel) {
        return n != FalseNode && n != EmptyTree(nodeLevel);
    };

    // We simply want to consider if the outer rim of cells is completely empty,
    // and if the next level down is completely empty. This may sometimes cause
    // unnecessary expansion, but it is a fairly low-overhead procedure that may
    // actually result in better performance when hyper speed is enabled.
    const auto l1 = level - 1;
    const auto l2 = level - 2;
    const auto l3 = level - 3;

    const auto* nw = node->NorthWest;
    if (notEmpty(nw, l1)) {
        if (notEmpty(nw->NorthWest, l2) || notEmpty(nw->NorthEast, l2) ||
            notEmpty(nw->SouthWest, l2))
            return true;

        const auto* nwSe = nw->SouthEast;
        if (notEmpty(nwSe, l2) &&
            (notEmpty(nwSe->NorthWest, l3) || notEmpty(nwSe->NorthEast, l3) ||
             notEmpty(nwSe->SouthWest, l3)))
            return true;
    }

    const auto* ne = node->NorthEast;
    if (notEmpty(ne, l1)) {
        if (notEmpty(ne->NorthWest, l2) || notEmpty(ne->NorthEast, l2) ||
            notEmpty(ne->SouthEast, l2))
            return true;

        const auto* ne_sw = ne->SouthWest;
        if (notEmpty(ne_sw, l2) &&
            (notEmpty(ne_sw->NorthWest, l3) || notEmpty(ne_sw->NorthEast, l3) ||
             notEmpty(ne_sw->SouthEast, l3)))
            return true;
    }

    const auto* sw = node->SouthWest;
    if (notEmpty(sw, l1)) {
        if (notEmpty(sw->NorthWest, l2) || notEmpty(sw->SouthWest, l2) ||
            notEmpty(sw->SouthEast, l2))
            return true;

        const auto* sw_ne = sw->NorthEast;
        if (notEmpty(sw_ne, l2) &&
            (notEmpty(sw_ne->NorthWest, l3) || notEmpty(sw_ne->SouthWest, l3) ||
             notEmpty(sw_ne->SouthEast, l3)))
            return true;
    }

    const auto* se = node->SouthEast;
    if (notEmpty(se, l1)) {
        if (notEmpty(se->NorthEast, l2) || notEmpty(se->SouthWest, l2) ||
            notEmpty(se->SouthEast, l2))
            return true;

        const auto* se_nw = se->NorthWest;
        if (notEmpty(se_nw, l2) &&
            (notEmpty(se_nw->NorthEast, l3) || notEmpty(se_nw->SouthWest, l3) ||
             notEmpty(se_nw->SouthEast, l3)))
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

int64_t HashQuadtree::Advance(int64_t maxAdvance, std::stop_token stopToken) {
    if (m_Root == FalseNode)
        return {};

    const auto* root = m_Root;

    // Before we can actually advance, we must make sure the universe is
    // sufficiently large so that no data is lost after advancing (remember that
    // HashLife will cut off 75% of the universe's area)

    auto depth = m_Depth;
    auto size = std::max(static_cast<int64_t>(1), Pow2(depth));
    auto offset = m_RootOffset;

    // The second condition in this while loop is to prevent freezing when the
    // user asks for a large step size on a small pattern. For example, running
    // hyperspeed on a 2x2 block will not cause it to advance particularly fast
    // since it exhibits no expansion, but if maxAdvance is specified to 2^32,
    // we can make it happen instantly.
    while (NeedsExpansion(root, depth) || (size / 4 < maxAdvance)) {
        root = ExpandUniverse(root, depth);
        const auto delta = std::max(static_cast<int64_t>(1), size / 2);
        offset.X -= delta;
        offset.Y -= delta;
        depth++;
        size = Pow2(depth);
    }

    if (depth >= 63)
        return 0;

    const auto advanced = AdvanceNode(stopToken, root, depth, maxAdvance);
    const auto centerDelta = std::max(static_cast<int64_t>(1), size / 4);
    offset.X += centerDelta;
    offset.Y += centerDelta;

    m_Root = advanced.Node;
    m_RootOffset = offset;
    m_Depth = depth - 1; // AdvanceNode returns a node half the size
    return advanced.Generations;
}

const LifeNode* HashQuadtree::EmptyTree(int32_t level) {
    if (level <= 0)
        return FalseNode;

    // Level-indexed lookup: index is the level (1..63).
    if (s_Cache.EmptyNodeCache[level] != nullptr)
        return s_Cache.EmptyNodeCache[level];

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

const LifeNode* HashQuadtree::BuildTree(const LifeHashSet& cells) {
    if (cells.empty()) {
        m_RootOffset = Vec2L{0LL, 0LL};
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

    m_RootOffset =
        Vec2L{static_cast<int64_t>(minXv), static_cast<int64_t>(minYv)};

    m_Depth = gridExponent;

    // Promote cells to int64_t types
    std::vector<Vec2L> cellVec{};
    cellVec.reserve(cells.size());
    for (const auto cell : cells)
        cellVec.emplace_back(static_cast<int64_t>(cell.X),
                             static_cast<int64_t>(cell.Y));

    return BuildTreeRegion(cellVec, m_RootOffset, gridExponent);
}
} // namespace gol
