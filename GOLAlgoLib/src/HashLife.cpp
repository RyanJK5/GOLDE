#include "HashLife.hpp"
#include "HashQuadtree.hpp"

namespace gol {
std::string_view HashLife::Identifier = "HashLife";

HashLife::HashLife() : LifeAlgorithm(this) {}

// Returns the exponent of the greatest power of two less than `stepSize`.
constexpr static int32_t Log2MaxAdvanceOf(const BigInt& stepSize) {
    if (stepSize.is_zero())
        return 0;

    return static_cast<int32_t>(boost::multiprecision::msb(stepSize));
}

constexpr BigInt BigPow2(int32_t exponent) { return BigOne << exponent; }

BigInt HashLife::Step(LifeDataStructure& data, const BigInt& numSteps,
                      std::stop_token stopToken) {
    auto& hashQuadtree = dynamic_cast<HashQuadtree&>(data);

    if (numSteps.is_zero()) // Hyper speed
        return BigPow2(hashQuadtree.Advance(-1, stopToken));

    BigInt generation{};
    while (generation < numSteps) {
        const auto advanceLevel = Log2MaxAdvanceOf(numSteps - generation);

        const auto gens =
            BigPow2(hashQuadtree.Advance(advanceLevel, stopToken));
        if (stopToken.stop_requested())
            return generation;
        generation += gens;
    }

    return generation;
}
} // namespace gol