#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <stop_token>
#include <type_traits>
#include <unordered_dense.h>
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

// Free function form of calling HashQuadtree::Advance, with the added bonus of
// supporting an exact `stepCount` rather than just a `maxAdvance`.
int64_t HashLife(HashQuadtree &data, int64_t numSteps,
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

size_t LifeNodeHash::operator()(const LifeNode *node) const {
    if (!node)
        return std::hash<const void *>{}(nullptr);
    return node->Hash;
}

// Arbitrary magic number hash
size_t SlowHash::operator()(const SlowKey &key) const noexcept {
    const auto h1 = static_cast<uint64_t>(LifeNodeHash{}(key.Node));
    const auto h2 = static_cast<uint64_t>(
        static_cast<uint64_t>(key.MaxAdvance) + 0x9e3779b97f4a7c15ULL);

    auto combined = h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));

    combined = (combined ^ (combined >> 30)) * 0xBF58476D1CE4E5B9ULL;
    combined = (combined ^ (combined >> 27)) * 0x94D049BB133111EBULL;
    combined = combined ^ (combined >> 31);

    return static_cast<size_t>(combined);
}

template class HashQuadtree::IteratorImpl<Vec2>;
template class HashQuadtree::IteratorImpl<const Vec2>;

HashQuadtree::HashQuadtree(const LifeHashSet &data, Vec2 offset) {
    if (data.empty())
        return;

    m_Root = BuildTree(data);
    m_RootOffset +=
        {static_cast<int64_t>(offset.X), static_cast<int64_t>(offset.Y)};
}

HashQuadtree::HashQuadtree(const HashQuadtree &other) { Copy(other); }

HashQuadtree &HashQuadtree::operator=(const HashQuadtree &other) {
    if (this != &other) {
        Copy(other);
    }
    return *this;
}

// Generic form of HashQuadtree::FindOrCreate that we can also use
// for building a transfer cache.
static const LifeNode *FindOrCreateFromCache(HashLifeCache &cache,
                                             const LifeNode *nw,
                                             const LifeNode *ne,
                                             const LifeNode *sw,
                                             const LifeNode *se) {
    LifeNode toFind{nw, ne, sw, se};
    if (const auto itr = cache.NodeMap.find(&toFind);
        itr != cache.NodeMap.end()) {
        return itr->first;
    }

    cache.NodeStorage.emplace_back(nw, ne, sw, se);
    cache.NodeMap[&cache.NodeStorage.back()] = nullptr;
    return &cache.NodeStorage.back();
}

// Simple container for using some dynamic programming when preparing
// quadtree copies.
using TransferMap =
    ankerl::unordered_dense::map<const LifeNode *, const LifeNode *>;
static const LifeNode *BuildCache(TransferMap &transferMap,
                                  HashLifeCache &cache,
                                  const LifeNode *original) {
    // Base cases: leaf nodes
    if (original == FalseNode)
        return FalseNode;
    if (original == TrueNode)
        return TrueNode;

    if (const auto it = transferMap.find(original); it != transferMap.end())
        return it->second;

    const auto *nw = BuildCache(transferMap, cache, original->NorthWest);
    const auto *ne = BuildCache(transferMap, cache, original->NorthEast);
    const auto *sw = BuildCache(transferMap, cache, original->SouthWest);
    const auto *se = BuildCache(transferMap, cache, original->SouthEast);

    const auto *ret = FindOrCreateFromCache(cache, nw, ne, sw, se);
    transferMap[original] = ret;
    return ret;
}

void HashQuadtree::Copy(const HashQuadtree &other) {
    m_Root = FalseNode;
    m_RootOffset = other.m_RootOffset;

    if (other.m_Root == FalseNode) {
        return;
    }

    if (other.m_TransferCache) {
        TransferMap transferMap{};
        m_Root = BuildCache(transferMap, s_Cache,
                            &other.m_TransferCache->NodeStorage.back());
        other.m_TransferCache = nullptr;
    } else { // Should ONLY occur if copying/moving in the same thread.
        m_Root = other.m_Root;
    }
}

void HashQuadtree::PrepareCopyBetweenThreads() {
    TransferMap transferMap{};
    m_TransferCache = std::make_unique<HashLifeCache>();
    BuildCache(transferMap, *m_TransferCache, m_Root);
}

int32_t HashQuadtree::CalculateDepth() const {
    if (m_Root == FalseNode || m_Root == TrueNode) {
        return 0;
    }

    auto depth = 0;
    const auto *current = m_Root;
    while (current != TrueNode && current != FalseNode) {
        current = current->NorthWest;
        depth++;
    }
    return depth;
}

