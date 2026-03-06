#include <cstdint>
#include <unordered_dense.h>

#include "Graphics2D.hpp"
#include "LifeHashSet.hpp"

// Arbitrary hash function
size_t std::hash<gol::Vec2>::operator()(gol::Vec2 vec) const {
    using ankerl::unordered_dense::hash;
    return hash<uint64_t>{}(static_cast<uint64_t>(vec.X) << 32 |
                            static_cast<uint32_t>(vec.Y));
}