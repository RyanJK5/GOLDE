#include <cstdint>
#include <memory>

namespace gol {
// Represents one node of the quadtree structure.
struct LifeNode {
    // Raw pointers are used with care here. For cache efficiency, all nodes are
    // stored in a block arena, which ensures these pointers remain valid (see
    // below).
    const LifeNode* NorthWest;
    const LifeNode* NorthEast;
    const LifeNode* SouthWest;
    const LifeNode* SouthEast;

    uint64_t Hash{}; // Pre-computed hash
    bool IsEmpty = false;

    constexpr LifeNode(const LifeNode* nw, const LifeNode* ne,
                       const LifeNode* sw, const LifeNode* se);

    // Computes a hash from 4 child pointers. Exposed so LifeNodeKey can reuse
    // it without constructing a full LifeNode.
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
                const LifeNode* se);
};

struct LifeNodeEqual {
    using is_transparent = void; // Flag for ankerl::unordered_dense

    bool operator()(const LifeNode* lhs, const LifeNode* rhs) const;

    // Heterogeneous overload: compare LifeNode* against LifeNodeKey
    bool operator()(const LifeNode* lhs, const LifeNodeKey& rhs) const;

    bool operator()(const LifeNodeKey& lhs, const LifeNode* rhs) const;
};

struct LifeNodeHash {
    using is_transparent = void;
    size_t operator()(const LifeNode* node) const;
    size_t operator()(const LifeNodeKey& key) const;
};

// Block-based arena for append-only LifeNode storage. Provides pointer
// stability (blocks never move once allocated) and fast bump-pointer
// allocation. All nodes are freed in bulk when the arena is destroyed or
// cleared.
class LifeNodeArena {
  public:
    template <typename... Args>
    LifeNode* emplace(Args&&... args);

    // Returns the most recently emplaced node.
    LifeNode* last() const;

    void clear();

  private:
    static constexpr size_t BlockCapacity = 65536 / sizeof(LifeNode);

    struct BlockDeleter {
        void operator()(LifeNode* p) const;
    };
    std::vector<std::unique_ptr<LifeNode, BlockDeleter>> m_Blocks;
    size_t m_Current = BlockCapacity; // Force first allocation
};

constexpr LifeNode::LifeNode(const LifeNode* nw, const LifeNode* ne,
                             const LifeNode* sw, const LifeNode* se)
    : NorthWest(nw), NorthEast(ne), SouthWest(sw), SouthEast(se) {
    if consteval {
    } else {
        IsEmpty = (nw ? nw->IsEmpty : true) && (ne ? ne->IsEmpty : true) &&
                  (sw ? sw->IsEmpty : true) && (se ? se->IsEmpty : true);
        Hash = ComputeHash(NorthWest, NorthEast, SouthWest, SouthEast);
    }
}

// Underlying node for TrueNode.
constexpr inline LifeNode StaticTrueNode{nullptr, nullptr, nullptr, nullptr};

constexpr inline const LifeNode* TrueNode = &StaticTrueNode;

template <typename... Args>
LifeNode* LifeNodeArena::emplace(Args&&... args) {
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

} // namespace gol
