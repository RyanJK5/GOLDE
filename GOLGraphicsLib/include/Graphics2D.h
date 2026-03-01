#ifndef Graphics2D_h_
#define Graphics2D_h_

#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <utility>

namespace gol {
struct Color {
    float Red;
    float Green;
    float Blue;
    float Alpha;

    constexpr Color() : Red(0), Green(0), Blue(0), Alpha(1) {}
    constexpr Color(float r, float g, float b, float a = 1.f)
        : Red(r), Green(g), Blue(b), Alpha(a) {}
};

template <std::totally_ordered T> struct GenericVec {
    T X;
    T Y;

    constexpr GenericVec() : X(0), Y(0) {}
    constexpr GenericVec(T x, T y) : X(x), Y(y) {}

    constexpr auto operator<=>(const GenericVec<T> &) const = default;

    constexpr GenericVec operator-() const { return {-X, -Y}; }

    constexpr GenericVec operator+(GenericVec<T> other) const {
        return {X + other.X, Y + other.Y};
    }
    constexpr GenericVec operator-(GenericVec<T> other) const {
        return {X - other.X, Y - other.Y};
    }

    constexpr GenericVec &operator+=(GenericVec<T> other) {
        X += other.X;
        Y += other.Y;
        return *this;
    }
    constexpr GenericVec &operator-=(GenericVec<T> other) {
        X -= other.X;
        Y -= other.Y;
        return *this;
    }
};

template <std::totally_ordered T> struct GenericSize {
    T Width;
    T Height;

    constexpr GenericSize() : Width(0), Height(0) {}
    constexpr GenericSize(T width, T height) : Width(width), Height(height) {}
};

template <std::totally_ordered T> struct GenericRect {
    T X;
    T Y;
    T Width;
    T Height;

    constexpr GenericVec<T> Pos() const { return {X, Y}; }
    constexpr GenericSize<T> Size() const { return {Width, Height}; }

    constexpr GenericVec<T> UpperLeft() const { return {X, Y}; }
    constexpr GenericVec<T> UpperRight() const { return {X + Width, Y}; }
    constexpr GenericVec<T> LowerLeft() const { return {X, Y + Height}; }
    constexpr GenericVec<T> LowerRight() const {
        return {X + Width, Y + Height};
    }
    constexpr GenericVec<T> Center() const {
        return {X + Width / 2, Y + Height / 2};
    }

    constexpr GenericRect() : GenericRect(0, 0, 0, 0) {}
    constexpr GenericRect(T x, T y, T width, T height)
        : X(x), Y(y), Width(width), Height(height) {}
    constexpr GenericRect(GenericVec<T> pos, GenericSize<T> size)
        : X(pos.X), Y(pos.Y), Width(size.Width), Height(size.Height) {}

    constexpr bool InBounds(std::totally_ordered auto x,
                            std::totally_ordered auto y) const {
        return x >= X && x < X + Width && y >= Y && y < Y + Height;
    }

    constexpr bool InBounds(GenericVec<T> pos) const {
        return InBounds(pos.X, pos.Y);
    }
};

struct Vec2F : public GenericVec<float> {
    constexpr Vec2F() : GenericVec() {}
    constexpr explicit Vec2F(GenericVec<int32_t> vec)
        : GenericVec(static_cast<float>(vec.X), static_cast<float>(vec.Y)) {}
    constexpr Vec2F(ImVec2 vec) : GenericVec(vec.x, vec.y) {}
    constexpr Vec2F(glm::vec2 vec) : GenericVec(vec.x, vec.y) {}
    constexpr Vec2F(float x, float y) : GenericVec(x, y) {}

    constexpr operator ImVec2() const { return {X, Y}; }
    constexpr operator glm::vec2() const { return {X, Y}; }

    float Magnitude() const { return std::sqrt(X * X + Y * Y); }
    Vec2F Normalized() const {
        return Magnitude() == 0 ? Vec2F{0.f, 0.f}
                                : Vec2F{X / Magnitude(), Y / Magnitude()};
    }
};

struct Size2F : public GenericSize<float> {
    constexpr Size2F() : GenericSize() {}
    constexpr Size2F(ImVec2 vec) : GenericSize(vec.x, vec.y) {}
    constexpr Size2F(float x, float y) : GenericSize(x, y) {}

    constexpr operator ImVec2() const { return {Width, Height}; }
};

struct RectF : public GenericRect<float> {
    constexpr RectF() : GenericRect() {}
    constexpr RectF(const GenericRect<int> &rect)
        : RectF(static_cast<float>(rect.X), static_cast<float>(rect.Y),
                static_cast<float>(rect.Width),
                static_cast<float>(rect.Height)) {}
    constexpr RectF(float x, float y, float width, float height)
        : GenericRect(x, y, width, height) {}
    constexpr RectF(GenericVec<float> pos, GenericSize<float> size)
        : GenericRect(pos, size) {}

    constexpr operator GenericRect<int32_t>() const {
        return {
            static_cast<int32_t>(X),
            static_cast<int32_t>(Y),
            static_cast<int32_t>(Width),
            static_cast<int32_t>(Height),
        };
    }
};

using Vec2 = GenericVec<int32_t>;
using Vec2L = GenericVec<int64_t>;

using Size2 = GenericSize<int32_t>;

using Rect = GenericRect<int32_t>;
using RectL = GenericRect<int64_t>;
using RectDouble = GenericRect<double>;
} // namespace gol

namespace std {
template <std::totally_ordered T> struct formatter<gol::GenericVec<T>> {
    constexpr auto parse(std::format_parse_context &context) {
        return context.begin();
    }

    template <typename Context>
    auto format(const gol::GenericVec<T> &vec, Context &context) const {
        return std::format_to(context.out(), "({}, {})", vec.X, vec.Y);
    }
};
} // namespace std

#endif