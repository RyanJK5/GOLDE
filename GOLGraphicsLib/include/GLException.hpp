#ifndef GLException_hpp_
#define GLException_hpp_

#include <exception>
#include <string_view>
#include <utility>

namespace Golde {
class GLException : public std::runtime_error {
  public:
    GLException(std::string_view str) : std::runtime_error(str.data()) {}
};
} // namespace Golde

#endif
