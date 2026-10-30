#ifndef GLDebug_hpp_
#define GLDebug_hpp_

#include <GL/glew.h>
#include <source_location>
#include <string_view>

// GL_DEBUG(statement) runs the statement and, in builds without NDEBUG,
// reports any OpenGL errors it raised. It expands to plain statements (no
// scope) so declarations like GL_DEBUG(auto result = glewInit()) stay visible.
#ifndef NDEBUG
#define GL_DEBUG(statement)                                                    \
    Golde::ClearGLErrors();                                                    \
    statement;                                                                 \
    Golde::LogGLErrors(#statement)
#else
#define GL_DEBUG(statement) statement
#endif

namespace Golde {
// Discards pending errors so the next check only sees the wrapped statement.
void ClearGLErrors();

// Prints every pending OpenGL error with the statement and call site.
void LogGLErrors(
    std::string_view statement,
    const std::source_location& location = std::source_location::current());
} // namespace Golde

#endif
