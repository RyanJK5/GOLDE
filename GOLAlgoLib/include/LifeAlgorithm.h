#ifndef LifeAlgorithm_h_
#define LifeAlgorithm_h_

#include <functional>
#include <span>
#include <stop_token>
#include <variant>

#include "Graphics2D.h"
#include "HashQuadtree.h"
#include "LifeHashSet.h"

namespace gol {
// A simple hashing algorithm for running Game of Life. It is fairly efficient
// for most patterns but struggles with breeders.
LifeHashSet SparseLife(std::span<const Vec2> data, Rect bounds,
                       std::stop_token = {});

// The most efficient algorithm for Game of Life. Allows rapid single generation
// advancement and allows travel into the distant future. 
int64_t HashLife(HashQuadtree &data, int64_t numSteps,
                 std::stop_token stopToken = {});

enum class LifeAlgorithm {
    HashLife,
    SparseLife,
};
} // namespace gol

#endif