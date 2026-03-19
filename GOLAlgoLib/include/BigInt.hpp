#ifndef BigInt_h_
#define BigInt_h_

#include <boost/multiprecision/cpp_int.hpp>
#include <compare>

#include "Graphics2D.hpp"

namespace gol {
using BigInt = boost::multiprecision::cpp_int;

const inline BigInt BigZero{};
const inline BigInt BigOne{1};

inline std::strong_ordering operator<=>(const BigInt& lhs, const BigInt& rhs) {
    const auto cmp = lhs.compare(rhs);
    if (cmp < 0) {
        return std::strong_ordering::less;
    }
    if (cmp > 0) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

using BigVec2 = GenericVec<BigInt>;
} // namespace gol

namespace std {
template <>
struct formatter<gol::BigInt> {
    constexpr auto parse(std::format_parse_context& context) {
        return context.begin();
    }

    auto format(const gol::BigInt& num, auto& context) const {
        const auto s = num.str();
        const bool negative = !s.empty() && s[0] == '-';
        const auto digits =
            negative ? std::string_view{s}.substr(1) : std::string_view{s};
        return std::format_to(context.out(), "{}{}", negative ? "-" : "",
                              AddCommas(digits));
    }

  private:
    static std::string AddCommas(std::string_view str) {
        std::string result{};
        result.reserve(str.size() + (str.size() - 1UZ) / 3UZ);
        for (auto i = 0UZ; i < str.size(); i++) {
            if (i > 0UZ && (str.size() - i) % 3UZ == 0UZ) {
                result += ',';
            }
            result += str[i];
        }
        return result;
    }
};
} // namespace std

#endif