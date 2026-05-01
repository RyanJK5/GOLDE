#include <gtest/gtest.h>

#include "HashQuadtree.hpp"
#include "LifeDataStructure.hpp"
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
} // namespace

TEST(TopologyTest, PlaneIdentifierCloneAndCompatibility) {
    Plane plane{Rect{0, 0, 4, 4}};

    EXPECT_EQ(plane.GetIdentifier(), "Plane");
    EXPECT_TRUE(plane.GetBounds().has_value());
    EXPECT_EQ(plane.GetBounds()->Width, 4);

    const auto clone = plane.Clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->GetIdentifier(), "Plane");
    EXPECT_EQ(clone->GetBounds(), plane.GetBounds());

    HashQuadtree tree{};
    DummyDataStructure dummy{};
    EXPECT_TRUE(plane.CompatibleWith(tree));
    EXPECT_FALSE(plane.CompatibleWith(dummy));
}

TEST(TopologyTest, PlaneCleansUpOutsideBounds) {
    Plane plane{Rect{0, 0, 4, 4}};
    constexpr static std::array cells{Vec2{0, 0}, Vec2{3, 3}, Vec2{4, 4}};
    HashQuadtree tree{cells};

    plane.CleanupBorderCells(tree);

    EXPECT_TRUE(tree.Get({0, 0}));
    EXPECT_TRUE(tree.Get({3, 3}));
    EXPECT_FALSE(tree.Get({4, 4}));
}

TEST(TopologyTest, TorusIdentifierCloneAndCompatibility) {
    Torus torus{Rect{0, 0, 4, 4}};

    EXPECT_EQ(torus.GetIdentifier(), "Torus");
    EXPECT_TRUE(torus.GetBounds().has_value());
    EXPECT_EQ(torus.GetBounds()->Height, 4);

    const auto clone = torus.Clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->GetIdentifier(), "Torus");
    EXPECT_EQ(clone->GetBounds(), torus.GetBounds());

    HashQuadtree tree{};
    DummyDataStructure dummy{};
    EXPECT_TRUE(torus.CompatibleWith(tree));
    EXPECT_FALSE(torus.CompatibleWith(dummy));
}

TEST(TopologyTest, TorusPreparesWrapCells) {
    Torus torus{Rect{0, 0, 4, 4}};
    constexpr static std::array cells{Vec2{0, 0}, Vec2{3, 3}};
    HashQuadtree tree{cells};

    torus.PrepareBorderCells(tree);

    EXPECT_TRUE(tree.Get({4, 0}));
    EXPECT_TRUE(tree.Get({0, 4}));
    EXPECT_TRUE(tree.Get({4, 4}));
    EXPECT_TRUE(tree.Get({-1, -1}));

    torus.CleanupBorderCells(tree);

    EXPECT_TRUE(tree.Get({0, 0}));
    EXPECT_TRUE(tree.Get({3, 3}));
    EXPECT_FALSE(tree.Get({4, 0}));
    EXPECT_FALSE(tree.Get({0, 4}));
    EXPECT_FALSE(tree.Get({4, 4}));
    EXPECT_FALSE(tree.Get({-1, -1}));
}

TEST(TopologyTest, Log2MaxIncrementDependsOnBounds) {
    const Plane boundedPlane{Rect{0, 0, 8, 8}};
    const Plane unboundedPlane{};
    const Torus boundedTorus{Rect{0, 0, 8, 8}};

    EXPECT_EQ(boundedPlane.Log2MaxIncrement(BigInt{1}), 0);
    EXPECT_EQ(unboundedPlane.Log2MaxIncrement(BigInt{0}), -1);
    EXPECT_EQ(unboundedPlane.Log2MaxIncrement(BigInt{8}), 3);
    EXPECT_EQ(boundedTorus.Log2MaxIncrement(BigInt{16}), 0);
}

} // namespace Golde