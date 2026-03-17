#ifndef HashQuadtree_h_
#define HashQuadtree_h_

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

#include "BigUInt.hpp"
#include "Graphics2D.hpp"
#include "LifeHashSet.hpp"

// HashQuadtree and its related free functions and structs contain all of the
// necessary representations for the HashLife algorithm. At a high level,
// HashLife uses a quadtree to represent the Game of Life universe, and
// canonicalizes nodes in a hash table. The recursive nature of the algorithm
// also allows it to jump many generations in one step.

namespace gol {
template <int32_t Size> constexpr static int32_t Index2D(int32_t x, int32_t y) {
    return y * Size + x;
}

// Represents one node of the quadtree structure.
struct LifeNode {
    // Raw pointers are used with care here. For cache efficiency, all nodes are
    // stored in a block arena, which ensures these pointers remain valid (see
    // below).
    const LifeNode* NorthWest;
    const LifeNode* NorthEast;
    const LifeNode* SouthWest;
    const LifeNode* SouthEast;

    uint64_t Hash{};      // Pre-computed hash
    BigUInt Population{}; // Recursive population of all subnodes combined

    bool IsEmpty() const { return Population.IsZero(); }

    LifeNode(const LifeNode* nw, const LifeNode* ne, const LifeNode* sw,
             const LifeNode* se)
        : NorthWest(nw), NorthEast(ne), SouthWest(sw), SouthEast(se) {

        Population = (NodePopulation(nw) + NodePopulation(ne)) +
                     (NodePopulation(sw) + NodePopulation(se));

        if consteval {
        } else {
            Hash = ComputeHash(NorthWest, NorthEast, SouthWest, SouthEast);
        }
    }

    // Computes a hash from 4 child pointers. Exposed so LifeNodeKey can reuse
    // it without constructing a full LifeNode.
    static uint64_t ComputeHash(const LifeNode* nw, const LifeNode* ne,
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

  private:
    static BigUInt NodePopulation(const LifeNode* n) {
        return n ? n->Population : BigUInt{};
    }
};

// The result of advancing a node. Tells us how many generations it advanced
// and returns the new node.
struct NodeUpdateInfo {
    const LifeNode* Node;
    int64_t Generations;
};

// Lightweight key for heterogeneous lookup into NodeMap.
// Avoids constructing a full LifeNode (which computes Population) on every
// lookup.
struct LifeNodeKey {
    const LifeNode* NorthWest;
    const LifeNode* NorthEast;
    const LifeNode* SouthWest;
    const LifeNode* SouthEast;
    uint64_t Hash;

    LifeNodeKey(const LifeNode* nw, const LifeNode* ne, const LifeNode* sw,
                const LifeNode* se)
        : NorthWest(nw), NorthEast(ne), SouthWest(sw), SouthEast(se),
          Hash(LifeNode::ComputeHash(nw, ne, sw, se)) {}
};

struct LifeNodeEqual {
    using is_transparent = void; // Flag for ankerl::unordered_dense
    bool operator()(const LifeNode* lhs, const LifeNode* rhs) const {
        if (lhs == rhs)
            return true;
        if (!lhs || !rhs)
            return false;
        return lhs->NorthWest == rhs->NorthWest &&
               lhs->NorthEast == rhs->NorthEast &&
               lhs->SouthWest == rhs->SouthWest &&
               lhs->SouthEast == rhs->SouthEast;
    }
    // Heterogeneous overload: compare LifeNode* against LifeNodeKey
    bool operator()(const LifeNode* lhs, const LifeNodeKey& rhs) const {
        if (!lhs)
            return false;
        return lhs->NorthWest == rhs.NorthWest &&
               lhs->NorthEast == rhs.NorthEast &&
               lhs->SouthWest == rhs.SouthWest &&
               lhs->SouthEast == rhs.SouthEast;
    }
    bool operator()(const LifeNodeKey& lhs, const LifeNode* rhs) const {
        return operator()(rhs, lhs);
    }
};

struct LifeNodeHash {
    using is_transparent = void;
    size_t operator()(const LifeNode* node) const;
    size_t operator()(const LifeNodeKey& key) const {
        return static_cast<size_t>(key.Hash);
    }
};

// Underlying node for TrueNode.
inline LifeNode StaticTrueNode = [] {
    LifeNode ret{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    ret.Population = BigUInt{1};
    return ret;
}();

inline const LifeNode* TrueNode = &StaticTrueNode;
inline const LifeNode* FalseNode = nullptr;
} // namespace gol

namespace gol {
// The key used for caching when HashLife has a bounded step size.
struct SlowKey {
    const LifeNode* Node;
    int64_t MaxAdvance = 0; // Max number of generations this node can advance
    auto operator<=>(const SlowKey&) const = default;
};

struct SlowHash {
    size_t operator()(const SlowKey& key) const noexcept;
};

// Block-based arena for append-only LifeNode storage. Provides pointer
// stability (blocks never move once allocated) and fast bump-pointer
// allocation. All nodes are freed in bulk when the arena is destroyed or
// cleared.
class LifeNodeArena {
  public:
    template <typename... Args> LifeNode* emplace(Args&&... args) {
        if (m_Current == BlockCapacity) {
            auto* raw = static_cast<LifeNode*>(
                ::operator new(BlockCapacity * sizeof(LifeNode)));
            m_Blocks.emplace_back(raw);
            m_Current = 0;
        }
        auto* node = m_Blocks.back().get() + m_Current++;
        std::construct_at(node, std::forward<Args>(args)...);
        return node;
    }

