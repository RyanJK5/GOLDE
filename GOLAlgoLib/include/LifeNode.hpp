#ifndef LifeNode_hpp_
#define LifeNode_hpp_

#include <cstdint>
#include <memory>
#include <vector>

#include "BigInt.hpp"
#include "Graphics2D.hpp"

namespace Golde {
// Represents one node of the quadtree structure.
struct LifeNode {
    // Raw pointers are used with care here. For cache efficiency, all nodes are
    // stored in a block arena, which ensures these pointers remain valid (see
    // below).
    const LifeNode* NorthWest;
    const LifeNode* NorthEast;
    const LifeNode* SouthWest;
    const LifeNode* SouthEast;

    union {
        uint64_t Hash{};    // Pre-computed hash
        LifeNode* NextDead; // For garbage collection
    };

    bool IsEmpty = false;
    mutable bool MarkedForGC = false;

    constexpr LifeNode(const LifeNode* nw, const LifeNode* ne,
                       const LifeNode* sw, const LifeNode* se);

    // Computes a hash from 4 child pointers.
    static uint64_t ComputeHash(const LifeNode* nw, const LifeNode* ne,
                                const LifeNode* sw, const LifeNode* se);
};

constexpr inline const LifeNode* FalseNode = nullptr;

// The result of advancing a node. Tells us how many generations it advanced
// and returns the new node.
struct NodeUpdateInfo {
    const LifeNode* Node;
    int32_t AdvanceLevel;
};

// Extracts the four 16-bit quadrant encodings from a level-3 node.
struct LeafQuadrants {
    uint16_t nw, ne, sw, se;
};

LeafQuadrants EncodeLevel3(const LifeNode* node);

bool IsWithinBounds(Rect bounds, Vec2L pos);
bool IsWithinBounds(const RectL& bounds, Vec2L pos);
bool IsWithinBounds(const BigRect& bounds, Vec2L pos);
bool IsWithinBounds(const BigRect& bounds, const BigVec2& pos);

// Checks if a `size * size` square with its upper-left corner at `pos`
// intersects with the bounds of this iterator
bool IntersectsBounds(Rect bounds, Vec2L pos, int32_t level);
bool IntersectsBounds(const RectL& bounds, Vec2L pos, int32_t level);
bool IntersectsBounds(const BigRect& bounds, Vec2L pos, int32_t level);
bool IntersectsBounds(const BigRect& bounds, const BigVec2& pos,
                      const BigInt& size);

struct LifeNodeEqual {
    using is_transparent = void; // Flag for ankerl::unordered_dense

    bool operator()(const LifeNode* lhs, const LifeNode* rhs) const;
};

struct LifeNodeHash {
    using is_transparent = void;
    using is_avalanching = void;
    size_t operator()(const LifeNode* node) const;
};

// Block-based arena for append-only LifeNode storage. Provides pointer
// stability (blocks never move once allocated) and fast bump-pointer
// allocation. All nodes are freed in bulk when the arena is destroyed or
// cleared.
class LifeNodeArena {
  public:
    const LifeNode* emplace(const LifeNode* nw, const LifeNode* ne,
                            const LifeNode* sw, const LifeNode* se);

    void Clear();

    void SweepGarbage();

  private:
    constexpr static auto BlockCapacity = 65536UZ / sizeof(LifeNode);

    struct BlockDeleter {
        void operator()(LifeNode* p) const;
    };

    std::vector<std::unique_ptr<LifeNode, BlockDeleter>> m_Blocks;
    LifeNode* m_DeadNodeHead = nullptr;

    size_t m_Current = BlockCapacity; // Force first allocation
};

constexpr LifeNode::LifeNode(const LifeNode* nw, const LifeNode* ne,
                             const LifeNode* sw, const LifeNode* se)
    : NorthWest(nw), NorthEast(ne), SouthWest(sw), SouthEast(se) {
    if !consteval {
        IsEmpty = (nw ? nw->IsEmpty : true) && (ne ? ne->IsEmpty : true) &&
                  (sw ? sw->IsEmpty : true) && (se ? se->IsEmpty : true);
        Hash = ComputeHash(NorthWest, NorthEast, SouthWest, SouthEast);
    }
}

// Underlying node for TrueNode.
constexpr inline LifeNode StaticTrueNode{nullptr, nullptr, nullptr, nullptr};

constexpr inline const LifeNode* TrueNode = &StaticTrueNode;

constexpr int64_t Pow2(std::integral auto exponent) {
    return int64_t{1} << exponent;
}

} // namespace Golde

#endif