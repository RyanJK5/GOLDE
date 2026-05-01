#ifndef LifeRule_hpp_
#define LifeRule_hpp_

#include <algorithm>
#include <array>
#include <charconv>
#include <expected>
#include <string_view>

#include "Graphics2D.hpp"

namespace Golde {

using namespace std::literals::string_view_literals;

enum class TopologyKind { Plane, Torus };

class LifeRule {
  public:
    constexpr static uint32_t NumLeafPatterns = 1 << 16;
    using LookupTable = std::array<uint16_t, NumLeafPatterns>;

    constexpr static std::expected<LifeRule, std::string_view>
    Make(std::string_view ruleString);

    constexpr static std::expected<std::string, std::string_view>
    Canonicalize(std::string_view ruleString);

    constexpr static std::expected<void, std::string_view>
    IsValidRule(std::string_view ruleString);

    constexpr static std::expected<Size2, std::string_view>
    ExtractDimensions(std::string_view ruleString);

    constexpr static std::expected<TopologyKind, std::string_view>
    ExtractTopologyKind(std::string_view ruleString);

    constexpr LifeRule(int32_t birthMask, int32_t surviveMask, Rect bounds = {},
                       TopologyKind topology = TopologyKind::Plane);
    constexpr const LookupTable& Table() const;

    constexpr std::optional<Rect> Bounds() const;

    constexpr TopologyKind GetTopology() const;

  private:
    constexpr LookupTable BuildRuleTable(int32_t birthMask,
                                         int32_t surviveMask);

    template <typename ExtractType>
    constexpr static std::expected<ExtractType, std::string_view>
    TryMake(std::string_view ruleString);

