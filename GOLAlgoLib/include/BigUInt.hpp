#ifndef BigUInt_h_
#define BigUInt_h_

#include <algorithm>
#include <bit>
#include <compare>
#include <cstdint>
#include <ranges>
#include <string>
#include <vector>

namespace gol {

class BigUInt {
  public:
    BigUInt();
    explicit BigUInt(uint64_t v);

    std::strong_ordering operator<=>(const BigUInt& rhs) const;

    bool operator==(const BigUInt&) const = default;

    BigUInt& operator+=(const BigUInt& rhs);

    BigUInt operator+(const BigUInt& rhs);

    BigUInt& operator<<=(size_t shift);

    BigUInt operator<<(size_t shift);

    std::string ToString() const;

  private:
    std::vector<uint32_t> m_Digits; // little-endian base-2^32

    void Trim();
};

} // namespace gol

#endif