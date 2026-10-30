#ifndef GraphicsInterop_hpp_
#define GraphicsInterop_hpp_

#include <glm/glm.hpp>
#include <imgui.h>

#include "Graphics2D.hpp"

// Explicit conversions between Golde's geometry types and ImGui/glm, kept out
// of Graphics2D.hpp so the algorithm layer builds without GUI headers.
namespace Golde {
constexpr ImVec2 ToImVec2(Vec2F vec) { return {vec.X, vec.Y}; }
constexpr ImVec2 ToImVec2(Size2F size) { return {size.Width, size.Height}; }

constexpr Vec2F ToVec2F(ImVec2 vec) { return {vec.x, vec.y}; }
constexpr Size2F ToSize2F(ImVec2 vec) { return {vec.x, vec.y}; }

constexpr glm::vec2 ToGlm(Vec2F vec) { return {vec.X, vec.Y}; }
} // namespace Golde

#endif
