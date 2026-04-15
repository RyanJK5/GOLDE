#include <fstream>
#include <gtest/gtest.h>
#include <print>

#include "Graphics2D.hpp"
#include "RLEEncoder.hpp"

namespace gol {

static std::expected<FileEncoder::DecodeResult, std::string>
EncodeDecodeRegionTest(const GameGrid& grid, Rect region, Vec2 offset) {
    const auto encoded = FileEncoder::EncodeRegion(grid, region, offset);
    const auto decodeResult = FileEncoder::DecodeRegion(encoded, 1000000);

    if (!decodeResult.has_value()) {
        const auto str = std::format("Decode failed with error: {}",
                                     decodeResult.error().Message);
        return std::unexpected{str};
    }

    return *decodeResult;
}

TEST(EncodeTest, DecodeEncodeTest) {
    const auto filePath =
        std::filesystem::path{"universes"} / "bigsquiggles1.rle";
    const auto result = FileEncoder::ReadRegion(filePath);
    ASSERT_TRUE(result.has_value()) << result.error().Message;

    GameGrid finalGrid{};
    for (const auto pos : result->Grid.Data())
        finalGrid.Set(pos.X + result->Offset.X, pos.Y + result->Offset.Y, true);

    const auto boundingBox = finalGrid.BoundingBox();
    const auto encoded =
        FileEncoder::EncodeRegion(finalGrid, boundingBox, boundingBox.Pos());

    const auto fileStr = [&] {
        auto in = std::ifstream{filePath};
        return std::string{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};
    }();

    ASSERT_EQ(fileStr, encoded);
}

TEST(EncodeTest, SquareTest) {
    const LifeHashSet data{{0, 0}, {1, 1}, {0, 1}, {1, 0}};
    GameGrid grid{};
    for (const auto pos : data)
        grid.Set(pos.X, pos.Y, true);

    constexpr static Vec2 offset{2, 2};
    const auto result = EncodeDecodeRegionTest(grid, Rect{0, 0, 4, 4}, offset);

    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_EQ(result->Grid.Data(), grid.Data());
    EXPECT_EQ(result->Offset, offset);
}

TEST(EncodeTest, SingleCellTest) {
    GameGrid grid{};
    grid.Set(3, 4, true);

    constexpr static Vec2 offset{5, -3};
    const auto result = EncodeDecodeRegionTest(grid, Rect{0, 0, 8, 8}, offset);

    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_EQ(result->Grid.Data(), grid.Data());
    EXPECT_EQ(result->Offset, offset);
}

TEST(EncodeTest, HorizontalLineTest) {
    const LifeHashSet data{{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}};
    GameGrid grid{};
    for (const auto pos : data)
        grid.Set(pos.X, pos.Y, true);

    constexpr static Vec2 offset{-4, 7};
    const auto result = EncodeDecodeRegionTest(grid, Rect{0, 0, 6, 6}, offset);

    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_EQ(result->Grid.Data(), grid.Data());
    EXPECT_EQ(result->Offset, offset);
}

TEST(EncodeTest, SparsePatternTest) {
    const LifeHashSet data{{0, 0}, {2, 5}, {3, 1}, {6, 6}, {8, 2}, {9, 9}};

    GameGrid grid{};
    for (const auto pos : data)
        grid.Set(pos.X, pos.Y, true);

    constexpr static Vec2 offset{-10, -10};
    const auto result =
        EncodeDecodeRegionTest(grid, Rect{0, 0, 10, 10}, offset);

    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_EQ(result->Grid.Data(), grid.Data());
    EXPECT_EQ(result->Offset, offset);
}

TEST(EncodeTest, EmptyRegionTest) {
    GameGrid grid{};

    constexpr static Vec2 offset{2, 2};
    const auto result = EncodeDecodeRegionTest(grid, Rect{0, 0, 5, 5}, offset);

    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_TRUE(result->Grid.Data().empty());
    EXPECT_EQ(result->Offset, offset);
}

TEST(EncodeTest, IgnoresCellsOutsideRegionTest) {
    const LifeHashSet data{{-1, 0}, {0, 0}, {3, 3}, {4, 0}, {0, 4}, {8, 8}};

    GameGrid grid{};
    for (const auto pos : data)
        grid.Set(pos.X, pos.Y, true);

    GameGrid expected{};
    expected.Set(0, 0, true);
    expected.Set(3, 3, true);

    constexpr static Vec2 offset{1, 1};
    const auto result = EncodeDecodeRegionTest(grid, Rect{0, 0, 4, 4}, offset);

    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_EQ(result->Grid.Data(), expected.Data());
    EXPECT_EQ(result->Offset, offset);
}
} // namespace gol
