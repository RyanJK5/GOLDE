#ifndef BigInt_hpp_
#define BigInt_hpp_

#include <boost/multiprecision/cpp_int.hpp>
#include <compare>

#include "Graphics2D.hpp"

namespace Golde {
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
using BigRect = GenericRect<BigInt>;
} // namespace Golde

template <>
struct std::formatter<Golde::BigInt> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it == 'L') {
            m_UseLocale = true;
            ++it;
        }
        return it;
    }

    auto format(const Golde::BigInt& num, auto& ctx) const {
        const std::string str{num.str()};
        std::string_view sv{str};

        const bool negative = !sv.empty() && sv[0] == '-';
        if (negative) {
            sv.remove_prefix(1);
        }

        if (sv.size() <= 10UZ) {
            return FormatSmall(sv, negative, ctx);
        }
        return FormatBig(sv, negative, ctx);
    }

  private:
    bool m_UseLocale = false;

    auto FormatBig(std::string_view str, bool negative, auto& ctx) const {
        const auto exponent = str.size() - 1UZ;

        const auto mantissaLength = std::min(str.size() - 1, 2UZ);
        const std::string_view mantissa{str.begin() + 1,
                                        str.begin() + 1 + mantissaLength};

        return std::format_to(ctx.out(), "{}{}.{}e+{}", negative ? "-" : "",
                              str[0], mantissa, exponent);
    }

    auto FormatSmall(std::string_view str, bool negative, auto& ctx) const {
        const auto& loc = ctx.locale();
        const auto& punct = std::use_facet<std::numpunct<char>>(loc);
        const std::string grouping = punct.grouping();
        const auto seperator = punct.thousands_sep();

        auto out = ctx.out();
        if (negative) {
            *out++ = '-';
        }

        if (!m_UseLocale || grouping.empty()) {
            return std::copy(str.begin(), str.end(), out);
        }

        auto digitsToSeperator = str.size() % 3UZ;
        if (digitsToSeperator == 0) {
            digitsToSeperator = 3;
        }

        for (auto i = 0UZ; i < str.size(); i++) {
            if (i > 0UZ && digitsToSeperator == 0UZ) {
                *out++ = seperator;
                digitsToSeperator = 3UZ;
            }
            *out++ = str[i];
            digitsToSeperator--;
        }

        return out;
    }
};

#endif