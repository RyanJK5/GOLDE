#ifndef HashQuadtree_hpp_
#define HashQuadtree_hpp_

#include <ankerl/unordered_dense.h>
#include <array>
#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <stack>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "BigInt.hpp"
#include "Graphics2D.hpp"
#include "LifeHashSet.hpp"
#include "LifeNode.hpp"

// HashQuadtree and its related free functions and structs contain all of the
// necessary representations for the HashLife algorithm. At a high level,
// HashLife uses a quadtree to represent the Game of Life universe, and
// canonicalizes nodes in a hash table. The recursive nature of the algorithm
// also allows it to jump many generations in one step.

namespace gol {
// The key used for caching when HashLife has a bounded step size.
struct SlowKey {
    const LifeNode* Node;
    int32_t AdvanceLevel = 0; // Max number of generations this node can advance
    auto operator<=>(const SlowKey&) const = default;
};

struct SlowHash {
    size_t operator()(SlowKey key) const noexcept;
};

// The cache used for the HashLife algorithm.
struct HashLifeCache {
    // Bump-pointer arena where all LifeNodes are stored. Nodes are only
    // accessed by pointer outside of the cache.
    LifeNodeArena NodeStorage{};

    // The cache for the core HashLife algorithm, with hyper speed enabled.
    ankerl::unordered_dense::map<const LifeNode*, const LifeNode*, LifeNodeHash,
                                 LifeNodeEqual>
        NodeMap{};

    ankerl::unordered_dense::map<const LifeNode*, BigInt, LifeNodeHash,
                                 LifeNodeEqual>
        PopulationCache{};

    ankerl::unordered_dense::map<const LifeNode*, int64_t, LifeNodeHash,
                                 LifeNodeEqual>
        SmallPopulationCache{};

    // The cache for the HashLife algorithm when the step size is bounded.
    ankerl::unordered_dense::map<SlowKey, const LifeNode*, SlowHash>
        SlowCache{};

    // Simple container for using some dynamic programming when preparing
    // quadtree copies.
    ankerl::unordered_dense::map<const LifeNode*, const LifeNode*>
        TransferCopyCache{};

    // Level-indexed cache for empty nodes. Index i holds the empty node for
    // size 2^i. At most 64 entries needed (levels 0..63).
    std::vector<const LifeNode*> EmptyNodeCache{};

    HashLifeCache();
};

// This is the primary data structure for executing the HashLife algorithm. It
// satisfies the requirements of `std::ranges::input_range` and can be used in
// many STL algorithms.
class HashQuadtree {
  public:
    class Iterator {
      private:
        // We keep track of what node we're on through a variety of factors.
        struct LifeNodeData {
            const LifeNode* Node; // The node we're looking at.
            Vec2L Position;       // Where this node is located, relative to
                                  // HashQuadtree's offset.
            int32_t Level;        // Useful for bounded iteration
            uint8_t Quadrant;     // The quadrant this iterator is looking at.
                                  // Useful for knowing where to look next.
        };

      public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Vec2;
        using pointer = const value_type*;
        using reference = const value_type&;

        friend HashQuadtree;
        Iterator() = default;

        Iterator& operator++();
        Iterator operator++(int);

        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;

        value_type operator*() const;
        pointer operator->() const;

      private:
        Iterator(const LifeNode* root, Vec2L offset, int32_t level, bool isEnd,
                 const Rect* bounds);

        void AdvanceToNext(); // Implementation for operator++
      private:
        std::stack<LifeNodeData, std::vector<LifeNodeData>> m_Stack;
        Rect m_Bounds;
        // Note that because the iterator is looking at abstract data,
        // it is actually storing the Vec2 as a proxy object.
        value_type m_Current;
        bool m_IsEnd = true;
        bool m_UseBounds = false;
    };

  public:
    HashQuadtree() = default;
    HashQuadtree(std::span<const Vec2> data, Vec2 offset = {});

    HashQuadtree(const HashQuadtree& other);
    HashQuadtree& operator=(const HashQuadtree& other);

    // The move constructor and move assignment are left intentionally
    // unspecified. Copying within a thread is essentially a move because
    // all quadtrees within the same thread use the same node storage.
    // Moving across threads is not possible because storage is thread local.

    ~HashQuadtree() = default;

