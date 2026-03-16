#include "BigUInt.hpp"
#include <compare>
#include <limits>

namespace gol {

void BigUInt::Trim() {
    while (m_Digits.size() > 1 && m_Digits.back() == 0) {
        m_Digits.pop_back();
    }
}

BigUInt::BigUInt() : m_Digits{static_cast<uint32_t>(0)} {}

BigUInt::BigUInt(uint64_t value)
    : m_Digits{
          static_cast<uint32_t>(value & std::numeric_limits<uint32_t>::max()),
          static_cast<uint32_t>(value >> 32)} {
    Trim();
}

std::strong_ordering BigUInt::operator<=>(const BigUInt& rhs) const {
    if (m_Digits.size() != rhs.m_Digits.size()) {
        return m_Digits.size() <=> rhs.m_Digits.size();
    }
    for (auto i = static_cast<int32_t>(m_Digits.size()) - 1; i >= 0; i--) {
        if (m_Digits[i] != rhs.m_Digits[i]) {
            return m_Digits[i] <=> rhs.m_Digits[i];
        }
    }
    return std::strong_ordering::equal;
}

BigUInt& BigUInt::operator+=(const BigUInt& rhs) {
    if (rhs.m_Digits.size() > m_Digits.size()) {
        m_Digits.resize(rhs.m_Digits.size(), 0U);
    }

    auto carry = static_cast<uint64_t>(0);
    for (auto i = 0UZ; i < m_Digits.size(); i++) {
        const auto sum = static_cast<uint64_t>(m_Digits[i]) +
                         (i < rhs.m_Digits.size() ? rhs.m_Digits[i] : 0U) +
                         carry;
        m_Digits[i] = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }

    if (carry) {
        m_Digits.push_back(static_cast<uint32_t>(carry));
    }

    return *this;
}

BigUInt BigUInt::operator+(const BigUInt& other) {
    BigUInt ret{*this};
    ret += other;
    return ret;
}

BigUInt& BigUInt::operator<<=(size_t shift) {
    const auto digitShift = shift / 32UZ;
    const auto bitShift = shift % 32UZ;

    m_Digits.insert(m_Digits.begin(), digitShift, 0U);

    if (bitShift > 0) {
        uint32_t carry = 0;
        for (auto i = digitShift; i < m_Digits.size(); i++) {
            const auto wide = static_cast<uint64_t>(
                static_cast<uint64_t>(m_Digits[i]) << bitShift | carry);
            m_Digits[i] = static_cast<uint32_t>(wide);
            carry = static_cast<uint32_t>(wide >> 32);
        }

        if (carry) {
            m_Digits.push_back(carry);
        }
    }

    return *this;
}

BigUInt BigUInt::operator<<(size_t shift) {
    BigUInt ret{*this};
    ret <<= shift;
    return ret;
}

std::string BigUInt::ToString() const {
    auto copy = m_Digits;

    const auto isZero = [&] { return copy.size() == 1UZ && copy[0] == 0; };

    const auto divideBy10 = [&] {
        auto remainder = static_cast<uint64_t>(0);
        for (auto i = static_cast<int32_t>(m_Digits.size()) - 1; i >= 0; i--) {
            const uint64_t current = (remainder << 32) | copy[i];
            copy[i] = static_cast<uint32_t>(current / 10);
            remainder = current % 10;
        }

        while (copy.size() > 1 && copy.back() == 0) {
            copy.pop_back();
        }
        return static_cast<uint32_t>(remainder);
    };

    std::string result{};
    while (!isZero()) {
        result += static_cast<char>('0' + divideBy10());
    }

    if (result.empty()) {
        result = "0";
    }

    std::ranges::reverse(result);
    return result;
}
} // namespace gol