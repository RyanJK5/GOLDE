#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <print>

#include "Camera.hpp"
#include "Graphics2D.hpp"

namespace Golde {
void GraphicsCamera::ZoomBy(Vec2F screenPos, RectF viewBounds, float zoom) {
    const auto scale = 1.f + zoom;
    if (!std::isfinite(scale) || scale <= 0.f) {
        return;
    }

    const auto nextZoom = Zoom * scale;
    if (!std::isfinite(nextZoom) || nextZoom < MinZoom) {
        Zoom = MinZoom;
        return;
    }
    if (nextZoom > MaxZoom) {
        Zoom = MaxZoom;
        return;
    }

    Zoom = nextZoom;

    Center += (ScreenToWorldPos(screenPos, viewBounds) - Center) *
              static_cast<double>(zoom);
}

void GraphicsCamera::Translate(glm::dvec2 delta) {
    Center -= delta / static_cast<double>(Zoom);
}

glm::dvec2 GraphicsCamera::ScreenToWorldPos(Vec2F pos, Rect viewBounds) const {
    auto vec = glm::dvec2{pos.X - viewBounds.X, pos.Y - viewBounds.Y};
    vec -= glm::dvec2{viewBounds.Width / 2.0, viewBounds.Height / 2.0};
    vec /= Zoom;
    vec += Center;
    return vec;
}

glm::dvec2 GraphicsCamera::WorldToScreenPos(Vec2D pos, Rect viewBounds,
                                            Size2F worldSize) const {
    auto vec = glm::dvec2(pos.X, pos.Y);
    vec.x -= Center.x;
    vec.y += Center.y;
    vec *= static_cast<double>(Zoom);
    vec += glm::dvec2{viewBounds.Width / 2.0,
                      viewBounds.Height / 2.0 - worldSize.Height * Zoom};
    return vec;
}

glm::mat4 GraphicsCamera::OrthographicProjection(Size2 viewSize) const {
    return glm::ortho(-(viewSize.Width / 2.f) / Zoom,
                      +(viewSize.Width / 2.f) / Zoom,
                      +(viewSize.Height / 2.f) / Zoom,
                      -(viewSize.Height / 2.f) / Zoom, -1.f, 1.f);
}

} // namespace Golde