  public:
    bool empty() const;

    Iterator begin() const;
    Iterator end() const;

    // Returns an iterator over one region of space. This is useful for allowing
    // interaction and display of a small subsection of the universe.
    Iterator begin(Rect bounds) const;

    BigInt Population() const;

    // Advances the simulation. Passing `maxAdvance = 0` indicates that hyper
    // speed should be used. If the stop token is called during advance, the
    // function exits early, and `HashQuadtree` is left in a valid but
    // unspecified state (likely part-way through iteration).
    int32_t Advance(int32_t advanceDepth = -1, std::stop_token stopToken = {});

    // Applies func to all cells within the given bounds. More efficient than
    // iterators because recursion can be used.
    template <std::invocable<Vec2, int64_t> Func>
    void ForEachCell(const Func& func, Rect bounds, int32_t minLevel) const;

    // Returns the number of levels in the current tree.
    int32_t CalculateDepth() const;
    // Returns the length/width of the tree's root node.
    int64_t CalculateTreeSize() const;

    bool operator==(const HashQuadtree& other) const;
    bool operator!=(const HashQuadtree& other) const;

    void Set(Vec2 pos, bool alive);

    bool Get(Vec2 pos) const;

    void Clear(Rect region);

    HashQuadtree Extract(Rect region) const;

  private:
    const LifeNode* SetImpl(const LifeNode* node, Vec2L pos, Vec2 targetPos,
                            int32_t level, bool alive);

    bool GetImpl(const LifeNode* node, Vec2L pos, Vec2 targetPos,
                 int32_t level) const;

    const LifeNode* ClearImpl(const LifeNode* node, Vec2L pos, Rect region,
                              int32_t level);

    const LifeNode* ExtractImpl(const LifeNode* node, Vec2L pos, Rect region,
                                int32_t level) const;

    template <std::invocable<Vec2, int64_t> Func>
    static void ForEachImpl(const Func& func, const LifeNode* node, Vec2L pos,
                            int32_t level, int32_t minLevel, Rect bounds);

    static BigInt PopulationOf(const LifeNode* node);
    static int64_t PopulationOf(const LifeNode* node, bool);

    // Implementation for copy constructor / copy assignment
    void Copy(const HashQuadtree& other);

    // AdvanceNode always returns a node that is one half the size.
    // ExpandUniverse is necessary to ensure that no data is lost when HashLife
    // is executed.
    const LifeNode* ExpandUniverse(const LifeNode* node, int32_t level) const;

    // Checks if data WOULD be lost if the quadtree were advanced.
    bool NeedsExpansion(const LifeNode* node, int32_t level) const;

    // Advances a node at the specified level by `maxAdvance` generations. Early
    // exit is possible through `stopToken`.
    NodeUpdateInfo AdvanceNode(std::stop_token stopToken, const LifeNode* node,
                               int32_t level, int32_t advanceLevel) const;

    // This is the primary interface for interaction with HashLife's cache.
    static const LifeNode* FindOrCreate(const LifeNode* nw, const LifeNode* ne,
                                        const LifeNode* sw, const LifeNode* se);

    // Helper function for converting a LifeHashSet into a quadtree.
    const LifeNode* BuildTreeRegion(std::span<Vec2L> cells, Vec2L pos,
                                    int32_t level);

    // Returns an empty tree at the given level (size 2^level).
    static const LifeNode* EmptyTree(int32_t level);

    const LifeNode* BuildTree(std::span<const Vec2> data);

    // Takes the inner portions from the east and west node and constructs a new
    // node.
    const LifeNode* CenteredHorizontal(const LifeNode& west,
                                       const LifeNode& east) const;

    // Takes the inner portions from the north and south node and constructs a
    // new node.
    const LifeNode* CenteredVertical(const LifeNode& north,
                                     const LifeNode& south) const;

    // Returns the subnode centered within `node`.
    const LifeNode* CenteredSubNode(const LifeNode& node) const;

    // Converts a 2x2 set of data into a LifeNode.
    template <int32_t Size, typename T>
    const LifeNode* Combine2x2(const T& nodes, int32_t topLeftX,
                               int32_t topLeftY) const;

    // Handles the base case for HashLife (8x8 node, advances 2 generations).
    const LifeNode* AdvanceBase(const LifeNode* node) const;

