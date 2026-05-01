#ifndef LifeHashSet_hpp_
#define LifeHashSet_hpp_

#include <ankerl/unordered_dense.h>

#include "Graphics2D.hpp"

template <>
struct std::hash<Golde::Vec2> {
    size_t operator()(Golde::Vec2 vec) const;
};

namespace Golde {
using LifeHashSet = ankerl::unordered_dense::set<Vec2>;
} // namespace Golde

#endif
