#include "HashLifeCache.hpp"

namespace Golde {
HashLifeCache::HashLifeCache() {
    EmptyNodeCache.resize(64, nullptr);
    NodeMap.reserve(1UZ << 20UZ);
    SlowCache.reserve(1UZ << 20UZ);
}

// Mixes the node's precomputed hash with MaxAdvance. The node hash is already
// well-distributed via splitmix64, so a single round of xor-shift mixing with
// the advance count is sufficient.
size_t SlowHash::operator()(SlowKey key) const noexcept {
    // Use a non-zero seed to prevent (0,0) from hashing to 0
    // This is a prime or a large constant like 0x9e3779b9
    auto h = key.Node ? key.Node->Hash : 0xD3212C32483522FBULL;

    const auto advanceHash = static_cast<uint64_t>(key.AdvanceLevel);

    // Multiplicative combine using the Golden Ratio
    // We shift 'h' to ensure the nodeHash and advanceHash don't
    // just sit in the same bit-lanes before the multiply.
    constexpr static auto GoldenRatio = 0x9E3779B97F4A7C15ULL;
    h ^= (advanceHash * GoldenRatio) + 0x9e3779b9 + (h << 6) + (h >> 2);

    // The "Avalanche": A faster, lighter version of Murmur's mixer.
    // This ensures that changes in AdvanceLevel propagate
    // across the entire 64-bit result.
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;

    return static_cast<size_t>(h);
}

void HashLifeCache::SetRule(const LifeRule& rule) {
    if (m_Rule == rule) {
        return;
    }

    m_Rule = rule;

    std::ranges::for_each(
        NodeMap, [](const LifeNode* node) { node->AdvanceResult = nullptr; });
    SlowCache.clear();
}

const LifeRule& HashLifeCache::GetRule() const { return m_Rule; }

const LifeNode* HashLifeCache::FindOrCreate(const LifeNode* nw,
                                            const LifeNode* ne,
                                            const LifeNode* sw,
                                            const LifeNode* se) {
    LifeNode key{nw, ne, sw, se};
    if (const auto itr = NodeMap.find(&key); itr != NodeMap.end()) {
        return *itr;
    }

#ifdef GOLDE_GARBAGE_COLLECTION
    if (NodeMap.size() >= 1'000'000) {
        auto guard1 = ProtectNodeFromGC(nw);
        auto guard2 = ProtectNodeFromGC(ne);
        auto guard3 = ProtectNodeFromGC(sw);
        auto guard4 = ProtectNodeFromGC(se);
        MarkAndSweep(m_Root);
    }
#endif

    const auto* node = NodeStorage.emplace(nw, ne, sw, se);
    NodeMap.insert(node);
    return node;
}

#ifdef GOLDE_GARBAGE_COLLECTION

void HashLifeCache::Mark(const LifeNode* root) {
    std::stack<const LifeNode*, std::vector<const LifeNode*>> stack;
    stack.push(root);

    while (!stack.empty()) {
        const auto* node = stack.top();
        stack.pop();

        if (node == FalseNode || node == TrueNode || node->MarkedForGC) {
            continue;
        }

        node->MarkedForGC = true;

        if (const auto it = NodeMap.find(node); it != NodeMap.end()) {
            stack.push((*it)->AdvanceResult);
        }

        stack.push(node->NorthWest);
        stack.push(node->NorthEast);
        stack.push(node->SouthWest);
        stack.push(node->SouthEast);
    }
}

void HashLifeCache::MarkAndSweep(const LifeNode* root) {
    Mark(EmptyNodeCache.back());
    Mark(root);
    for (const auto* node : m_ProtectedRoots) {
        Mark(node);
    }

    decltype(NodeMap) newCache{};
    newCache.reserve(NodeMap.size());

    for (const auto* node : NodeMap) {
        if (node != nullptr && node->MarkedForGC) {
            newCache.insert(node);
        }
    }

    NodeMap = std::move(newCache);

    NodeStorage.SweepGarbage();
}

#endif

} // namespace Golde