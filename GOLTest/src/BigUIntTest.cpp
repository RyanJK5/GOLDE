#include "BigUInt.hpp"
#include <gtest/gtest.h>

namespace gol {
TEST(BigUIntTest, DefaultConstructsToZero) {
    BigUInt n{};
    EXPECT_EQ(n.ToString(), "0");
}

TEST(BigUIntTest, ConstructFromSmallUInt64) {
    BigUInt n{42ULL};
    EXPECT_EQ(n.ToString(), "42");
}

TEST(BigUIntTest, ConstructFromMaxUInt32) {
    BigUInt n{0xFFFFFFFFULL};
    EXPECT_EQ(n.ToString(), "4,294,967,295");
}

TEST(BigUIntTest, ConstructFromUInt64SpanningTwoLimbs) {
    BigUInt n{0xFFFFFFFFFFFFFFFFULL};
    EXPECT_EQ(n.ToString(), "18,446,744,073,709,551,615");
}

TEST(BigUIntTest, AddZeroToZero) {
    BigUInt a, b;
    EXPECT_EQ((a + b).ToString(), "0");
}

TEST(BigUIntTest, AddSmallValues) {
    BigUInt a{1ULL}, b{2ULL};
    EXPECT_EQ((a + b).ToString(), "3");
}

TEST(BigUIntTest, AddWithCarryAcrossLimbBoundary) {
    BigUInt a{0xFFFFFFFFULL}, b{1ULL};
    EXPECT_EQ((a + b).ToString(), "4,294,967,296");
}

TEST(BigUIntTest, AddWithCarryAcrossUInt64Boundary) {
    BigUInt a{0xFFFFFFFFFFFFFFFFULL}, b{1ULL};
    EXPECT_EQ((a + b).ToString(), "18,446,744,073,709,551,616");
}

TEST(BigUIntTest, AddLargeValues) {
    BigUInt a{0xFFFFFFFFFFFFFFFFULL};
    BigUInt b{0xFFFFFFFFFFFFFFFFULL};
    EXPECT_EQ((a + b).ToString(), "36,893,488,147,419,103,230");
}

TEST(BigUIntTest, AddIsCommutative) {
    BigUInt a{123456789ULL}, b{987654321ULL};
    EXPECT_EQ((a + b).ToString(), (b + a).ToString());
}

TEST(BigUIntTest, CompoundAddAssign) {
    BigUInt a{100ULL};
    a += BigUInt{200ULL};
    EXPECT_EQ(a.ToString(), "300");
}

TEST(BigUIntTest, RepeatedIncrementCrossesLimbBoundary) {
    BigUInt a{0xFFFFFFFEULL};
    a += BigUInt{1ULL};
    a += BigUInt{1ULL};
    EXPECT_EQ(a.ToString(), "4,294,967,296");
}

TEST(BigUIntTest, ShiftZeroByAnyAmountIsZero) {
    BigUInt a;
    EXPECT_EQ((a << 64).ToString(), "0");
}

TEST(BigUIntTest, ShiftByZero) {
    BigUInt a{42ULL};
    EXPECT_EQ((a << 0).ToString(), "42");
}

TEST(BigUIntTest, ShiftOneByOne) {
    BigUInt a{1ULL};
    EXPECT_EQ((a << 1).ToString(), "2");
}

TEST(BigUIntTest, ShiftOneBy32) {
    BigUInt a{1ULL};
    EXPECT_EQ((a << 32).ToString(), "4,294,967,296");
}

TEST(BigUIntTest, ShiftOneBy64) {
    BigUInt a{1ULL};
    EXPECT_EQ((a << 64).ToString(), "18,446,744,073,709,551,616");
}

TEST(BigUIntTest, ShiftOneBy128) {
    BigUInt a{1ULL};
    EXPECT_EQ((a << 128).ToString(),
              "340,282,366,920,938,463,463,374,607,431,768,211,456");
}

TEST(BigUIntTest, ShiftAcrossLimbBoundary) {
    BigUInt a{0xFFFFFFFFULL};
    EXPECT_EQ((a << 4).ToString(), "68,719,476,720");
}

TEST(BigUIntTest, CompoundShiftAssign) {
    BigUInt a{1ULL};
    a <<= 10;
    EXPECT_EQ(a.ToString(), "1,024");
}

TEST(BigUIntTest, EqualityOfZeros) { EXPECT_EQ(BigUInt{}, BigUInt{}); }

TEST(BigUIntTest, EqualityOfSameValue) {
    EXPECT_EQ(BigUInt{42ULL}, BigUInt{42ULL});
}

TEST(BigUIntTest, InequalityOfDifferentValues) {
    EXPECT_NE(BigUInt{1ULL}, BigUInt{2ULL});
}

TEST(BigUIntTest, LessThan) { EXPECT_LT(BigUInt{1ULL}, BigUInt{2ULL}); }

TEST(BigUIntTest, GreaterThan) { EXPECT_GT(BigUInt{2ULL}, BigUInt{1ULL}); }

TEST(BigUIntTest, LessThanAcrossLimbBoundary) {
    BigUInt small{0xFFFFFFFFULL};
    BigUInt large{1ULL};
    large <<= 32;
    EXPECT_LT(small, large);
}

TEST(BigUIntTest, CompareValuesOfDifferentLimbCounts) {
    BigUInt small{1ULL};
    BigUInt large = BigUInt{1ULL} << 64;
    EXPECT_LT(small, large);
    EXPECT_GT(large, small);
}

TEST(BigUIntTest, GenerationCountDoubling) {
    BigUInt gen{1ULL};
    for (int i = 0; i < 64; ++i)
        gen <<= 1;
    EXPECT_EQ(gen.ToString(), "18,446,744,073,709,551,616");
}

TEST(BigUIntTest, GenerationCountDoublingBeyondUInt64) {
    BigUInt gen{1ULL};
    gen <<= 128;
    EXPECT_EQ(gen.ToString(),
              "340,282,366,920,938,463,463,374,607,431,768,211,456");
}

TEST(BigUIntTest, AccumulatedGenerationIncrement) {
    BigUInt total{};
    BigUInt step{1ULL};
    step <<= 64;
    for (int i = 0; i < 100; ++i)
        total += step;
    BigUInt expected{100ULL};
    expected <<= 64;
    EXPECT_EQ(total, expected);
}
} // namespace gol