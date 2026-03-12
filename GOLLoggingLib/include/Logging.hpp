#ifndef Logger_h_
#define Logger_h_

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
    gol::LogGLErrors()
#define ERROR(str, ...)                                                        \
    gol::Log(gol::LogCode::Error, std::source_location::current(), str,        \
             __VA_ARGS__)
#define WARN(str, ...)                                                         \
    gol::Log(gol::LogCode::Error, std::source_location::current(), str,        \
             __VA_ARGS__)
#define INFO(str, ...)                                                         \
    gol::Log(gol::LogCode::Info, std::source_location::current(), str,         \
             __VA_ARGS__)
#else
#define GL_DEBUG(statement) statement
#define ERROR(str, ...)
#define WARN(str, ...)
#define INFO(str, ...)
#endif

namespace gol {
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

std::string SimplifyFileName(const std::string &fileName);

std::string SimplifyFunctionName(const std::string &funcName);
} // namespace logimpl

template <typename... Args>
inline void Log(LogCode code, const std::source_location &location,
                std::format_string<Args...> str = "", Args &&...args) {
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
    const std::source_location &location = std::source_location::current());
} // namespace gol

#endif