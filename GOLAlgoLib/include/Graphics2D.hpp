#ifndef Graphics2D_hpp_
#define Graphics2D_hpp_

#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include <imgui.h>
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

template <std::totally_ordered T>
struct GenericVec {
    T X;
    T Y;

    constexpr GenericVec() : X(0), Y(0) {}
    constexpr GenericVec(T x, T y) : X(x), Y(y) {}

    constexpr auto operator<=>(const GenericVec<T>&) const = default;

    template <typename Self>
    constexpr auto operator-(this const Self& self) {
        return Self{-self.X, -self.Y};
    }

    template <typename Self>
    constexpr auto operator+(this const Self& self,
                             const GenericVec<T>& other) {
        return Self{self.X + other.X, self.Y + other.Y};
    }

    template <typename Self>
    constexpr auto operator-(this const Self& self,
                             const GenericVec<T>& other) {
        return Self{self.X - other.X, self.Y - other.Y};
    }

    template <typename Self>
    constexpr auto& operator+=(this Self& self, const GenericVec<T>& other) {
        self.X += other.X;
        self.Y += other.Y;
        return self;
    }

    template <typename Self>
    constexpr auto& operator-=(this Self& self, const GenericVec<T>& other) {
        self.X -= other.X;
        self.Y -= other.Y;
        return self;
    }
};

template <std::totally_ordered T>
struct GenericSize {
    T Width;
    T Height;

    constexpr GenericSize() : Width(0), Height(0) {}
    constexpr GenericSize(T width, T height) : Width(width), Height(height) {}

    constexpr auto operator<=>(const GenericSize<T>&) const = default;
};

template <std::totally_ordered T>
struct GenericRect {
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
    constexpr GenericRect(const T& x, const T& y, const T& width,
                          const T& height)
        : X(x), Y(y), Width(width), Height(height) {}
    constexpr GenericRect(const GenericVec<T>& pos, const GenericSize<T>& size)
        : X(pos.X), Y(pos.Y), Width(size.Width), Height(size.Height) {}

    template <std::totally_ordered U>
    constexpr bool InBounds(const U& x, const U& y) const {
        return x >= X && x < X + Width && y >= Y && y < Y + Height;
    }

    template <std::totally_ordered U>
    constexpr bool InBounds(const GenericVec<U>& pos) const {
        return InBounds(pos.X, pos.Y);
    }

    template <std::totally_ordered U>
    constexpr bool Contains(const GenericRect<U>& other) const {
        return (other.X >= X) && (other.Y >= Y) &&
               (other.X + other.Width <= X + Width) &&
               (other.Y + other.Height <= Y + Height);
    }

    template <std::totally_ordered U>
    constexpr bool Intersects(const GenericRect<U>& other) const {
        return !(other.X >= X + Width || other.X + other.Width <= X ||
                 other.Y >= Y + Height || other.Y + other.Height <= Y);
    }

    constexpr auto operator<=>(const GenericRect&) const = default;
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

struct Vec2D : public GenericVec<double> {
    constexpr Vec2D() : GenericVec() {}
    constexpr explicit Vec2D(GenericVec<int32_t> vec)
        : GenericVec(static_cast<double>(vec.X), static_cast<double>(vec.Y)) {}
    constexpr Vec2D(ImVec2 vec)
        : GenericVec(static_cast<double>(vec.x), static_cast<double>(vec.y)) {}
    constexpr Vec2D(glm::dvec2 vec) : GenericVec(vec.x, vec.y) {}
    constexpr Vec2D(float x, float y)
        : GenericVec(static_cast<double>(x), static_cast<double>(y)) {}
    constexpr Vec2D(double x, double y) : GenericVec(x, y) {}

    // Allow conversion from Vec2F
    constexpr explicit Vec2D(GenericVec<float> vec)
        : GenericVec(static_cast<double>(vec.X), static_cast<double>(vec.Y)) {}

    constexpr operator glm::dvec2() const { return {X, Y}; }
    constexpr explicit operator Vec2F() const {
        return {static_cast<float>(X), static_cast<float>(Y)};
    }

    double Magnitude() const { return std::sqrt(X * X + Y * Y); }
    Vec2D Normalized() const {
        return Magnitude() == 0 ? Vec2D{0.0, 0.0}
                                : Vec2D{X / Magnitude(), Y / Magnitude()};
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
    constexpr RectF(const GenericRect<int>& rect)
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
using Size2L = GenericSize<int64_t>;

using Rect = GenericRect<int32_t>;
using RectL = GenericRect<int64_t>;
using RectDouble = GenericRect<double>;

struct RowMajorEqual {
    bool operator()(Vec2 a, Vec2 b) const {
        return a.Y != b.Y ? a.Y < b.Y : a.X < b.X;
    }
};
} // namespace gol

namespace std {
template <std::totally_ordered T>
struct formatter<gol::GenericVec<T>> {
    constexpr auto parse(std::format_parse_context& context) {
        return context.begin();
    }

    template <typename Context>
    auto format(const gol::GenericVec<T>& vec, Context& context) const {
        return std::format_to(context.out(), "({}, {})", vec.X, vec.Y);
    }
};
} // namespace std

#endif