    // Returns the most recently emplaced node.
    LifeNode* last() const { return m_Blocks.back().get() + (m_Current - 1); }

    void clear() {
        m_Blocks.clear();
        m_Current = BlockCapacity;
    }

  private:
    static constexpr size_t BlockCapacity = 65536 / sizeof(LifeNode);

    struct BlockDeleter {
        void operator()(LifeNode* p) const { ::operator delete(p); }
    };
    std::vector<std::unique_ptr<LifeNode, BlockDeleter>> m_Blocks;
    size_t m_Current = BlockCapacity; // Force first allocation
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

    // The cache for the HashLife algorithm when the step size is bounded.
    ankerl::unordered_dense::map<SlowKey, const LifeNode*, SlowHash>
        SlowCache{};

    // Level-indexed cache for empty nodes. Index i holds the empty node for
    // size 2^i. At most 64 entries needed (levels 0..63).
    std::array<const LifeNode*, 64> EmptyNodeCache{};

    HashLifeCache() {
        NodeMap.reserve(
            1 << 20); // Reserve space for 1 million nodes to avoid rehashing
                      // during early stages of the simulation.
        SlowCache.reserve(1 << 20);
    }
};

// This is the primary data structure for executing the HashLife algorithm. It
// satisfies the requirements of `std::ranges::input_range` and can be used in
// many STL algorithms.
class HashQuadtree {
  private:
    // IteratorImpl is templated to support both `Vec2` and `const Vec2`.
    // Under the hood, a Vec2L is actually used for storage to allow for larger
    // universes. In practice, users will be restricted to only viewing the
    // cells that can be represented by Vec2.
    template <typename T> class IteratorImpl {
      private:
        // We keep track of what node we're on through a variety of factors.
        struct LifeNodeData {
            const LifeNode* node; // The node we're looking at.
            Vec2L position;       // Where this node is located, based on
                                  // HashQuadtree's offset.
            int64_t size;         // Useful for bounded iteration
            uint8_t quadrant;     // The quadrant this iterator is looking at.
                                  // Useful for knowing where to look next.
        };

        constexpr static Vec2 ConvertToVec2(Vec2L pos) {
            return Vec2{static_cast<int32_t>(pos.X),
                        static_cast<int32_t>(pos.Y)};
        }

        bool IsWithinBounds(Vec2L pos) const {
            constexpr static auto maxInt32 =
                static_cast<int64_t>(std::numeric_limits<int32_t>::max());
            constexpr static auto minInt32 =
                static_cast<int64_t>(std::numeric_limits<int32_t>::min());

            if (pos.X < minInt32 || pos.X > maxInt32 || pos.Y < minInt32 ||
                pos.Y > maxInt32) {
                return false;
            }
            if (!m_UseBounds) {
                return true;
            }

            const auto left = static_cast<int64_t>(m_Bounds.X);
            const auto top = static_cast<int64_t>(m_Bounds.Y);
            const auto right = left + m_Bounds.Width;
            const auto bottom = top + m_Bounds.Height;
            return pos.X >= left && pos.X < right && pos.Y >= top &&
                   pos.Y < bottom;
        }

        // Checks if a `size * size` square with its upper-left corner at `pos`
        // intersects with the bounds of this iterator
        bool IntersectsBounds(Vec2L pos, int64_t size) const {
            if (!m_UseBounds)
                return true;
            const auto left = static_cast<int64_t>(m_Bounds.X);
            const auto top = static_cast<int64_t>(m_Bounds.Y);
            const auto right = left + m_Bounds.Width;
            const auto bottom = top + m_Bounds.Height;
            const auto regionRight = pos.X + size;
            const auto regionBottom = pos.Y + size;
            return !(regionRight <= left || pos.X >= right ||
                     regionBottom <= top || pos.Y >= bottom);
        }

      public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::remove_const_t<T>;
        using pointer = const value_type*;
        using reference = const value_type&;

        friend HashQuadtree;

        IteratorImpl()
            : m_Current{}, m_IsEnd(true), m_UseBounds(false), m_Bounds{} {}

        IteratorImpl<T>& operator++();
        IteratorImpl<T> operator++(int);

        bool operator==(const IteratorImpl<T>& other) const;
        bool operator!=(const IteratorImpl<T>& other) const;

        reference operator*() const;
        pointer operator->() const;

      private:
        IteratorImpl(const LifeNode* root, Vec2L offset, int64_t size,
                     bool isEnd, const Rect* bounds = nullptr);
        void AdvanceToNext(); // Implementation for operator++
      private:
        std::stack<LifeNodeData> m_Stack;
        // Note that because the iterator is looking at abstract data,
        // it is actually storing the Vec2 as a proxy object.
        value_type m_Current;
        bool m_IsEnd;
        bool m_UseBounds;
        Rect m_Bounds;
    };

