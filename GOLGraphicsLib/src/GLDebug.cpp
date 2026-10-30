#include <GL/glew.h>
#include <cstdio>
#include <filesystem>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>

#include "GLDebug.hpp"

namespace Golde {
// glGetError can keep returning an error forever (no current context, lost
// context), so never drain more than this many per check.
constexpr int MaxErrorsPerCheck = 16;

static std::string ErrorName(GLenum error) {
    switch (error) {
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    case GL_STACK_UNDERFLOW:
        return "GL_STACK_UNDERFLOW";
    case GL_STACK_OVERFLOW:
        return "GL_STACK_OVERFLOW";
    default:
        return std::format("0x{:04X}", error);
    }
}

void ClearGLErrors() {
    for (int i = 0; i < MaxErrorsPerCheck && glGetError() != GL_NO_ERROR; i++)
        ;
}

void LogGLErrors(std::string_view statement,
                 const std::source_location& location) {
    for (int i = 0; i < MaxErrorsPerCheck; i++) {
        const auto error = glGetError();
        if (error == GL_NO_ERROR)
            return;

        std::println(
            stderr, "[GL ERROR] {} at {}:{} in {}\n    {}", ErrorName(error),
            std::filesystem::path{location.file_name()}.filename().string(),
            location.line(), location.function_name(), statement);
    }
}
} // namespace Golde
