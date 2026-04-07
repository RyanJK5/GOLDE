#ifndef LifeDataStructure_hpp_
#define LifeDataStructure_hpp_

#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <variant>

#include "Graphics2D.hpp"

namespace gol {

class LifeDataStructure {
  public:
    virtual void Set(Vec2 pos, bool alive) = 0;

    virtual void Clear(Rect region) = 0;

    virtual bool Get(Vec2 pos) const = 0;

    virtual Rect FindBoundingBox() const = 0;

    virtual ~LifeDataStructure() = default;
};

} // namespace gol

#endif