  public:
    HashQuadtree() = default;
    HashQuadtree(const LifeHashSet& data, Vec2 offset = {});

    HashQuadtree(const HashQuadtree& other);
    HashQuadtree& operator=(const HashQuadtree& other);

    // The move constructor and move assignment are left intentionally
    // unspecified. Copying within a thread is essentially a move because
    // all quadtrees within the same thread use the same node storage.
    // Moving across threads is not possible because storage is thread local.

    ~HashQuadtree() = default;

  public:
    using Iterator = IteratorImpl<Vec2>;
    using ConstIterator = IteratorImpl<const Vec2>;

    bool empty() const;

    Iterator begin();
    Iterator end();

    ConstIterator begin() const;
    ConstIterator end() const;

    // Returns an iterator over one region of space. This is useful for allowing
    // interaction and display of a small subsection of the universe.
    Iterator begin(Rect bounds);
    // Returns an iterator over one region of space. This is useful for allowing
    // interaction and display of a small subsection of the universe.
    ConstIterator begin(Rect bounds) const;

    const BigUInt& Population() const;

    // Advances the simulation. Passing `maxAdvance = 0` indicates that hyper
    // speed should be used. If the stop token is called during advance, the
    // function exits early, and `HashQuadtree` is left in a valid but
    // unspecified state (likely part-way through iteration).
    int64_t Advance(int64_t maxAdvance = 0, std::stop_token stopToken = {});

    // Returns the number of levels in the current tree.
    int32_t CalculateDepth() const;
    // Returns the length/width of the tree's root node.
    int64_t CalculateTreeSize() const;

    // IMPORTANT: This function must ALWAYS be called before a HashQuadtree is
    // potentially copied between threads. HashQuadtree exploits performance
    // gains from using static storage for its cache. To ensure thread-safety
    // when running multiple simulations, this storage is also THREAD LOCAL.
    // This function makes it possible for HashQuadtree to move its data from
    // static storage to a member variable, and then when it is copied in a new
    // thread, its cache can be copied once more into thread local storage.
    void PrepareCopyBetweenThreads();

    bool operator==(const HashQuadtree& other) const;
    bool operator!=(const HashQuadtree& other) const;

  private:
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
                               int32_t level, int64_t maxAdvance) const;

    // This is the primary interface for interaction with HashLife's cache.
    const LifeNode* FindOrCreate(const LifeNode* nw, const LifeNode* ne,
                                 const LifeNode* sw, const LifeNode* se) const;

    // Helper function for converting a LifeHashSet into a quadtree.
    const LifeNode* BuildTreeRegion(std::span<Vec2L> cells, Vec2L pos,
                                    int32_t level);

    // Returns an empty tree at the given level (size 2^level).
    const LifeNode* EmptyTree(int32_t level) const;

