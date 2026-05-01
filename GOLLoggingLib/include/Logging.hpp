#ifndef Logger_hpp_
#define Logger_hpp_

#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>

#include <GL/glew.h>

#ifdef _DEBUG

#define GL_DEBUG(statement)                                                    \
    while (glGetError())                                                       \
        ;                                                                      \
    statement;                                                                 \
    Golde::LogGLErrors()
#define ERROR(str, ...)                                                        \
    Golde::Log(Golde::LogCode::Error, std::source_location::current(), str,    \
               __VA_ARGS__)
#define WARN(str, ...)                                                         \
    Golde::Log(Golde::LogCode::Error, std::source_location::current(), str,    \
               __VA_ARGS__)
#define INFO(str, ...)                                                         \
    Golde::Log(Golde::LogCode::Info, std::source_location::current(), str,     \
               __VA_ARGS__)
#else
#define GL_DEBUG(statement) statement
#define ERROR(str, ...)
#define WARN(str, ...)
#define INFO(str, ...)
#endif

namespace Golde {
enum class LogCode { GLError, Error, Warning, Info };

inline LogCode Level = LogCode::Info;

namespace logimpl {
constexpr std::string StringRepresentation(LogCode code) {
    switch (code) {
    case LogCode::GLError:
        return "[GL ERROR]";
    case LogCode::Error:
        return "[ERROR]";
    case LogCode::Warning:
        return "[WARNING]";
    case LogCode::Info:
        return "[INFO]";
    }
    return "";
}

std::string SimplifyFileName(const std::string& fileName);

std::string SimplifyFunctionName(const std::string& funcName);
} // namespace logimpl

template <typename... Args>
inline void Log(LogCode code, const std::source_location& location,
                std::format_string<Args...> str = "", Args&&... args) {
    if (code > Level)
        return;

    std::println("{} at {}:{} in {}", logimpl::StringRepresentation(code),
                 logimpl::SimplifyFileName(location.file_name()),
                 location.line(),
                 logimpl::SimplifyFunctionName(location.function_name()));

    if (!str.get().empty())
        std::println(str, std::forward<Args>(args)...);
}

void LogGLErrors(
    const std::source_location& location = std::source_location::current());
} // namespace Golde

#endif
