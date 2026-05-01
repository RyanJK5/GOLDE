#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "FileFormatHandler.hpp"

namespace Golde {
namespace {
struct PatternCase {
    std::string_view FileName;
    LifeHashSet InitialCells;
    LifeHashSet ExpectedCells;
    int32_t StepCount;
};

class ClassicPatternTest : public ::testing::TestWithParam<PatternCase> {};

LifeHashSet ApplyOffset(const GameGrid& grid, Vec2 offset = {}) {
    LifeHashSet result{};
    for (const auto cell : grid.SortedData()) {
        result.insert(cell + offset);
    }
    return result;
}
} // namespace

TEST_P(ClassicPatternTest, FileLoadsAndMatchesExpectedPattern) {
    const auto path = std::filesystem::path{"universes"} / GetParam().FileName;
    const auto loaded = FileEncoder::ReadRegion(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().Message;

    EXPECT_EQ(loaded->Grid.GetRuleString(), "B3/S23");
    EXPECT_EQ(ApplyOffset(loaded->Grid, loaded->Offset),
              GetParam().InitialCells);
}

TEST_P(ClassicPatternTest, GameGridEvolvesPattern) {
    const auto path = std::filesystem::path{"universes"} / GetParam().FileName;
    const auto loaded = FileEncoder::ReadRegion(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().Message;

    GameGrid grid{loaded->Grid, Size2{8, 8}};
    const auto generations = grid.Update(GetParam().StepCount);

    EXPECT_EQ(generations, GetParam().StepCount);
    EXPECT_EQ(ApplyOffset(grid, loaded->Offset), GetParam().ExpectedCells);
}

INSTANTIATE_TEST_SUITE_P(
    ConwayPatterns, ClassicPatternTest,
    ::testing::Values(
        PatternCase{
            .FileName = "block.rle",
            .InitialCells = LifeHashSet{{0, 0}, {1, 0}, {0, 1}, {1, 1}},
            .ExpectedCells = LifeHashSet{{0, 0}, {1, 0}, {0, 1}, {1, 1}},
            .StepCount = 1},
        PatternCase{.FileName = "blinker.rle",
                    .InitialCells = LifeHashSet{{1, 0}, {1, 1}, {1, 2}},
                    .ExpectedCells = LifeHashSet{{0, 1}, {1, 1}, {2, 1}},
                    .StepCount = 1},
        PatternCase{
            .FileName = "glider.rle",
            .InitialCells = LifeHashSet{{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}},
            .ExpectedCells =
                LifeHashSet{{2, 1}, {3, 2}, {1, 3}, {2, 3}, {3, 3}},
            .StepCount = 4}));

} // namespace Golde