    const LifeNode* BuildTree(const LifeHashSet& data);

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
                               int32_t level, int64_t maxAdvance) const;
    // Handles unbounded advancement.
    NodeUpdateInfo AdvanceFast(std::stop_token stopToken, const LifeNode* node,
                               int32_t level, int64_t maxAdvance) const;

  private:
    // The rationale for storing HashLifeCache in static, thread_local storage
    // is provided above.
    static thread_local HashLifeCache s_Cache;
    // This is the transfer cache used for copying `s_Cache` between threads.
    std::unique_ptr<HashLifeCache> m_TransferCache{};
    const LifeNode* m_TransferRoot = nullptr;
    std::thread::id m_TransferID{std::this_thread::get_id()};

    const LifeNode* m_Root = FalseNode;
    Vec2L m_RootOffset;
    int32_t m_Depth =
        0; // Cached depth of the tree (number of levels from root to leaf)
};

// Slight build time optimization since we know the only two instantiations of
// IteratorImpl.
extern template class HashQuadtree::IteratorImpl<Vec2>;
extern template class HashQuadtree::IteratorImpl<const Vec2>;

template <typename T>
HashQuadtree::IteratorImpl<T>::IteratorImpl(const LifeNode* root, Vec2L offset,
                                            int64_t size, bool isEnd,
                                            const Rect* bounds)
    : m_Current(), m_IsEnd(isEnd), m_UseBounds(bounds != nullptr),
      m_Bounds(bounds ? *bounds : Rect{}) {
    if (!isEnd && root != FalseNode) {
        if (!m_UseBounds || IntersectsBounds(offset, size)) {
            m_Stack.push({root, offset, size, 0});
            AdvanceToNext();
        } else {
            m_IsEnd = true;
        }
    }
}

template <typename T> void HashQuadtree::IteratorImpl<T>::AdvanceToNext() {
    while (!m_Stack.empty()) {
        auto& frame = m_Stack.top();

        // If we're at a leaf (size == 1)
        if (frame.size == 1) {
            if (frame.node == TrueNode) {
                if (IsWithinBounds(frame.position)) {
                    m_Current = ConvertToVec2(frame.position);
                    m_Stack.pop();
                    return; // Found a live cell within bounds
                }
                m_Stack.pop();
                continue;
            }
            m_Stack.pop();
            continue;
        }

        // Process next quadrant
        if (frame.quadrant >= 4) {
            m_Stack.pop();
            continue;
        }

        const auto halfSize = frame.size / 2;
        const LifeNode* child = nullptr;
        auto childPos = frame.position;

        switch (frame.quadrant++) {
        case 0:
            child = frame.node->NorthWest;
            break;
        case 1:
            child = frame.node->NorthEast;
            childPos.X += halfSize;
            break;
        case 2:
            child = frame.node->SouthWest;
            childPos.Y += halfSize;
            break;
        case 3:
            child = frame.node->SouthEast;
            childPos.X += halfSize;
            childPos.Y += halfSize;
            break;
        }
        m_Stack.top().quadrant = frame.quadrant;

        if (child != FalseNode && !child->IsEmpty() &&
            IntersectsBounds(childPos, halfSize)) {
            m_Stack.push({child, childPos, halfSize, 0});
        }
    }

    m_IsEnd = true; // No more live cells
}

template <typename T>
HashQuadtree::IteratorImpl<T>& HashQuadtree::IteratorImpl<T>::operator++() {
    AdvanceToNext();
    return *this;
}

template <typename T>
HashQuadtree::IteratorImpl<T> HashQuadtree::IteratorImpl<T>::operator++(int) {
    auto copy = *this;
    AdvanceToNext();
    return copy;
}

template <typename T>
bool HashQuadtree::IteratorImpl<T>::operator==(
    const IteratorImpl<T>& other) const {
    return m_IsEnd == other.m_IsEnd &&
           (m_IsEnd || m_Current == other.m_Current);
}

template <typename T>
bool HashQuadtree::IteratorImpl<T>::operator!=(
    const IteratorImpl<T>& other) const {
    return !(*this == other);
}

template <typename T>
typename HashQuadtree::IteratorImpl<T>::reference
HashQuadtree::IteratorImpl<T>::operator*() const {
    return m_Current;
}

template <typename T>
typename HashQuadtree::IteratorImpl<T>::pointer
HashQuadtree::IteratorImpl<T>::operator->() const {
    return &m_Current;
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

} // namespace gol

#endif
