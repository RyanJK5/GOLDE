#ifndef Camera_h_
#define Camera_h_

#include <glm/glm.hpp>

#include "Graphics2D.hpp"

namespace gol {
class GraphicsCamera {
  public:
    static constexpr float MinZoom = 0.0001f;
    static constexpr float MaxZoom = 60.f;

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
} // namespace gol

#endif