    // Handles the base case for HashLife with 1-generation advancement.
    const LifeNode* AdvanceBaseOneGen(const LifeNode* node) const;

    // Handles bounded advancement.
    NodeUpdateInfo AdvanceSlow(std::stop_token stopToken, const LifeNode* node,
                               int32_t level, int32_t advanceDepth) const;
    // Handles unbounded advancement.
    NodeUpdateInfo AdvanceFast(std::stop_token stopToken, const LifeNode* node,
                               int32_t level, int32_t advanceDepth) const;

    struct CenteredNodeResult {
        const LifeNode* Node;
        Vec2L Offset;
    };
    CenteredNodeResult GetCenteredNode(int32_t level) const;

  private:
    // The rationale for storing HashLifeCache in static, thread_local storage
    // is provided above.
    static thread_local HashLifeCache s_Cache;

    const LifeNode* m_Root = FalseNode;

    // The offset when this tree was constructed, before applying expansions
    Vec2L m_SeedOffset;
    // Cached depth of the tree (number of levels from root to leaf)
    int32_t m_Depth = 0;
};

template <int32_t Size>
constexpr int32_t Index2D(int32_t x, int32_t y) {
    return y * Size + x;
}

template <int32_t Size, typename T>
const LifeNode* HashQuadtree::Combine2x2(const T& nodes, int32_t topLeftX,
                                         int32_t topLeftY) const {
    // Some functionality is provided to support bitsets here as well,
    // with a future refactor of HashLife's base case in mind.
    if constexpr (std::is_same_v<T, std::bitset<Size * Size>>) {
        constexpr static auto toNode = [](bool live) {
            return live ? TrueNode : FalseNode;
        };
        return FindOrCreate(
            toNode(nodes[Index2D<Size>(topLeftX, topLeftY)]),
            toNode(nodes[Index2D<Size>(topLeftX + 1, topLeftY)]),
            toNode(nodes[Index2D<Size>(topLeftX, topLeftY + 1)]),
            toNode(nodes[Index2D<Size>(topLeftX + 1, topLeftY + 1)]));
    } else {
        return FindOrCreate(nodes[Index2D<Size>(topLeftX, topLeftY)],
                            nodes[Index2D<Size>(topLeftX + 1, topLeftY)],
                            nodes[Index2D<Size>(topLeftX, topLeftY + 1)],
                            nodes[Index2D<Size>(topLeftX + 1, topLeftY + 1)]);
    }
}

template <std::invocable<Vec2, int64_t> Func>
void HashQuadtree::ForEachImpl(const Func& func, const LifeNode* node,
                               Vec2L pos, int32_t level, int32_t minLevel,
                               Rect bounds) {
    if (node == FalseNode || node->IsEmpty ||
        !IntersectsBounds(bounds, pos, level)) {
        return;
    }

    if (level == minLevel) {
        bool containsCells = node != FalseNode && !node->IsEmpty;
        if (containsCells && IsWithinBounds(bounds, pos)) {
            Vec2 intPos{static_cast<int32_t>(pos.X),
                        static_cast<int32_t>(pos.Y)};
            func(intPos, PopulationOf(node, true));
        }
        return;
    }

    const auto childLevel = level - 1;
    const auto halfSize = Pow2(childLevel);

    ForEachImpl(func, node->NorthWest, pos, childLevel, minLevel, bounds);
    ForEachImpl(func, node->NorthEast, {pos.X + halfSize, pos.Y}, childLevel,
                minLevel, bounds);
    ForEachImpl(func, node->SouthWest, {pos.X, pos.Y + halfSize}, childLevel,
                minLevel, bounds);
    ForEachImpl(func, node->SouthEast, {pos.X + halfSize, pos.Y + halfSize},
                childLevel, minLevel, bounds);
}

template <std::invocable<Vec2, int64_t> Func>
void HashQuadtree::ForEachCell(const Func& func, Rect bounds,
                               int32_t minLevel) const {
    if (m_Root == FalseNode) {
        return;
    }

    const auto [node, offset] = GetCenteredNode(32);
    return ForEachImpl(func, node, offset, std::min(m_Depth, 32), minLevel,
                       bounds);
}
} // namespace gol

#endif
