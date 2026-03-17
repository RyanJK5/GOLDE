#ifndef BigUInt_h_
#define BigUInt_h_

#include <algorithm>
#include <bit>
#include <compare>
#include <concepts>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <vector>

namespace gol {

class BigUInt {
  public:
    const static BigUInt Zero;

    BigUInt();
    explicit BigUInt(uint64_t v);

    std::strong_ordering operator<=>(const BigUInt& rhs) const;

    bool operator==(const BigUInt&) const = default;

    bool operator==(std::unsigned_integral auto other) const;

    BigUInt& operator+=(const BigUInt& rhs);

    BigUInt& operator+=(std::unsigned_integral auto other);

    BigUInt operator+(const BigUInt& rhs) const;

    BigUInt& operator++();

    BigUInt operator++(int);

    BigUInt& operator-=(std::unsigned_integral auto other);

    BigUInt& operator--();

    BigUInt operator--(int);

    BigUInt& operator<<=(size_t shift);

    BigUInt operator<<(size_t shift) const;

    std::string ToString() const;

    bool IsZero() const;

  private:
    void Trim();

  private:
    std::vector<uint32_t> m_Digits; // little-endian base-2^32
};

bool BigUInt::operator==(std::unsigned_integral auto other) const {
    if (m_Digits.size() > 2) {
        return false;
    }

    const auto otherValue = static_cast<uint64_t>(other);
    const auto lhsLow = static_cast<uint64_t>(m_Digits[0]);
    const auto lhsHigh =
        m_Digits.size() > 1 ? static_cast<uint64_t>(m_Digits[1]) : 0ULL;

    return ((lhsHigh << 32) | lhsLow) == otherValue;
}

bool operator==(std::unsigned_integral auto lhs, const BigUInt& rhs) {
    return rhs == lhs;
}

BigUInt& BigUInt::operator+=(std::unsigned_integral auto other) {
    auto carry = static_cast<uint64_t>(other);
    for (auto i = 0UZ; i < m_Digits.size() && carry; i++) {
        const auto sum = static_cast<uint64_t>(m_Digits[i]) + carry;
        m_Digits[i] = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }
    if (carry) {
        m_Digits.push_back(static_cast<uint32_t>(carry));
    }
    return *this;
}

BigUInt& BigUInt::operator-=(std::unsigned_integral auto other) {
    auto borrow = static_cast<uint64_t>(other);
    for (auto i = 0UZ; i < m_Digits.size() && borrow; i++) {
        const auto diff = static_cast<uint64_t>(m_Digits[i]) - borrow;
        m_Digits[i] = static_cast<uint32_t>(diff);
        borrow = diff >> 63; // 1 if underflowed, 0 otherwise
    }
    Trim();
    return *this;
}

BigUInt operator+(const BigUInt& lhs, std::unsigned_integral auto rhs) {
    BigUInt ret{lhs};
    ret += rhs;
    return ret;
}

BigUInt operator-(const BigUInt& lhs, std::unsigned_integral auto rhs) {
    BigUInt ret{lhs};
    ret -= rhs;
    return ret;
}

} // namespace gol

namespace std {
template <> struct formatter<gol::BigUInt> {
    constexpr auto parse(std::format_parse_context& context) {
        return context.begin();
    }

    template <typename Context>
    auto format(const gol::BigUInt& vec, Context& context) const {
        return std::format_to(context.out(), "{}", vec.ToString());
    }
};
} // namespace std

#endif