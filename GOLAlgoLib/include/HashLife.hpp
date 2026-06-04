#ifndef HashLife_hpp_
#define HashLife_hpp_

#include <concepts>

#include "HashLifeCache.hpp"
#include "HashQuadtree.hpp"
#include "LifeAlgorithm.hpp"

namespace Golde {

class HashLife : public LifeAlgorithm {
  public:
    HashLife(const LifeRule& rule);

    HashLife(const LifeRule& rule, std::unique_ptr<Topology> topology);

    void SetTopology(std::unique_ptr<Topology> topology) override;

    void SetRule(const LifeRule& rule) override;

    bool CompatibleWith(const LifeDataStructure& data) const override;

    BigInt Step(LifeDataStructure& data, const BigInt& numSteps,
                std::stop_token stopToken = {}) override;

    std::string_view GetIdentifier() const override;

    std::unique_ptr<LifeAlgorithm> Clone() const override;

  private:
    template <bool UseFastPath = false>
    int32_t DoOneJump();

    template <bool UseFastPath = false>
    NodeUpdateInfo AdvanceNode(const LifeNode* node, int32_t level) const;

    NodeUpdateInfo AdvanceSlow(const LifeNode* node, int32_t level) const;

    template <bool UseFastPath = false>
    NodeUpdateInfo AdvanceFast(const LifeNode* node, int32_t level) const;

    const LifeNode* AdvanceBase(const LifeNode* node) const;

    const LifeNode* AdvanceBaseOneGen(const LifeNode* node) const;

    const LifeNode* CenteredHorizontal(const LifeNode& west,
                                       const LifeNode& east) const;
    const LifeNode* CenteredVertical(const LifeNode& north,
                                     const LifeNode& south) const;
    const LifeNode* CenteredSubNode(const LifeNode& node) const;

    bool NeedsExpansion(const LifeNode* node, int32_t level) const;

    struct FirstGenResults {
        uint16_t nw, n, ne, w, center, e, sw, s, se;
    };

    FirstGenResults ComputeFirstGeneration(const LeafQuadrants& q) const;
    uint16_t AssembleCentered6x6(const FirstGenResults& gen1) const;

  private:
    std::reference_wrapper<const LifeRule> m_Rule;
    std::unique_ptr<Topology> m_Topology;

