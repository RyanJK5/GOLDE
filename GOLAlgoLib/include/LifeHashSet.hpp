#ifndef LifeHashSet_hpp_
#define LifeHashSet_hpp_

#include <ankerl/unordered_dense.h>

#include "Graphics2D.hpp"

template <>
struct std::hash<gol::Vec2> {
    size_t operator()(gol::Vec2 vec) const;
};

namespace gol {
using LifeHashSet = ankerl::unordered_dense::set<Vec2>;
} // namespace gol

#endif