// TODO: Improve efficiency of equality comparison
bool HashQuadtree::operator==(const HashQuadtree &other) const {
    if (m_Root == other.m_Root && m_RootOffset == other.m_RootOffset) {
        return true;
    }

    const auto hashSet1 = *this | std::ranges::to<LifeHashSet>();
    const auto hashSet2 = other | std::ranges::to<LifeHashSet>();

    return hashSet1 == hashSet2;
}

bool HashQuadtree::operator!=(const HashQuadtree &other) const {
    return !(*this == other);
}

int64_t HashQuadtree::CalculateTreeSize() const {
    if (m_Root == FalseNode) {
        return 0;
    }
    return Pow2(CalculateDepth());
}

bool HashQuadtree::empty() const {
    return m_Root == FalseNode || m_Root->IsEmpty;
}

uint64_t HashQuadtree::Population() const {
    return m_Root ? m_Root->Population : 0ULL;
}

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

HashQuadtree::Iterator HashQuadtree::begin(const Rect &bounds) {
    if (m_Root == FalseNode) {
        return end();
    }

    const auto size = CalculateTreeSize();
    return Iterator{m_Root, m_RootOffset, size, false, &bounds};
}

HashQuadtree::ConstIterator HashQuadtree::begin(const Rect &bounds) const {
    if (m_Root == FalseNode)
        return end();

    const auto size = CalculateTreeSize();
    return ConstIterator{m_Root, m_RootOffset, size, false, &bounds};
}

HashQuadtree::ConstIterator HashQuadtree::end() const {
    return ConstIterator{};
}

const LifeNode *HashQuadtree::FindOrCreate(const LifeNode *nw,
                                           const LifeNode *ne,
                                           const LifeNode *sw,
                                           const LifeNode *se) const {
    // Defer to more generic function
    return FindOrCreateFromCache(s_Cache, nw, ne, sw, se);
}

