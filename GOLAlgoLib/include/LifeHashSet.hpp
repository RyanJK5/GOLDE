#ifndef LifeHashSet_h_
#define LifeHashSet_h_

#include <ankerl/unordered_dense.h>

#include "Graphics2D.hpp"

template <>
struct std::hash<gol::Vec2> {
    size_t operator()(gol::Vec2 vec) const;
};

namespace gol {
// The primary data structure for representing the grid through the SparseLife
// algorithm. It is also the most useful way for viewing the grid when using
// editor commands, such as copy, paste, etc.
using LifeHashSet = ankerl::unordered_dense::set<Vec2>;
} // namespace gol

#endif
