#include <gtest/gtest.h>

#include "HashQuadtree.hpp"
#include "LifeAlgorithm.hpp"
#include "LifeRule.hpp"
#include "Plane.hpp"
#include "Torus.hpp"

namespace Golde {
namespace {
class DummyDataStructure : public LifeDataStructure {
  public:
    void Set(Vec2, bool) override {}

    void Clear(Rect) override {}

    bool Get(Vec2) const override { return false; }

    Rect FindBoundingBox() const override { return {}; }
};

class TestAlgorithm : public LifeAlgorithm {
  public:
    TestAlgorithm() = default;

    TestAlgorithm(const TestAlgorithm& other)
        : m_Topology(other.m_Topology ? other.m_Topology->Clone() : nullptr),
          m_Rule(other.m_Rule) {}

    BigInt Step(LifeDataStructure& data, const BigInt& numSteps,
                std::stop_token = {}) override {
        if (numSteps.is_zero()) {
            return BigZero;
        }

        auto& tree = dynamic_cast<HashQuadtree&>(data);
        const auto ruleBounds = m_Rule.Bounds();
        const auto topologyBounds =
            m_Topology ? m_Topology->GetBounds() : std::optional<Rect>{};
        const auto bounds = ruleBounds ? ruleBounds : topologyBounds;

        auto x = bounds ? bounds->Width - 1 : 0;
        auto y = bounds ? bounds->Height - 1 : 0;

        if (m_Rule.GetTopology() == TopologyKind::Torus) {
            ++x;
        }
        if (m_Topology && m_Topology->GetIdentifier() == "Torus") {
            ++y;
        }

        tree.Set({x, y}, true);
        return BigInt{1};
    }

    void SetTopology(std::unique_ptr<Topology> topology) override {
        m_Topology = std::move(topology);
    }

    void SetRule(const LifeRule& rule) override { m_Rule = rule; }

    bool CompatibleWith(const LifeDataStructure& data) const override {
        return typeid(HashQuadtree) == typeid(data);
    }

    std::string_view GetIdentifier() const override { return "TestAlgorithm"; }

    std::unique_ptr<LifeAlgorithm> Clone() const override {
        return std::make_unique<TestAlgorithm>(*this);
    }

  private:
    std::unique_ptr<Topology> m_Topology;
    LifeRule m_Rule{1 << 3, (1 << 2) | (1 << 3)};
};
} // namespace

TEST(DummyAlgorithmTest, IdentifierCompatibilityAndClone) {
    TestAlgorithm algo{};

    EXPECT_EQ(algo.GetIdentifier(), "TestAlgorithm");

    HashQuadtree tree{};
    DummyDataStructure dummy{};
    EXPECT_TRUE(algo.CompatibleWith(tree));
    EXPECT_FALSE(algo.CompatibleWith(dummy));

    algo.SetTopology(std::make_unique<Plane>(Rect{0, 0, 4, 4}));
    algo.SetRule(*LifeRule::Make("B3/S23"));

    const auto clone = algo.Clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->GetIdentifier(), algo.GetIdentifier());

    HashQuadtree originalTree{};
    HashQuadtree clonedTree{};
    EXPECT_EQ(algo.Step(originalTree, BigInt{1}), BigInt{1});
    EXPECT_EQ(clone->Step(clonedTree, BigInt{1}), BigInt{1});
    EXPECT_EQ(originalTree, clonedTree);
}

TEST(DummyAlgorithmTest, RuleAndTopologyAffectStepTarget) {
    TestAlgorithm algo{};

    algo.SetTopology(std::make_unique<Plane>(Rect{0, 0, 4, 4}));
    algo.SetRule(*LifeRule::Make("B3/S23"));

    HashQuadtree planeTree{};
    EXPECT_EQ(algo.Step(planeTree, BigInt{1}), BigInt{1});
    EXPECT_TRUE(planeTree.Get({3, 3}));
    EXPECT_FALSE(planeTree.Get({4, 4}));

    algo.SetTopology(std::make_unique<Torus>(Rect{0, 0, 4, 4}));
    algo.SetRule(*LifeRule::Make("B36/S23:T4,4"));

    HashQuadtree torusTree{};
    EXPECT_EQ(algo.Step(torusTree, BigInt{1}), BigInt{1});
    EXPECT_TRUE(torusTree.Get({4, 4}));
}

TEST(DummyAlgorithmTest, ZeroStepsDoNotMutate) {
    TestAlgorithm algo{};
    HashQuadtree tree{};

    EXPECT_EQ(algo.Step(tree, BigInt{0}), BigZero);
    EXPECT_TRUE(tree.empty());
}

} // namespace Golde