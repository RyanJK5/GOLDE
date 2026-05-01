#ifndef Camera_hpp_
#define Camera_hpp_

#include <glm/glm.hpp>
#include <limits>

#include "Graphics2D.hpp"

namespace Golde {
class GraphicsCamera {
  public:
    // Keep zoom strictly positive to avoid divide-by-zero in transforms.
    static constexpr float MinZoom = std::numeric_limits<float>::min();
    static constexpr float MaxZoom = 100.f;

  public:
    float Zoom = 1.f;
    glm::dvec2 Center = {};

  public:
    GraphicsCamera() = default;
    GraphicsCamera(float zoom, glm::dvec2 center)
        : Zoom(zoom), Center(center) {}

  public:
    void ZoomBy(Vec2F screenPos, RectF viewBounds, float zoom);
    void Translate(glm::dvec2 translation);

    glm::dvec2 ScreenToWorldPos(Vec2F pos, Rect viewBounds) const;
    glm::dvec2 WorldToScreenPos(Vec2D pos, Rect viewBounds,
                                Size2F worldSize) const;

    glm::mat4 OrthographicProjection(Size2 viewSize) const;
};
} // namespace Golde

#endif