  private:
    LookupTable m_RuleTable;
    Rect m_Bounds;
    TopologyKind m_TopologyKind;
};

template <typename ExtractType>
constexpr std::expected<ExtractType, std::string_view>
LifeRule::TryMake(std::string_view ruleString) {
    const auto slash = ruleString.find('/');
    if (slash == std::string_view::npos ||
        (ruleString[0] != 'B' && ruleString[0] != 'b') ||
        (ruleString[slash + 1] != 'S' && ruleString[slash + 1] != 's')) {
        return std::unexpected{
            "Rule string must be in B.../S...:[P|T]... format."sv};
    }

    const auto surviveEnd = [&] {
        auto topologyBegin = ruleString.find(":");
        if (topologyBegin == std::string_view::npos) {
            return ruleString.size();
        } else {
            return topologyBegin;
        }
    }();

    auto birthMask = 0;
    for (char c : ruleString.substr(1, slash - 1)) {
        if (c == '0') {
            return std::unexpected{"B0 rules are not currently supported."sv};
        }
        if (c < '0' || c > '8') {
            return std::unexpected{"Invalid birth neighbor count."sv};
        }
        birthMask |= (1 << (c - '0'));
    }

    auto surviveMask = 0;
    for (char c : ruleString.substr(slash + 2, surviveEnd - (slash + 2))) {
        if (c < '0' || c > '8') {
            return std::unexpected{"Invalid survive neighbor count."sv};
        }
        surviveMask |= (1 << (c - '0'));
    }

    const auto topologyKind =
        [&] -> std::expected<TopologyKind, std::string_view> {
        if (surviveEnd == ruleString.size()) {
            return TopologyKind::Plane;
        }
        if (surviveEnd + 2 >= ruleString.size()) {
            return std::unexpected{"Invalid topology specification."sv};
        }

        const auto topologyChar = ruleString[surviveEnd + 1];
        switch (topologyChar) {
        case 'p':
        case 'P':
            return TopologyKind::Plane;
        case 't':
        case 'T':
            return TopologyKind::Torus;
        case 'k':
        case 'K':
            return std::unexpected{
                "Klein bottle topology is not currently supported."sv};
        case 'c':
        case 'C':
            return std::unexpected{
                "Cross-surface topology is not currently supported."sv};
        case 's':
        case 'S':
            return std::unexpected{
                "Sphere topology is not currently supported."sv};
        default:
            return std::unexpected{"Unkown topology."sv};
        }
    }();

    if (!topologyKind) {
        return std::unexpected{topologyKind.error()};
    }

    const auto bounds = [&] -> std::expected<Size2, std::string_view> {
        if (surviveEnd == ruleString.size()) {
            return Size2{};
        }

        const auto separatorIndex = [&] {
            const auto ret = ruleString.find(',', surviveEnd);
            if (ret == std::string_view::npos) {
                return ruleString.size();
            }
            return ret;
        }();

        auto width = 0;
        auto [pointer, ec] =
            std::from_chars(ruleString.data() + surviveEnd + 2,
                            ruleString.data() + separatorIndex, width);

        if (ec != std::errc{}) {
            return std::unexpected{"Invalid topology width."sv};
        }

        if (separatorIndex == ruleString.size()) {
            return Size2{width, width};
        }

        auto height = 0;
        auto [pointer2, ec2] = std::from_chars(
            pointer + 1, ruleString.data() + ruleString.size(), height);

        if (ec2 != std::errc{}) {
            return std::unexpected{"Invalid topology height."sv};
        }

        return Size2{width, height};
    }();

    if (!bounds) {
        return std::unexpected{bounds.error()};
    }

    if constexpr (std::is_same_v<ExtractType, LifeRule>) {
        return LifeRule{birthMask, surviveMask, Rect{Vec2{}, *bounds},
                        *topologyKind};
    } else if constexpr (std::is_same_v<ExtractType, Size2>) {
        return *bounds;
    } else if constexpr (std::is_same_v<ExtractType, TopologyKind>) {
        return *topologyKind;
    } else {
        return std::expected<void, std::string_view>{};
    }
}

constexpr std::expected<std::string, std::string_view>
LifeRule::Canonicalize(std::string_view ruleString) {
    if (auto isValid = IsValidRule(ruleString); !isValid) {
        return std::unexpected{isValid.error()};
    }

    std::string result{};
    result.reserve(ruleString.size());
    std::ranges::remove_copy_if(ruleString, std::back_inserter(result),
                                [](char c) { return std::isspace(c); });

    for (auto& c : result) {
        if (std::islower(c)) {
            c = std::toupper(c);
        }
    }

    const auto canonicalizeSegment = [&](size_t start, size_t end) {
        auto sub =
            std::ranges::subrange(result.begin() + start, result.begin() + end);
        std::ranges::sort(sub);

        auto [newEnd, oldEnd] = std::ranges::unique(sub);
        result.erase(newEnd, oldEnd);
    };

    auto colonPos = result.find(':');
    canonicalizeSegment(result.find('S') + 1, colonPos == std::string::npos
                                                  ? result.size()
                                                  : colonPos);
    canonicalizeSegment(result.find('B') + 1, result.find('/'));
    colonPos = result.find(':');

    if (auto dimensions = ExtractDimensions(ruleString)) {
        if (dimensions->Width == 0 && dimensions->Height == 0) {
            if (colonPos != std::string::npos) {
                result.erase(result.begin() + colonPos, result.end());
            }
        } else if (dimensions->Width == dimensions->Height &&
                   result.find(',') == std::string::npos) {
            const auto endPos = result.size();
            result.push_back(',');
            for (auto i = colonPos + 2; i < endPos; i++) {
                result.push_back(result[i]);
            }
        }
    }

    return result;
}

constexpr std::expected<LifeRule, std::string_view>
LifeRule::Make(std::string_view ruleString) {
    return TryMake<LifeRule>(ruleString);
}

constexpr std::expected<void, std::string_view>
LifeRule::IsValidRule(std::string_view ruleString) {
    return TryMake<void>(ruleString);
}

constexpr std::expected<Size2, std::string_view>
LifeRule::ExtractDimensions(std::string_view ruleString) {
    return TryMake<Size2>(ruleString);
}

constexpr std::expected<TopologyKind, std::string_view>
LifeRule::ExtractTopologyKind(std::string_view ruleString) {
    return TryMake<TopologyKind>(ruleString);
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

constexpr TopologyKind LifeRule::GetTopology() const { return m_TopologyKind; }

constexpr LifeRule::LifeRule(int32_t birthMask, int32_t surviveMask,
                             Rect bounds, TopologyKind topology)
    : m_RuleTable(BuildRuleTable(birthMask, surviveMask)), m_Bounds(bounds),
      m_TopologyKind(topology) {}

} // namespace Golde
#endif