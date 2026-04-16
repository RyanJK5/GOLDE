#ifndef HashQuadtree_hpp_
#define HashQuadtree_hpp_

#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <array>
#include <bit>
#include <bitset>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <stack>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "BigInt.hpp"
#include "Graphics2D.hpp"
#include "LifeDataStructure.hpp"
#include "LifeHashSet.hpp"
#include "LifeNode.hpp"
#include "LifeRule.hpp"

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

    ankerl::unordered_dense::map<const LifeNode*, const LifeNode*, LifeNodeHash,
                                 LifeNodeEqual>
        NodeMap;

    ankerl::unordered_dense::map<const LifeNode*, BigInt, LifeNodeHash,
                                 LifeNodeEqual>
        PopulationCache{};

    ankerl::unordered_dense::map<const LifeNode*, int64_t, LifeNodeHash,
                                 LifeNodeEqual>
        SmallPopulationCache{};

    // Level-indexed cache for empty nodes. Index i holds the empty node for
    // size 2^i. At most 64 entries needed (levels 0..63).
    std::vector<const LifeNode*> EmptyNodeCache{};

    HashLifeCache();
};

// This is the primary data structure for executing the HashLife algorithm. It
// satisfies the requirements of `std::ranges::input_range` and can be used in
// many STL algorithms.
class HashQuadtree : public LifeDataStructure {
  public:
    // The maximum number of distinct caches that can be stored in static memory.
    constexpr inline static auto MaxCacheCount = 200UZ;

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
    HashQuadtree();
    HashQuadtree(std::span<const Vec2> data, Vec2 offset = {});

    static void SetCacheIndex(size_t index);
  public:
    bool empty() const;

    Iterator begin() const;
    Iterator end() const;

    // Returns an iterator over one region of space. This is useful for allowing
    // interaction and display of a small subsection of the universe.
    Iterator begin(Rect bounds) const;

    BigInt Population() const;

    // Applies func to all cells within the given bounds. More efficient than
    // iterators because recursion can be used.
    template <std::invocable<Vec2, int64_t> Func>
    void ForEachCell(const Func& func, Rect bounds, int32_t minLevel) const;

    template <std::invocable<const BigVec2&, int64_t> Func>
    void ForEachCell(const Func& func, const BigRect& bounds,
                     int32_t minLevel) const;

    // Returns the number of levels in the current tree.
    int32_t CalculateDepth() const;
    // Returns the length/width of the tree's root node.
    int64_t CalculateTreeSize() const;

    bool operator==(const HashQuadtree& other) const;
    bool operator!=(const HashQuadtree& other) const;

    void Set(Vec2 pos, bool alive) override;

    // Inserts `other` shifted by `offset` into this tree.
    // This operation can merge aligned subtrees directly to avoid
    // per-cell insertion in common cases.
    void Insert(const HashQuadtree& other, Vec2 offset);

    bool Get(Vec2 pos) const override;

    void Clear(Rect region) override;

    HashQuadtree Extract(Rect region) const;

    Rect FindBoundingBox() const override;

    // This is the primary interface for interaction with HashLife's cache.
    const LifeNode* FindOrCreate(const LifeNode* nw, const LifeNode* ne,
                                 const LifeNode* sw, const LifeNode* se) const;

    // Returns an empty tree at the given level (size 2^level).
    const LifeNode* EmptyTree(int32_t level) const;

    std::optional<const LifeNode*> Find(const LifeNode* node) const;

    void CacheResult(const LifeNode* key, const LifeNode* value) const;

    static void ClearCache();

    void ExpandUniverse(int32_t targetLevel);
    const LifeNode* ExpandNode(const LifeNode* node, int32_t level) const;

    const LifeNode* Data() const;
    void OverwriteData(const LifeNode* root, int32_t level);

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
    void ForEachImpl(const Func& func, const LifeNode* node, Vec2L pos,
                            int32_t level, int32_t minLevel, Rect bounds) const;

    template <std::invocable<const BigVec2&, int64_t> Func>
    void
    ForEachBigImpl(const Func& func, const LifeNode* node, int32_t level,
                   int32_t minLevel, const BigInt& left, const BigInt& top,
                   const BigInt& right, const BigInt& bottom,
                   const BigInt& boundsLeft, const BigInt& boundsTop,
                   const BigInt& boundsRight, const BigInt& boundsBottom) const;

    BigInt PopulationOf(const LifeNode* node) const;
    int64_t PopulationOf(const LifeNode* node, bool) const;

    // Helper function for converting a LifeHashSet into a quadtree.
    const LifeNode* BuildTreeRegion(std::span<Vec2L> cells, Vec2L pos,
                                    int32_t level);

    const LifeNode* BuildTree(std::span<const Vec2> data);

