#ifndef GLBuffer_hpp_
#define GLBuffer_hpp_

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <concepts>
#include <cstdint>
#include <utility>

#include "Logging.hpp"

namespace Golde {
template <typename T>
concept GLGenerator = std::same_as<T, decltype(&glGenBuffers)> ||
                      std::same_as<T, decltype(&glGenTextures)>;

template <typename T>
concept GLDeleter = std::same_as<T, decltype(&glDeleteBuffers)> ||
                    std::same_as<T, decltype(&glDeleteTextures)>;

template <auto Generator, auto Deleter>
class GLWrapper {
  public:
    static_assert(GLGenerator<decltype(Generator)>);
    static_assert(GLDeleter<decltype(Deleter)>);

    GLWrapper();

    GLWrapper(const GLWrapper<Generator, Deleter>&) = delete;

    auto& operator=(const GLWrapper<Generator, Deleter>&) = delete;

    GLWrapper(GLWrapper<Generator, Deleter>&& other) noexcept;

    auto& operator=(GLWrapper<Generator, Deleter>&& other) noexcept;

    ~GLWrapper();

    uint32_t ID() const { return m_ID; }

  private:
    uint32_t m_ID = 0;
};

using GLBuffer = GLWrapper<&glGenBuffers, &glDeleteBuffers>;

using GLIndexBuffer = GLBuffer;

using GLFrameBuffer = GLWrapper<&glGenFramebuffers, &glDeleteFramebuffers>;

using GLRenderBuffer = GLWrapper<&glGenRenderbuffers, &glDeleteRenderbuffers>;

using GLTexture = GLWrapper<&glGenTextures, &glDeleteTextures>;

using GLVertexArray = GLWrapper<&glGenVertexArrays, &glDeleteVertexArrays>;

template <auto Generator, auto Deleter>
GLWrapper<Generator, Deleter>::GLWrapper() {
    GL_DEBUG((*Generator)(1, &m_ID));
}

template <auto Generator, auto Deleter>
GLWrapper<Generator, Deleter>::GLWrapper(
    GLWrapper<Generator, Deleter>&& other) noexcept {
    m_ID = std::exchange(other.m_ID, 0);
}

template <auto Generator, auto Deleter>
auto& GLWrapper<Generator, Deleter>::operator=(
    GLWrapper<Generator, Deleter>&& other) noexcept {
    if (this != &other)
        m_ID = std::exchange(other.m_ID, 0);
    return *this;
}

template <auto Generator, auto Deleter>
GLWrapper<Generator, Deleter>::~GLWrapper() {
    GL_DEBUG((*Deleter)(1, &m_ID));
}
} // namespace Golde
#endif
