#ifndef HashLifeCache_hpp_
#define HashLifeCache_hpp_

#include <ankerl/unordered_dense.h>

#include "LifeNode.hpp"
#include "LifeRule.hpp"

namespace Golde {
// The cache used for the HashLife algorithm.

// The key used for caching when HashLife has a bounded step size.
struct SlowKey {
    const LifeNode* Node;
    int32_t AdvanceLevel = 0; // Max number of generations this node can advance
    auto operator<=>(const SlowKey&) const = default;
};

struct SlowHash {
    size_t operator()(SlowKey key) const noexcept;
};

class HashLifeCache {
  public:
    HashLifeCache();

    // Bump-pointer arena where all LifeNodes are stored. Nodes are only
    // accessed by pointer outside of the cache.
    LifeNodeArena NodeStorage{};

    ankerl::unordered_dense::set<const LifeNode*, LifeNodeHash, LifeNodeEqual>
        NodeMap{};

    // Level-indexed cache for empty nodes. Index i holds the empty node for
    // size 2^i
    std::vector<const LifeNode*> EmptyNodeCache{};

    using SlowCacheType =
        ankerl::unordered_dense::map<SlowKey, const LifeNode*, SlowHash>;
    SlowCacheType SlowCache;

    void SetRule(const LifeRule& rule);
    const LifeRule& GetRule() const;

    const LifeNode* FindOrCreate(const LifeNode* nw, const LifeNode* ne, const LifeNode* sw, const LifeNode* se);
private:
    LifeRule m_Rule = *LifeRule::Make("B3/S23");
#ifdef GOLDE_GARBAGE_COLLECTION
public:
    // Store the return value in a scope where the node should be protected from
    // garbage collection.
    [[nodiscard]] auto ProtectNodeFromGC(const LifeNode* node) {
        struct GCRootGuard {
            std::vector<const LifeNode*>& ProtectStack;
            size_t InitialSize;
            GCRootGuard(std::vector<const LifeNode*>& stack, const LifeNode* n)
                : ProtectStack(stack), InitialSize(stack.size()) {
                ProtectStack.push_back(n);
            }

            ~GCRootGuard() { ProtectStack.resize(InitialSize); }

            const LifeNode* Protect(const LifeNode* node) {
                ProtectStack.push_back(node);
                return node;
            }
        };

        return GCRootGuard{m_ProtectedRoots, node};
    }
    
    void MarkAndSweep(const LifeNode* root);
  private:
    std::vector<const LifeNode*> m_ProtectedRoots{};
    
    void Mark(const LifeNode* node);
#endif
};

}

#endif