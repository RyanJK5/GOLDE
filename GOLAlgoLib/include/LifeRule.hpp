#ifndef LifeRule_hpp_
#define LifeRule_hpp_
#include <array>
#include <charconv>
#include <expected>
#include <string_view>

#include "Graphics2D.hpp"

namespace gol {
class LifeRule {
  public:
    constexpr static uint32_t NumLeafPatterns = 1 << 16;
    using LookupTable = std::array<uint16_t, NumLeafPatterns>;

    constexpr static std::expected<LifeRule, std::string_view>
    Make(std::string_view ruleString);

    constexpr static std::expected<void, std::string_view>
    IsValidRule(std::string_view ruleString);

    constexpr static std::expected<Size2, std::string_view>
    ExtractDimensions(std::string_view ruleString);

    constexpr LifeRule(int32_t birthMask, int32_t surviveMask,
                       Rect bounds = {});
    constexpr const LookupTable& Table() const;

    constexpr std::optional<Rect> Bounds() const;

  private:
    constexpr LookupTable BuildRuleTable(int32_t birthMask,
                                         int32_t surviveMask);

    template <bool FullyConstruct>
    constexpr static std::expected<
        std::conditional_t<FullyConstruct, LifeRule, Size2>, std::string_view>
    TryMake(std::string_view ruleString);

  private:
    LookupTable m_RuleTable;
    Rect m_Bounds;
};

template <bool FullyConstruct>
constexpr std::expected<std::conditional_t<FullyConstruct, LifeRule, Size2>,
                        std::string_view>
LifeRule::TryMake(std::string_view ruleString) {
    const auto slash = ruleString.find('/');
    if (slash == std::string_view::npos || ruleString[0] != 'B' ||
        ruleString[slash + 1] != 'S') {
        return std::unexpected{
            std::string_view{"Rule string must be in B.../S...:P... format."}};
    }

    const auto surviveEnd = [&] {
        auto topologyBegin = ruleString.find(":P");
        if (topologyBegin == std::string_view::npos) {
            return ruleString.size();
        } else {
            return topologyBegin;
        }
    }();

    auto birthMask = 0;
    for (char c : ruleString.substr(1, slash - 1)) {
        if (c == '0') {
            return std::unexpected{
                std::string_view{"B0 rules are not currently supported."}};
        }
        if (c < '0' || c > '8') {
            return std::unexpected{
                std::string_view{"Invalid birth neighbor count."}};
        }
        birthMask |= (1 << (c - '0'));
    }

    auto surviveMask = 0;
    for (char c : ruleString.substr(slash + 2, surviveEnd - (slash + 2))) {
        if (c < '0' || c > '8') {
            return std::unexpected{
                std::string_view{"Invalid survive neighbor count."}};
        }
        surviveMask |= (1 << (c - '0'));
    }

    const auto bounds = [&] -> std::expected<Size2, std::string_view> {
        if (surviveEnd == ruleString.size()) {
            return Size2{};
        }

        const auto separatorIndex = ruleString.find(',', surviveEnd);

        auto width = 0;
        auto [pointer, ec] =
            std::from_chars(ruleString.data() + surviveEnd + 2,
                            ruleString.data() + separatorIndex, width);

        if (ec != std::errc{}) {
            return std::unexpected{std::string_view{"Invalid topology width."}};
        }

        auto height = 0;
        auto [pointer2, ec2] = std::from_chars(
            pointer + 1, ruleString.data() + ruleString.size(), height);

        if (ec2 != std::errc{}) {
            return std::unexpected{
                std::string_view{"Invalid topology height."}};
        }

        return Size2{width, height};
    }();

    if (!bounds) {
        return std::unexpected{bounds.error()};
    }

    if constexpr (FullyConstruct) {
        return LifeRule{birthMask, surviveMask, Rect{Vec2{}, *bounds}};
    } else {
        return *bounds;
    }
}

constexpr std::expected<LifeRule, std::string_view>
LifeRule::Make(std::string_view ruleString) {
    return TryMake<true>(ruleString);
}

constexpr std::expected<void, std::string_view>
LifeRule::IsValidRule(std::string_view ruleString) {
    return TryMake<false>(ruleString).transform([](auto&&) {});
}

constexpr std::expected<Size2, std::string_view>
LifeRule::ExtractDimensions(std::string_view ruleString) {
    return TryMake<false>(ruleString);
}

constexpr LifeRule::LookupTable LifeRule::BuildRuleTable(int32_t birthMask,
                                                         int32_t surviveMask) {
    // The four center cells of a 4x4 grid whose next state we compute.
    constexpr std::array centerCol{1, 2, 1, 2};
    constexpr std::array centerRow{1, 1, 2, 2};
    // Where each center cell's result is placed in the output.
    constexpr std::array resultBit{5, 4, 1, 0};
    // Returns the bit position (0-15) for a cell at (col, row) in a 4x4 grid.
    constexpr auto bitPosition = [](int32_t col, int32_t row) {
        return (3 - row) * 4 + (3 - col);
    };

    LookupTable table{};
    for (uint32_t pattern = 0; pattern < NumLeafPatterns; ++pattern) {
        uint16_t result = 0;
        for (int cell = 0; cell < 4; ++cell) {
            const int col = centerCol[cell];
            const int row = centerRow[cell];
            int neighborCount = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0)
                        continue;
                    neighborCount += static_cast<int>(
                        (pattern >> bitPosition(col + dx, row + dy)) & 1);
                }
            }
            const bool isAlive = ((pattern >> bitPosition(col, row)) & 1) != 0;
            const bool survives =
                isAlive && ((surviveMask >> neighborCount) & 1);
            const bool born = !isAlive && ((birthMask >> neighborCount) & 1);
            if (survives || born)
                result |= static_cast<uint16_t>(1 << resultBit[cell]);
        }
        table[pattern] = result;
    }
    return table;
}

constexpr const LifeRule::LookupTable& LifeRule::Table() const {
    return m_RuleTable;
}

constexpr std::optional<Rect> LifeRule::Bounds() const {
    if (m_Bounds == Rect{}) {
        return std::nullopt;
    }
    return m_Bounds;
}

constexpr LifeRule::LifeRule(int32_t birthMask, int32_t surviveMask,
                             Rect bounds)
    : m_RuleTable(BuildRuleTable(birthMask, surviveMask)), m_Bounds(bounds) {}

} // namespace gol
#endif