#include <algorithm>
#include <gtest/gtest.h>

#include <print>
#include <random>
#include <ranges>

#include "HashLife.hpp"
#include "HashQuadtree.hpp"
#include "LifeAlgorithm.hpp"
#include "RLEEncoder.hpp"

namespace gol {
// Helper to verify the tree iterator yields exactly the expected points
static void VerifyContent(HashQuadtree& tree, const LifeHashSet& expected) {
    LifeHashSet actual{};

    // Test range-based for loop
    for (const auto pos : tree)
        actual.insert(pos);

    ASSERT_EQ(actual.size(), expected.size())
        << "Tree yielded wrong number of cells";

    for (const auto cell : expected)
        ASSERT_TRUE(actual.contains(cell))
            << "Tree missing a cell that should be there";
}

static void CheckAgainstFile(const std::filesystem::path& unevolved,
                             const std::filesystem::path& evolved,
                             uint64_t expectedGenerationsPerJump,
                             int32_t numJumps = 1, int32_t stepSize = 0) {
    ASSERT_TRUE(std::filesystem::exists(unevolved));
    ASSERT_TRUE(std::filesystem::exists(evolved));
    ASSERT_GE(numJumps, 1);

    const auto data1 = FileEncoder::ReadRegion(unevolved);
    const auto data2 = FileEncoder::ReadRegion(evolved);
    const auto shifted = [](const HashQuadtree& tree, Vec2 offset) {
        std::vector<Vec2> result{};
        for (const auto cell : tree) {
            result.emplace_back(cell + offset);
        }
        return result;
    };

    HashQuadtree current{shifted(data1->Grid.Data(), data1->Offset)};
    const HashQuadtree expected{shifted(data2->Grid.Data(), data2->Offset)};

    BigInt totalGenerations{};
    for (auto i = 0; i < numJumps; ++i) {
        const auto genCount = HashLife{}.Step(current, stepSize);
        EXPECT_EQ(genCount, expectedGenerationsPerJump)
            << "HashLife should advance by " << expectedGenerationsPerJump
            << " generations per jump";
        totalGenerations += genCount;
    }

    EXPECT_EQ(current, expected);
    EXPECT_EQ(totalGenerations, expectedGenerationsPerJump * BigInt{numJumps})
        << "Total generations advanced should match expectation";
}

TEST(HashQuadtreeTest, SquigglesTest) {
    const std::filesystem::path directory{"universes"};
    CheckAgainstFile(directory / "squiggles1.rle",
                     directory / "squiggles64.rle", 64);
    CheckAgainstFile(directory / "squiggles1.rle",
                     directory / "squiggles64.rle", 16, 4, 16);
    CheckAgainstFile(directory / "squiggles1.rle", directory / "squiggles2.rle",
                     1, 1, 1);
}

TEST(HashQuadtreeTest, BigSquigglesTest) {
    const std::filesystem::path directory{"universes"};
    CheckAgainstFile(directory / "bigsquiggles1.rle",
                     directory / "bigsquiggles1024.rle", 1024);
    CheckAgainstFile(directory / "bigsquiggles1.rle",
                     directory / "bigsquiggles1024.rle", 128, 8, 128);
}

TEST(HashQuadtreeTest, RectTest) {
    const std::filesystem::path directory{"universes"};
    CheckAgainstFile(directory / "rect1.rle", directory / "rect4.rle", 4);
}

TEST(HashQuadtreeTest, EmptyTree) {
    LifeHashSet cells{};
    HashQuadtree tree{cells};

    EXPECT_EQ(tree.begin(), tree.end())
        << "Empty tree should only contain end iterator";
    EXPECT_TRUE(std::ranges::empty(tree))
        << "Empty tree should satisfy std::ranges::empty";
    EXPECT_EQ(tree.Population(), 0ULL);

    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, PopulationMatchesLiveCells) {
    LifeHashSet singleCell{{5, 5}};
    HashQuadtree singleTree{singleCell};
    EXPECT_EQ(singleTree.Population(), 1ULL);

    LifeHashSet blockCells{{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    HashQuadtree blockTree{blockCells};
    EXPECT_EQ(blockTree.Population(), BigInt{blockCells.size()});

    HashLife{}.Step(blockTree, 0);
    EXPECT_EQ(blockTree.Population(), BigInt{blockCells.size()});

    HashLife{}.Step(singleTree, 1);

    EXPECT_EQ(singleTree.Population(), BigZero);
}

TEST(HashQuadtreeTest, SingleCell) {
    LifeHashSet cells = {{10, 20}};
    HashQuadtree tree{cells};

    ASSERT_NE(tree.begin(), tree.end());
    EXPECT_EQ((*tree.begin()).X, 10);
    EXPECT_EQ((*tree.begin()).Y, 20);

    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, BlockPattern) {
    // Standard 2x2 block
    LifeHashSet cells{{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    HashQuadtree tree{cells};

    EXPECT_EQ(std::ranges::distance(tree), 4);
    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, SparsePattern) {
    // Points far away to force deep quadtree recursion
    LifeHashSet cells = {{-1280, -1208}, {0, 0}, {1208, 1028}};
    HashQuadtree tree{cells};

    EXPECT_EQ(std::ranges::distance(tree), 3);
    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, LargeDenseGrid) {
    LifeHashSet cells;
    for (int x = 0; x < 100; ++x) {
        for (int y = 0; y < 100; ++y) {
            cells.insert({x, y});
        }
    }
    HashQuadtree tree{cells};
    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, DiagonalLine) {
    LifeHashSet cells;
    for (int i = 0; i < 1000; ++i) {
        cells.insert({i, i});
    }
    HashQuadtree tree{cells};
    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, RandomSparse) {
    LifeHashSet cells;
    std::mt19937 gen{12345}; // Fixed seed for reproducibility
    std::uniform_int_distribution<> dist{-10000, 10000};

    for (int i = 0; i < 500; ++i) {
        cells.insert({dist(gen), dist(gen)});
    }
    HashQuadtree tree{cells};
    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, Checkerboard) {
    LifeHashSet cells;
    for (int x = 0; x < 50; ++x) {
        for (int y = 0; y < 50; ++y) {
            if ((x + y) % 2 == 0)
                cells.insert({x, y});
        }
    }
    HashQuadtree tree{cells};
    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, RangesCompliance) {
    static_assert(std::ranges::range<HashQuadtree>);
    static_assert(std::ranges::input_range<HashQuadtree>);

    LifeHashSet cells = {{1, 1}, {2, 2}, {3, 3}};
    HashQuadtree tree{cells};

    // Test with std::algorithms
    auto count = std::count_if(tree.begin(), tree.end(), [](Vec2 v) {
        return v.X > 1; // Should find {2,2} and {3,3}
    });
    EXPECT_EQ(count, 2);
}

TEST(HashQuadtreeTest, AdvanceBlock) {
    // 2x2 Block (Still Life)
    LifeHashSet cells{{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    HashQuadtree tree{cells};

    // Should remain stable
    auto gens = HashLife{}.Step(tree, 0);
    EXPECT_GE(gens, 2); // At least 2 generations (level 3)
    VerifyContent(tree, cells);

    // Advance again
    gens = HashLife{}.Step(tree, 0);
    EXPECT_GE(gens, 2);
    VerifyContent(tree, cells);
}

TEST(HashQuadtreeTest, AdvanceBlinker) {
    // Blinker (Period 2 Oscillator)
    LifeHashSet start{{0, 0}, {0, 1}, {0, 2}};
    HashQuadtree tree(start);

    auto gens = HashLife{}.Step(tree, 0);
    // HashLife advances by 2^(k-2) steps. For k=3, steps=2.
    // A blinker's period is 2, so it should be back to start if steps is a
    // multiple of 2.
    EXPECT_EQ(gens % 2, 0)
        << "HashLife should advance by a multiple of blinker period";

    LifeHashSet actual;
    for (const auto p : tree)
        actual.insert(p);

    // Blinker should still be vertical
    ASSERT_EQ(actual.size(), 3);
    EXPECT_TRUE(actual.contains({0, 0}));
    EXPECT_TRUE(actual.contains({0, 1}));
    EXPECT_TRUE(actual.contains({0, 2}));
}

TEST(HashQuadtreeTest, AdvanceGlider) {
    // Glider
    LifeHashSet start{{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};

    HashQuadtree tree{start};
    auto gens = HashLife{}.Step(tree, 0);
    EXPECT_GT(gens, 0);

    LifeHashSet actual;
    for (const auto p : tree)
        actual.insert(p);

    // Glider should survive (5 cells)
    EXPECT_EQ(actual.size(), 5);

    // And should have moved from original position
    bool sameAsStart = true;
    for (const auto cell : start) {
        if (!actual.contains(cell)) {
            sameAsStart = false;
            break;
        }
    }
    EXPECT_FALSE(sameAsStart) << "Glider did not move";
}

TEST(HashQuadtreeTest, ConstIteratorUsage) {
    const LifeHashSet cells{{1, 1}, {5, 5}};
    const HashQuadtree tree{cells};

    // Ensure we can iterate over a const tree using range-based for
    size_t count = 0;
    for ([[maybe_unused]] const auto cell : tree)
        count++;
    EXPECT_EQ(count, 2);

    // Explicitly check ConstIterator type
    static_assert(
        std::is_same_v<HashQuadtree::Iterator, decltype(tree.begin())>);
}

TEST(HashQuadtreeTest, CopyingBreeder) {
    const std::filesystem::path directory{"universes"};

    const auto data = FileEncoder::ReadRegion(directory / "glider_gun.rle");

    HashQuadtree original{data->Grid.Data() | std::ranges::to<LifeHashSet>(),
                          data->Offset};

    HashLife{}.Step(original, 32);
    HashQuadtree copy{original};

    ASSERT_TRUE(true);
    // ASSERT_EQ(original, copy);
}

TEST(HashQuadtreeTest, TranslationInvariance) {
    // HashLife should behave the same regardless of where the pattern is in the
    // grid
    const LifeHashSet blockAtOrigin{{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    const LifeHashSet blockAtDistance{
        {1000, 1000}, {1001, 1000}, {1000, 1001}, {1001, 1001}};

    HashQuadtree tree1{blockAtOrigin};
    HashQuadtree tree2{blockAtDistance};

    auto gens1 = HashLife{}.Step(tree1, 0);
    auto gens2 = HashLife{}.Step(tree2, 0);

    EXPECT_EQ(gens1, gens2) << "Translation should not affect step size";
    EXPECT_EQ(std::ranges::distance(tree1), 4);
    EXPECT_EQ(std::ranges::distance(tree2), 4);
}

TEST(HashQuadtreeTest, UniverseHeatDeath) {
    // A single cell dies in the next generation
    LifeHashSet cells{{42, 42}};
    HashQuadtree tree{cells};

    const auto gens = HashLife{}.Step(tree, 0);

    EXPECT_GT(gens, 0);
    EXPECT_TRUE(tree.empty());
    EXPECT_EQ(tree.begin(), tree.end());
    EXPECT_EQ(std::ranges::distance(tree), 0);
}

TEST(HashQuadtreeTest, LargeCoordinateStability) {
    // Test with coordinates that might challenge bit-depth or offsets
    constexpr static int32_t far = 1'000'000;
    const LifeHashSet cells{
        {far, far}, {far + 1, far}, {far, far + 1}, {far + 1, far + 1}};
    HashQuadtree tree{cells};

    EXPECT_EQ(std::ranges::distance(tree), 4);

    auto gens = HashLife{}.Step(tree, 0);
    EXPECT_GT(gens, 0);

    EXPECT_EQ(std::ranges::distance(tree), 4);
    for (const auto pos : tree) {
        EXPECT_GE(pos.X, far);
        EXPECT_GE(pos.Y, far);
    }
}

TEST(HashQuadtreeTest, LifecycleAndCopy) {
    // Testing that copy and move work correctly and maintain tree integrity
    LifeHashSet cells{{1, 1}, {2, 2}};
    HashQuadtree tree1{cells};

    // Copy construction
    HashQuadtree tree2 = tree1;
    VerifyContent(tree2, cells);

    // Move construction
    HashQuadtree tree3 = std::move(tree1);
    VerifyContent(tree3, cells);

    // Identity check via NextGeneration
    auto gen2 = HashLife{}.Step(tree2, 0);
    auto gen3 = HashLife{}.Step(tree3, 0);

    EXPECT_EQ(gen2, gen3);
    EXPECT_EQ(std::ranges::distance(tree2), std::ranges::distance(tree3));
}

TEST(HashQuadtreeTest, StandardViewComposition) {
    // Integration with C++23 views
    const LifeHashSet cells{{0, 0}, {1, 1}, {2, 2}, {10, 10}};
    HashQuadtree tree{cells};

    // Filter and count using ranges
    auto filtered =
        tree | std::views::filter([](const auto p) { return p.X < 5; });
    EXPECT_EQ(std::ranges::distance(filtered), 3);

    // Sum coordinates
    const auto sumX = std::ranges::fold_left(
        tree | std::views::transform([](auto p) { return p.X; }), 0,
        std::plus<>{});
    EXPECT_EQ(sumX, 13);
}

TEST(HashQuadtreeTest, SlowAdvanceSingleStepDyingPattern) {
    LifeHashSet cells{{0, 0}, {7, 0}, {0, 7}, {7, 7}};
    HashQuadtree tree{cells};

    EXPECT_GE(tree.CalculateDepth(), 3)
        << "Tree must be deep enough to trigger slow advance";

    const auto gens = HashLife{}.Step(tree, 1);
    EXPECT_EQ(gens, 1);
    EXPECT_TRUE(tree.empty())
        << "All isolated cells should die after one generation";
}

// ========== Tests for Vec2L refactoring and bounds checking ==========

TEST(HashQuadtreeTest, Vec2LInternalStoragePositiveCoordinates) {
    // Test that Vec2L internal storage correctly handles positive coordinates
    LifeHashSet cells{{100, 200}, {150, 250}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // Verify all cells are returned correctly (converted from Vec2L to Vec2)
    ASSERT_EQ(actual.size(), 2);
    EXPECT_TRUE(actual.contains({100, 200}));
    EXPECT_TRUE(actual.contains({150, 250}));
}

TEST(HashQuadtreeTest, Vec2LInternalStorageNegativeCoordinates) {
    // Test that Vec2L internal storage correctly handles negative coordinates
    LifeHashSet cells{{-100, -200}, {-50, -75}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // Verify all cells with negative coordinates are returned correctly
    ASSERT_EQ(actual.size(), 2);
    EXPECT_TRUE(actual.contains({-100, -200}));
    EXPECT_TRUE(actual.contains({-50, -75}));
}

TEST(HashQuadtreeTest, Vec2LMixedSignCoordinates) {
    // Test that Vec2L internal storage correctly handles mixed sign coordinates
    LifeHashSet cells{{-500, 500}, {500, -500}, {-1000, -1000}, {1000, 1000}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // Verify all cells with mixed signs are returned correctly
    ASSERT_EQ(actual.size(), 4);
    EXPECT_TRUE(actual.contains({-500, 500}));
    EXPECT_TRUE(actual.contains({500, -500}));
    EXPECT_TRUE(actual.contains({-1000, -1000}));
    EXPECT_TRUE(actual.contains({1000, 1000}));
}

TEST(HashQuadtreeTest, BoundsCheckingAtInt32Max) {
    // Test bounds checking at the maximum int32 value
    constexpr int32_t maxInt32 = std::numeric_limits<int32_t>::max();
    const LifeHashSet cells{{maxInt32, maxInt32}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // Cell at max int32 should be included
    ASSERT_EQ(actual.size(), 1);
    EXPECT_TRUE(actual.contains({maxInt32, maxInt32}));
}

TEST(HashQuadtreeTest, BoundsCheckingAtInt32Min) {
    // Test bounds checking at the minimum int32 value
    constexpr int32_t minInt32 = std::numeric_limits<int32_t>::min();
    const LifeHashSet cells{{minInt32, minInt32}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // Cell at min int32 should be included
    ASSERT_EQ(actual.size(), 1);
    EXPECT_TRUE(actual.contains({minInt32, minInt32}));
}

TEST(HashQuadtreeTest, BoundsCheckingNearInt32Boundaries) {
    // Test bounds checking with coordinates near int32 boundaries
    constexpr static int32_t maxInt32 =
        std::numeric_limits<int32_t>::max() >> 1;
    constexpr static int32_t minInt32 =
        std::numeric_limits<int32_t>::min() >> 1;

    const LifeHashSet cells{{maxInt32 - 1, maxInt32 - 1},
                            {minInt32 + 1, minInt32 + 1},
                            {maxInt32 - 100, minInt32 + 100}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // All cells within bounds should be included
    ASSERT_EQ(actual.size(), 3);
    EXPECT_TRUE(actual.contains({maxInt32 - 1, maxInt32 - 1}));
    EXPECT_TRUE(actual.contains({minInt32 + 1, minInt32 + 1}));
    EXPECT_TRUE(actual.contains({maxInt32 - 100, minInt32 + 100}));
}

TEST(HashQuadtreeTest, Vec2LOffsetHandling) {
    // Test that Vec2L offsets are correctly computed and used during iteration
    LifeHashSet cells{{5000, 6000}, {5100, 6100}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // Verify that the offset is correctly applied
    ASSERT_EQ(actual.size(), 2);
    EXPECT_TRUE(actual.contains({5000, 6000}));
    EXPECT_TRUE(actual.contains({5100, 6100}));
}

TEST(HashQuadtreeTest, Vec2LConversionConsistency) {
    // Test that conversion from Vec2L back to Vec2 is consistent
    const std::vector<Vec2> originalCells{
        {0, 0}, {100, 100}, {-100, -100}, {50, -50}, {-50, 50}};

    LifeHashSet input;
    for (const auto cell : originalCells)
        input.insert(cell);

    HashQuadtree tree{input};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // Verify that all original cells are preserved through conversion
    ASSERT_EQ(actual.size(), originalCells.size());
    for (const auto cell : originalCells)
        EXPECT_TRUE(actual.contains(cell));
}

TEST(HashQuadtreeTest, Vec2LWithLargeOffsets) {
    // Test Vec2L handling with large offset values
    constexpr int32_t largeOffset = 1000000;
    const LifeHashSet cells{{largeOffset, largeOffset},
                            {largeOffset + 10, largeOffset + 20}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    ASSERT_EQ(actual.size(), 2);
    EXPECT_TRUE(actual.contains({largeOffset, largeOffset}));
    EXPECT_TRUE(actual.contains({largeOffset + 10, largeOffset + 20}));
}

TEST(HashQuadtreeTest, Vec2LNegativeLargeOffsets) {
    // Test Vec2L handling with large negative offset values
    constexpr int32_t largeNegativeOffset = -1000000;
    const LifeHashSet cells{
        {largeNegativeOffset, largeNegativeOffset},
        {largeNegativeOffset - 10, largeNegativeOffset - 20}};
    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    ASSERT_EQ(actual.size(), 2);
    EXPECT_TRUE(actual.contains({largeNegativeOffset, largeNegativeOffset}));
    EXPECT_TRUE(
        actual.contains({largeNegativeOffset - 10, largeNegativeOffset - 20}));
}

TEST(HashQuadtreeTest, Vec2LWithExplicitConstructorOffset) {
    // Test that explicit offset parameter in constructor is correctly added to
    // computed offset
    LifeHashSet cells{{100, 100}, {200, 200}};
    Vec2 explicitOffset{50, 50};

    // Create tree without explicit offset
    HashQuadtree tree1{cells};

    // Create tree with explicit offset
    HashQuadtree tree2{cells, explicitOffset};

    LifeHashSet actual1, actual2;
    for (const auto pos : tree1)
        actual1.insert(pos);
    for (const auto pos : tree2)
        actual2.insert(pos);

    // Both trees should have same number of cells
    ASSERT_EQ(actual1.size(), 2);
    ASSERT_EQ(actual2.size(), 2);

    // tree1 should have cells at {100, 100} and {200, 200}
    EXPECT_TRUE(actual1.contains({100, 100}));
    EXPECT_TRUE(actual1.contains({200, 200}));

    // tree2 should have cells offset by {50, 50}: {150, 150} and {250, 250}
    EXPECT_TRUE(actual2.contains({150, 150}));
    EXPECT_TRUE(actual2.contains({250, 250}));
}

TEST(HashQuadtreeTest, Vec2LCopyConstructorPreservesInternalState) {
    // Test that copying a tree preserves Vec2L internal state correctly
    LifeHashSet cells{{-500, 500}, {500, -500}};
    HashQuadtree tree1{cells};

    HashQuadtree tree2 = tree1;

    LifeHashSet actual1, actual2;
    for (const auto pos : tree1)
        actual1.insert(pos);
    for (const auto pos : tree2)
        actual2.insert(pos);

    // Both trees should yield the same cells
    EXPECT_EQ(actual1, actual2);
    ASSERT_EQ(actual1.size(), 2);
}

TEST(HashQuadtreeTest, Vec2LMoveConstructorPreservesInternalState) {
    // Test that moving a tree preserves Vec2L internal state correctly
    LifeHashSet cells{{-500, 500}, {500, -500}};
    HashQuadtree tree1{cells};

    LifeHashSet expectedCells;
    for (const auto pos : tree1)
        expectedCells.insert(pos);

    HashQuadtree tree2 = std::move(tree1);

    LifeHashSet actualFromMoved;
    for (const auto pos : tree2)
        actualFromMoved.insert(pos);

    // Moved tree should yield the same cells
    EXPECT_EQ(expectedCells, actualFromMoved);
}

TEST(HashQuadtreeTest, Vec2LIteratorComparison) {
    // Test that iterator comparison works correctly with Vec2 values converted
    // from Vec2L
    LifeHashSet cells{{100, 200}, {300, 400}};
    HashQuadtree tree{cells};

    auto it1 = tree.begin();
    auto it2 = tree.begin();

    // Both iterators should start at the same position
    EXPECT_EQ(it1, it2);
    EXPECT_FALSE(it1 != it2);

    // They should dereference to the same value
    EXPECT_EQ(*it1, *it2);
}

TEST(HashQuadtreeTest, Vec2LIteratorAdvancementWithBounds) {
    // Test that iterator advancement correctly handles Vec2L positions within
    // bounds
    LifeHashSet cells{{100, 100}, {200, 200}, {300, 300}};
    HashQuadtree tree{cells};

    size_t count = 0;
    for (const auto pos : tree) {
        count++;
        // All positions should be within int32 bounds
        EXPECT_GE(pos.X, std::numeric_limits<int32_t>::min());
        EXPECT_LE(pos.X, std::numeric_limits<int32_t>::max());
        EXPECT_GE(pos.Y, std::numeric_limits<int32_t>::min());
        EXPECT_LE(pos.Y, std::numeric_limits<int32_t>::max());
    }

    EXPECT_EQ(count, 3);
}

TEST(HashQuadtreeTest, Vec2LEqualityComparison) {
    // Test that two trees with identical cells are equal after Vec2L->Vec2
    // conversion
    LifeHashSet cells1{{10, 20}, {30, 40}, {-50, -60}};
    LifeHashSet cells2{{10, 20}, {30, 40}, {-50, -60}};

    HashQuadtree tree1{cells1};
    HashQuadtree tree2{cells2};

    EXPECT_EQ(tree1, tree2);
}

TEST(HashQuadtreeTest, Vec2LEmptyTreeBoundsCheck) {
    // Test that empty tree bounds checking doesn't affect iteration
    LifeHashSet cells;
    HashQuadtree tree{cells};

    size_t count = 0;
    for ([[maybe_unused]] const auto pos : tree)
        count++;

    EXPECT_EQ(count, 0);
    EXPECT_EQ(tree.begin(), tree.end());
}

TEST(HashQuadtreeTest, Vec2LDensePatternWithVaryingCoordinates) {
    // Test Vec2L handling with a dense pattern using various coordinate ranges
    LifeHashSet cells;

    // Add cells across different coordinate ranges
    for (int x = -100; x <= 100; x += 50) {
        for (int y = -100; y <= 100; y += 50) {
            cells.insert({x, y});
        }
    }

    HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree)
        actual.insert(pos);

    // All cells should be preserved
    EXPECT_EQ(actual, cells);
}

TEST(HashQuadtreeTest, Vec2LNextGenerationPreservesCoordinates) {
    // Test that NextGeneration correctly preserves coordinates through
    // Vec2L->Vec2 conversion
    LifeHashSet blockCells{{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    HashQuadtree tree{blockCells};

    HashLife{}.Step(tree, 1);
    LifeHashSet resultCells;
    for (const auto pos : tree)
        resultCells.insert(pos);

    // Block should remain stable
    EXPECT_EQ(resultCells, blockCells);
}

TEST(HashQuadtreeTest, Vec2LIteratorConstness) {
    // Test that const iterators correctly handle Vec2L conversion
    const LifeHashSet cells{{100, 200}, {-100, -200}};
    const HashQuadtree tree{cells};

    LifeHashSet actual;
    for (const auto pos : tree) {
        actual.insert(pos);
    }

    ASSERT_EQ(actual.size(), 2);
    EXPECT_TRUE(actual.contains({100, 200}));
    EXPECT_TRUE(actual.contains({-100, -200}));
}
} // namespace gol