    struct CenteredNodeResult {
        const LifeNode* Node;
        Vec2L Offset;
    };

    enum class Quadrant { NW, NE, SW, SE };

    CenteredNodeResult GetCenteredNode(int32_t level) const;

    const LifeNode* ReplaceAlongPath(const LifeNode* node, int32_t level,
                                     Quadrant sdir, const LifeNode* value,
                                     int32_t targetLevel) const;
    const LifeNode* SetCenteredNode(const LifeNode* outer, int32_t currentLevel,
                                    const LifeNode* toInsert,
                                    int32_t level) const;

    const LifeNode* OverlayNodes(const LifeNode* a, const LifeNode* b,
                                 int32_t level) const;
    std::optional<const LifeNode*>
    TryOverlayAlignedImpl(const LifeNode* destNode, int32_t destLevel,
                          Vec2L destPos, const LifeNode* srcNode,
                          int32_t srcLevel, Vec2L srcPos) const;
    const LifeNode* InsertNodeImpl(const LifeNode* destNode, int32_t destLevel,
                                   Vec2L destPos, const LifeNode* srcNode,
                                   int32_t srcLevel, Vec2L srcPos) const;

  private:
    static std::array<HashLifeCache, MaxCacheCount> s_Cache;
    static thread_local size_t s_CacheIndex;

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

template <std::invocable<Vec2, int64_t> Func>
void HashQuadtree::ForEachImpl(const Func& func, const LifeNode* node,
                               Vec2L pos, int32_t level, int32_t minLevel,
                               Rect bounds) const {
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

template <std::invocable<const BigVec2&, int64_t> Func>
void HashQuadtree::ForEachCell(const Func& func, const BigRect& bounds,
                               int32_t minLevel) const {
    if (m_Root == FalseNode) {
        return;
    }

    const auto clampedMinLevel = std::clamp(minLevel, 0, m_Depth);
    const auto half = (m_Depth == 0) ? BigZero : (BigOne << (m_Depth - 1));
    const auto left = BigInt{m_SeedOffset.X} - half;
    const auto top = BigInt{m_SeedOffset.Y} - half;
    const auto right = left + (BigOne << m_Depth);
    const auto bottom = top + (BigOne << m_Depth);

    const auto boundsLeft = bounds.X;
    const auto boundsTop = bounds.Y;
    const auto boundsRight = bounds.X + bounds.Width;
    const auto boundsBottom = bounds.Y + bounds.Height;

    return ForEachBigImpl(func, m_Root, m_Depth, clampedMinLevel, left, top,
                          right, bottom, boundsLeft, boundsTop, boundsRight,
                          boundsBottom);
}

template <std::invocable<const BigVec2&, int64_t> Func>
void HashQuadtree::ForEachBigImpl(
    const Func& func, const LifeNode* node, int32_t level, int32_t minLevel,
    const BigInt& left, const BigInt& top, const BigInt& right,
    const BigInt& bottom, const BigInt& boundsLeft, const BigInt& boundsTop,
    const BigInt& boundsRight, const BigInt& boundsBottom) const {
    // Intersects check — no temporaries on the bounds side
    if (node == FalseNode || node->IsEmpty || right <= boundsLeft ||
        left >= boundsRight || bottom <= boundsTop || top >= boundsBottom) {
        return;
    }

    if (level == minLevel) {
        if (left >= boundsLeft && left < boundsRight && top >= boundsTop &&
            top < boundsBottom) {
            func(BigVec2{left, top}, PopulationOf(node, true));
        }
        return;
    }

    // Computed once, shared across all four children
    const auto midX = left + ((right - left) >> 1);
    const auto midY = top + ((bottom - top) >> 1);

    const auto childLevel = level - 1;

    // NW: (left, top, midX, midY)
    ForEachBigImpl(func, node->NorthWest, childLevel, minLevel, left, top, midX,
                   midY, boundsLeft, boundsTop, boundsRight, boundsBottom);
    // NE: reuses right, top, midY — one new value is midX
    ForEachBigImpl(func, node->NorthEast, childLevel, minLevel, midX, top,
                   right, midY, boundsLeft, boundsTop, boundsRight,
                   boundsBottom);
    // SW: reuses left, bottom, midX — one new value is midY
    ForEachBigImpl(func, node->SouthWest, childLevel, minLevel, left, midY,
                   midX, bottom, boundsLeft, boundsTop, boundsRight,
                   boundsBottom);
    // SE: reuses right, bottom — midX and midY already computed
    ForEachBigImpl(func, node->SouthEast, childLevel, minLevel, midX, midY,
                   right, bottom, boundsLeft, boundsTop, boundsRight,
                   boundsBottom);
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