    // These variables are stored to reduce the size of the
    // AdvanceFast/AdvanceSlow stack frame.
    HashQuadtree* m_StepData = nullptr;
    std::stop_token m_StepStopToken{};
    int32_t m_StepAdvanceDepth = 0;
};

template <bool UseFastPath>
NodeUpdateInfo HashLife::AdvanceNode(const LifeNode* node,
                                     int32_t level) const {
    if (m_StepStopToken.stop_requested())
        return {node, 0};
    if (node == FalseNode || level < 3)
        return {node, 0};

    if constexpr (UseFastPath) {
        return AdvanceFast<true>(node, level);
    } else {
        if (m_StepAdvanceDepth >= 0) {
            if (level - 2 > m_StepAdvanceDepth)
                return AdvanceSlow(node, level);
        }
        return AdvanceFast(node, level);
    }
}

template <bool UseFastPath>
NodeUpdateInfo HashLife::AdvanceFast(const LifeNode* node,
                                     int32_t level) const {
    // At a high level, we want to assemble a node that is half the size of
    // `node`, but centered at the same point. By following this logic all the
    // way down the recursion, we are able to safely advance the entire universe
    // without having to worry about any cells on the boundary of the universe.
    // To achieve this end, we create a grid of overlapping cells centered
    // around `node`'s center, and tactically combine them to form the four
    // quadrants of the center node that is half the size. THe key to this
    // process is that the two levels are made by recursively calling
    // AdvanceFast, which allows for logarithmic time progression.

    if constexpr (UseFastPath) {
        if (m_StepStopToken.stop_requested()) {
            return {node, 0};
        }
    }

#ifdef GOLDE_GARBAGE_COLLECTION
    auto guard = m_StepData->ProtectNodeFromGC(node);
#endif

    if (node == FalseNode)
        return {FalseNode, 0};

    if (const auto result = m_StepData->Find(node)) {
        return {*result, level - 2};
    }

    if (level == 3) {
        const auto* base = AdvanceBase(node);
        node->AdvanceResult = base;
        return {base, 1};
    }

    const auto advanceFunc =
        [&]<typename... Args>(Args&&... args) -> NodeUpdateInfo {
        auto [node, advanceLevel] = [&] {
            if constexpr (UseFastPath) {
                return AdvanceFast<true>(std::forward<Args>(args)...);
            } else {
                return AdvanceNode(std::forward<Args>(args)...);
            }
        }();
#ifdef GOLDE_GARBAGE_COLLECTION
        return {guard.Protect(node), advanceLevel};
#else
        return {node, advanceLevel};
#endif
    };

    const auto n00 = advanceFunc(node->NorthWest, level - 1);
    const auto n01 = advanceFunc(
        CenteredHorizontal(*node->NorthWest, *node->NorthEast), level - 1);
    const auto n02 = advanceFunc(node->NorthEast, level - 1);
    const auto n10 = advanceFunc(
        CenteredVertical(*node->NorthWest, *node->SouthWest), level - 1);
    const auto n11 = advanceFunc(CenteredSubNode(*node), level - 1);
    const auto n12 = advanceFunc(
        CenteredVertical(*node->NorthEast, *node->SouthEast), level - 1);
    const auto n20 = advanceFunc(node->SouthWest, level - 1);
    const auto n21 = advanceFunc(
        CenteredHorizontal(*node->SouthWest, *node->SouthEast), level - 1);
    const auto n22 = advanceFunc(node->SouthEast, level - 1);

    const auto topLeft = advanceFunc(
        m_StepData->FindOrCreate(n00.Node, n01.Node, n10.Node, n11.Node),
        level - 1);
    const auto topRight = advanceFunc(
        m_StepData->FindOrCreate(n01.Node, n02.Node, n11.Node, n12.Node),
        level - 1);
    const auto bottomLeft = advanceFunc(
        m_StepData->FindOrCreate(n10.Node, n11.Node, n20.Node, n21.Node),
        level - 1);
    const auto bottomRight = advanceFunc(
        m_StepData->FindOrCreate(n11.Node, n12.Node, n21.Node, n22.Node),
        level - 1);

    const auto* result = m_StepData->FindOrCreate(
        topLeft.Node, topRight.Node, bottomLeft.Node, bottomRight.Node);

    if (m_StepStopToken.stop_requested()) {
        return {node, 0};
    }

    node->AdvanceResult = result;
    return {result, level - 2};
}

template <bool UseFastPath>
int32_t HashLife::DoOneJump() {
    if (m_StepData->Data() == FalseNode)
        return {};

    m_Topology->PrepareBorderCells(*m_StepData);

    // The condition in this while loop is to prevent freezing when the
    // user asks for a large step size on a small pattern. For example, running
    // hyperspeed on a 2x2 block will not cause it to advance particularly fast
    // since it exhibits no expansion, but if maxAdvance is specified to 2^32,
    // we can make it happen instantly.
    const auto* root = m_StepData->Data();
    auto depth = m_StepData->CalculateDepth();
    while (NeedsExpansion(root, depth) || depth - 2 < m_StepAdvanceDepth) {
        root = m_StepData->ExpandNode(root, depth);
        depth++;
    }

    const auto advanced = AdvanceNode<UseFastPath>(root, depth);

    if (m_StepStopToken.stop_requested()) {
        return 0;
    }

    m_StepData->OverwriteData(advanced.Node, depth - 1);
    m_Topology->CleanupBorderCells(*m_StepData);

    return advanced.AdvanceLevel;
}

} // namespace Golde

#endif