const LifeNode *HashQuadtree::CenteredHorizontal(const LifeNode &west,
                                                 const LifeNode &east) const {
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

const LifeNode *HashQuadtree::CenteredVertical(const LifeNode &north,
                                               const LifeNode &south) const {
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

const LifeNode *HashQuadtree::CenteredSubNode(const LifeNode &node) const {
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

// TODO: Use more sophisticated algorithm for base case
const LifeNode *HashQuadtree::AdvanceBase(const LifeNode *node) const {
    // Some implementations use an 8x8 base case, which may be more efficient.
    constexpr static auto gridSize = 4;

    // Imagine cells as a 2D grid, 4x4 grid.
    const std::array cells{
        node->NorthWest->NorthWest, node->NorthWest->NorthEast,
        node->NorthEast->NorthWest, node->NorthEast->NorthEast,
        node->NorthWest->SouthWest, node->NorthWest->SouthEast,
        node->NorthEast->SouthWest, node->NorthEast->SouthEast,
        node->SouthWest->NorthWest, node->SouthWest->NorthEast,
        node->SouthEast->NorthWest, node->SouthEast->NorthEast,
        node->SouthWest->SouthWest, node->SouthWest->SouthEast,
        node->SouthEast->SouthWest, node->SouthEast->SouthEast};

    LifeHashSet result{};
    for (auto x = 0; x < gridSize; x++) {
        for (auto y = 0; y < gridSize; y++) {
            const auto index = y * gridSize + x;
            if (cells[index] == TrueNode) {
                result.insert({x, y});
            }
        }
    }
    // Simple deference to our other algorithm
    result = SparseLife(result, {0, 0, gridSize, gridSize});

    // We still return the inner node in the base case.
    return FindOrCreate(result.contains({1, 1}) ? TrueNode : FalseNode,
                        result.contains({2, 1}) ? TrueNode : FalseNode,
                        result.contains({1, 2}) ? TrueNode : FalseNode,
                        result.contains({2, 2}) ? TrueNode : FalseNode);
}

NodeUpdateInfo HashQuadtree::AdvanceNode(std::stop_token stopToken,
                                         const LifeNode *node, int32_t level,
                                         int64_t maxAdvance) const {
    if (stopToken.stop_requested())
        return {node, 0};
    if (node == FalseNode || level < 2)
        return {node, 0};

    // Currently GOLExecutable cannot handle depth >= 63, so we bound it here.
    if (maxAdvance > 0 || CalculateDepth() >= 63) {
        const auto span = Pow2(level);
        if ((span / 4) > maxAdvance)
            return AdvanceSlow(stopToken, node, level, maxAdvance);
    }
    return AdvanceFast(stopToken, node, level, maxAdvance);
}

// TODO: Refactor AdvanceSlow
NodeUpdateInfo HashQuadtree::AdvanceSlow(std::stop_token stopToken,
                                         const LifeNode *node, int32_t level,
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
    if (level <= 2) // Base case is capped at 1 generation anyway
        return AdvanceFast(stopToken, node, level, maxAdvance);
    if (const auto it = s_Cache.SlowCache.find({node, maxAdvance});
        it != s_Cache.SlowCache.end())
        return {it->second, maxAdvance};

    constexpr static auto subdivisions = 8;
    constexpr static auto index = [](int32_t x, int32_t y) {
        return y * subdivisions + x;
    };

    std::array<const LifeNode *, subdivisions * subdivisions> segments{};
    const auto fetchSegment = [&](int32_t x, int32_t y) {
        const auto *current = node;
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
        const auto *nw = combine2x2(startX, startY);
        const auto *ne = combine2x2(startX + 2, startY);
        const auto *sw = combine2x2(startX, startY + 2);
        const auto *se = combine2x2(startX + 2, startY + 2);
        return FindOrCreate(nw, ne, sw, se);
    };

    const auto *window00 = buildWindow(1, 1);
    const auto *window01 = buildWindow(3, 1);
    const auto *window10 = buildWindow(1, 3);
    const auto *window11 = buildWindow(3, 3);

    const auto result00 =
        AdvanceNode(stopToken, window00, level - 1, maxAdvance);
    const auto result01 =
        AdvanceNode(stopToken, window01, level - 1, maxAdvance);
    const auto result10 =
        AdvanceNode(stopToken, window10, level - 1, maxAdvance);
    const auto result11 =
        AdvanceNode(stopToken, window11, level - 1, maxAdvance);

    const auto generations = result00.Generations;
    const auto *combined = FindOrCreate(result00.Node, result01.Node,
                                        result10.Node, result11.Node);

    s_Cache.SlowCache[{node, generations}] = combined;
    return {combined, generations};
}

NodeUpdateInfo HashQuadtree::AdvanceFast(std::stop_token stopToken,
                                         const LifeNode *node, int32_t level,
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

    if (level == 2) {
        const auto *base = AdvanceBase(node);
        s_Cache.NodeMap[node] = base;
        return {base, 1};
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

    const auto *result = FindOrCreate(topLeft.Node, topRight.Node,
                                      bottomLeft.Node, bottomRight.Node);
    s_Cache.NodeMap[node] = result;
    const auto generations = Pow2(level - 2);
    return {result, generations};
}

bool HashQuadtree::NeedsExpansion(const LifeNode *node, int32_t level) const {
    if (node == FalseNode)
        return false;
    if (level <= 2)
        return true;

    constexpr static auto notEmpty = [](const LifeNode *n) {
        return n != FalseNode && !n->IsEmpty;
    };

    // We simply want to consider if the outer rim of cells is completely empty,
    // and if the next level down is completely empty. This may sometimes cause
    // unnecessary expansion, but it is a fairly low-overhead procedure that may
    // actually result in better performance when hyper speed is enabled.

    const auto *nw = node->NorthWest;
    if (notEmpty(nw)) {
        if (notEmpty(nw->NorthWest) || notEmpty(nw->NorthEast) ||
            notEmpty(nw->SouthWest))
            return true;

        const auto *nwSe = nw->SouthEast;
        if (notEmpty(nwSe) &&
            (notEmpty(nwSe->NorthWest) || notEmpty(nwSe->NorthEast) ||
             notEmpty(nwSe->SouthWest)))
            return true;
    }

    const auto *ne = node->NorthEast;
    if (notEmpty(ne)) {
        if (notEmpty(ne->NorthWest) || notEmpty(ne->NorthEast) ||
            notEmpty(ne->SouthEast))
            return true;

        const auto *ne_sw = ne->SouthWest;
        if (notEmpty(ne_sw) &&
            (notEmpty(ne_sw->NorthWest) || notEmpty(ne_sw->NorthEast) ||
             notEmpty(ne_sw->SouthEast)))
            return true;
    }

    const auto *sw = node->SouthWest;
    if (notEmpty(sw)) {
        if (notEmpty(sw->NorthWest) || notEmpty(sw->SouthWest) ||
            notEmpty(sw->SouthEast))
            return true;

        const auto *sw_ne = sw->NorthEast;
        if (notEmpty(sw_ne) &&
            (notEmpty(sw_ne->NorthWest) || notEmpty(sw_ne->SouthWest) ||
             notEmpty(sw_ne->SouthEast)))
            return true;
    }

    const auto *se = node->SouthEast;
    if (notEmpty(se)) {
        if (notEmpty(se->NorthEast) || notEmpty(se->SouthWest) ||
            notEmpty(se->SouthEast))
            return true;

        const auto *se_nw = se->NorthWest;
        if (notEmpty(se_nw) &&
            (notEmpty(se_nw->NorthEast) || notEmpty(se_nw->SouthWest) ||
             notEmpty(se_nw->SouthEast)))
            return true;
    }

    return false;
}

const LifeNode *HashQuadtree::ExpandUniverse(const LifeNode *node,
                                             int32_t level) const {
    if (node == FalseNode)
        return EmptyTree(Pow2(level + 1));

    if (node == TrueNode)
        return FindOrCreate(FalseNode, FalseNode, FalseNode, TrueNode);

    const auto childSize =
        level > 0 ? Pow2(level - 1) : static_cast<int64_t>(1);
    const auto *empty = EmptyTree(childSize);
    const auto *expandedNW = FindOrCreate(empty, empty, empty, node->NorthWest);
    const auto *expandedNE = FindOrCreate(empty, empty, node->NorthEast, empty);
    const auto *expandedSW = FindOrCreate(empty, node->SouthWest, empty, empty);
    const auto *expandedSE = FindOrCreate(node->SouthEast, empty, empty, empty);

    return FindOrCreate(expandedNW, expandedNE, expandedSW, expandedSE);
}

int64_t HashQuadtree::Advance(int64_t maxAdvance, std::stop_token stopToken) {
    if (m_Root == FalseNode)
        return {};

    const auto *root = m_Root;

    // Before we can actually advance, we must make sure the universe is
    // sufficiently large so that no data is lost after advancing (remember that
    // HashLife will cut off 75% of the universe's area)

    auto depth = CalculateDepth();
    auto size = std::max(static_cast<int64_t>(1), CalculateTreeSize());
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
    return advanced.Generations;
}

const LifeNode *HashQuadtree::EmptyTree(int64_t size) const {
    if (size <= 1)
        return FalseNode;

    // Empty node cache is used here as simple memoization. Since empty nodes
    // are requested practically every frame, this reduces time complexity
    // significantly.
    if (const auto it = s_Cache.EmptyNodeCache.find(size);
        it != s_Cache.EmptyNodeCache.end())
        return it->second;

    const auto child = EmptyTree(size / 2);
    const auto result = FindOrCreate(child, child, child, child);
    s_Cache.EmptyNodeCache[size] = result;
    return result;
}

const LifeNode *HashQuadtree::BuildTreeRegion(std::span<Vec2L> cells, Vec2L pos,
                                              int64_t size) {
    if (cells.empty())
        return EmptyTree(size);

    if (size == 1)
        return TrueNode;

    const auto half = size / 2;
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
        BuildTreeRegion({north.begin(), itNorthX}, {pos.X, pos.Y}, half),
        BuildTreeRegion({itNorthX, north.end()}, {pos.X + half, pos.Y}, half),
        BuildTreeRegion({south.begin(), itSouthX}, {pos.X, pos.Y + half}, half),
        BuildTreeRegion({itSouthX, south.end()}, {pos.X + half, pos.Y + half},
                        half));
}

const LifeNode *HashQuadtree::BuildTree(const LifeHashSet &cells) {
    if (cells.empty()) {
        m_RootOffset = Vec2L{0LL, 0LL};
        return FalseNode;
    }

    const auto [minX, maxX] = std::ranges::minmax_element(
        cells, [](Vec2 a, Vec2 b) { return a.X < b.X; });
    const auto [minY, maxY] = std::ranges::minmax_element(
        cells, [](Vec2 a, Vec2 b) { return a.Y < b.Y; });

    const auto neighborhoodSize =
        std::max(static_cast<int64_t>(maxX->X) - minX->X,
                 static_cast<int64_t>(maxY->Y) - minY->Y) +
        1;
    const auto gridExponent =
        static_cast<int64_t>(std::ceil(std::log2(neighborhoodSize)));
    const auto gridSize = Pow2(gridExponent);

    m_RootOffset =
        Vec2L{static_cast<int64_t>(minX->X), static_cast<int64_t>(minY->Y)};

    // Promote cells to int64_t types
    std::vector<Vec2L> cellVec{};
    cellVec.reserve(cells.size());
    for (const auto cell : cells)
        cellVec.emplace_back(static_cast<int64_t>(cell.X),
                             static_cast<int64_t>(cell.Y));

    return BuildTreeRegion(cellVec, m_RootOffset, gridSize);
}
} // namespace gol