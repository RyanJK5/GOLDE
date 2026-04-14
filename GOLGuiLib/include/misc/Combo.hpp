#ifndef Combo_hpp_
#define Combo_hpp_

#include <span>
#include <string_view>
#include <vector>

namespace gol {
class Combo {
  public:
    constexpr Combo(std::string_view label,
                    std::span<const std::string_view> options);

    template <typename... Args>
    constexpr Combo(std::string_view label, Args&&... options);

    void Update();

    int32_t ActiveIndex = 0;

  private:
    std::vector<char> m_Data;
    std::string_view m_Label;
};

constexpr Combo::Combo(std::string_view label,
                       std::span<const std::string_view> options)
    : m_Label(label) {
    for (auto str : options) {
        for (char c : str) {
            m_Data.push_back(c);
        }
        m_Data.push_back('\0');
    }
    m_Data.push_back('\0');
}

template <typename... Args>
constexpr Combo::Combo(std::string_view label, Args&&... options)
    : m_Label(label) {
    (
        [&](auto&& option) {
            std::string_view str{option};

            for (char c : str) {
                m_Data.push_back(c);
            }
            m_Data.push_back('\0');
        }(options),
        ...);
    m_Data.push_back('\0');
}
} // namespace gol

#endif