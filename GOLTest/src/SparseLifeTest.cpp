#include <algorithm>
#include <gtest/gtest.h>
#include <ranges>
#include <vector>

#include "LifeAlgorithm.hpp"
#include "RLEEncoder.hpp"

namespace gol {
TEST(SparseLifeTest, SquigglesTest) {
    const auto directory = std::filesystem::path{"universes"};
    EXPECT_TRUE(std::filesystem::exists(directory));

    auto data1 =
        RLEEncoder::ReadRegion(directory / "squiggles1.rle")->Grid.Data() |
        std::ranges::to<std::vector<Vec2>>();
    auto data2 =
        RLEEncoder::ReadRegion(directory / "squiggles2.rle")->Grid.Data() |
        std::ranges::to<LifeHashSet>();
    auto data3 =
        RLEEncoder::ReadRegion(directory / "squiggles3.rle")->Grid.Data() |
        std::ranges::to<LifeHashSet>();
    auto data4 =
        RLEEncoder::ReadRegion(directory / "squiggles4.rle")->Grid.Data() |
        std::ranges::to<LifeHashSet>();
    const Rect bounds{};

    constexpr static auto DataEqual = [](const LifeHashSet& a,
                                         const LifeHashSet& b) {
        auto offset = std::ranges::min(b) - std::ranges::min(a);
        if (a.size() != b.size())
            return false;
        for (const auto cell : a)
            if (!b.contains(cell + offset))
                return false;
        return true;
    };

    auto sparse1 = SparseLife(data1, bounds);
    EXPECT_TRUE(DataEqual(sparse1, data2));

    sparse1 =
        SparseLife(std::vector<Vec2>{sparse1.begin(), sparse1.end()}, bounds);
    data2 = SparseLife(std::vector<Vec2>{data2.begin(), data2.end()}, bounds);
    EXPECT_TRUE(DataEqual(sparse1, data3));
    EXPECT_TRUE(DataEqual(data2, data3));

    sparse1 =
        SparseLife(std::vector<Vec2>{sparse1.begin(), sparse1.end()}, bounds);
    data2 = SparseLife(data2, bounds);
    data3 = SparseLife(data3, bounds);
    EXPECT_TRUE(DataEqual(sparse1, data4));
    EXPECT_TRUE(DataEqual(data2, data4));
    EXPECT_TRUE(DataEqual(data3, data4));
}

TEST(SparseLifeTest, EmptyGrid) {
    std::vector<Vec2> cells{};
    Rect bounds(0, 0, 10, 10);
    auto result = SparseLife(cells, bounds);

    EXPECT_TRUE(result.empty());
}

TEST(SparseLifeTest, SingleCellDies) {
    std::vector<Vec2> cells{{5, 5}};
    Rect bounds(0, 0, 10, 10);
    auto result = SparseLife(cells, bounds);

    EXPECT_TRUE(result.empty());
}

TEST(SparseLifeTest, BlockPattern) {
    // Standard 2x2 block (static)
    std::vector<Vec2> cells = {{1, 1}, {2, 1}, {1, 2}, {2, 2}};
    Rect bounds{0, 0, 10, 10};
    auto result = SparseLife(cells, bounds);

    ASSERT_EQ(result.size(), 4);
    EXPECT_TRUE(result.contains({1, 1}));
    EXPECT_TRUE(result.contains({2, 1}));
    EXPECT_TRUE(result.contains({1, 2}));
    EXPECT_TRUE(result.contains({2, 2}));
}

TEST(SparseLifeTest, BlinkerOscillation) {
    // Horizontal blinker
    std::vector<Vec2> start = {{1, 2}, {2, 2}, {3, 2}};
    Rect bounds(0, 0, 5, 5);

    // Gen 1: Vertical
    auto gen1 = SparseLife(start, bounds);
    ASSERT_EQ(gen1.size(), 3);
    EXPECT_TRUE(gen1.contains({2, 1}));
    EXPECT_TRUE(gen1.contains({2, 2}));
    EXPECT_TRUE(gen1.contains({2, 3}));

    // Gen 2: Horizontal again
    std::vector<Vec2> gen1_vec(gen1.begin(), gen1.end());
    auto gen2 = SparseLife(gen1_vec, bounds);
    ASSERT_EQ(gen2.size(), 3);
    EXPECT_TRUE(gen2.contains({1, 2}));
    EXPECT_TRUE(gen2.contains({2, 2}));
    EXPECT_TRUE(gen2.contains({3, 2}));
}

TEST(SparseLifeTest, RespectsBounds) {
    // Blinker near the edge
    std::vector<Vec2> cells = {{0, 1}, {1, 1}, {2, 1}};
    // Bounds only includes {1,1} and {2,1}
    Rect bounds(1, 0, 10, 10);

    auto result = SparseLife(cells, bounds);

    EXPECT_TRUE(result.contains({1, 0}));
    EXPECT_TRUE(result.contains({1, 1}));
    EXPECT_TRUE(result.contains({1, 2}));
    EXPECT_FALSE(result.contains({0, 1})); // Outside bounds
}

TEST(SparseLifeTest, InfiniteBounds) {
    // When bounds width/height are 0 or less, it should be infinite.
    std::vector<Vec2> cells = {{1000, 1000}, {1001, 1000}, {1002, 1000}};
    Rect bounds(0, 0, 0, 0);

    auto result = SparseLife(cells, bounds);

    ASSERT_EQ(result.size(), 3);
    EXPECT_TRUE(result.contains({1001, 999}));
    EXPECT_TRUE(result.contains({1001, 1000}));
    EXPECT_TRUE(result.contains({1001, 1001}));
}

TEST(SparseLifeTest, Loneliness) {
    // Two cells too far apart
    std::vector<Vec2> cells = {{1, 1}, {1, 3}};
    Rect bounds(0, 0, 10, 10);
    auto result = SparseLife(cells, bounds);
    EXPECT_TRUE(result.empty());
}

TEST(SparseLifeTest, Overpopulation) {
    // 3x3 block of cells
    std::vector<Vec2> cells;
    for (int x = 1; x <= 3; ++x)
        for (int y = 1; y <= 3; ++y)
            cells.push_back({x, y});

    Rect bounds(0, 0, 10, 10);
    auto result = SparseLife(cells, bounds);

    // Center cell (2,2) has 8 neighbors, should die.
    EXPECT_FALSE(result.contains({2, 2}));

    // Corners (1,1), (3,1), (1,3), (3,3) have 3 neighbors, should survive.
    EXPECT_TRUE(result.contains({1, 1}));
    EXPECT_TRUE(result.contains({3, 1}));
    EXPECT_TRUE(result.contains({1, 3}));
    EXPECT_TRUE(result.contains({3, 3}));

    // Middle edges (2,1), (1,2), (3,2), (2,3) have 5 neighbors, should die.
    EXPECT_FALSE(result.contains({2, 1}));
    EXPECT_FALSE(result.contains({1, 2}));
}
} // namespace gol
