#ifndef RLEEncoder_h_
#define RLEEncoder_h_

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "Logging.hpp"

namespace gol::RLEEncoder {
enum class RLEStorageType { UInt8, UInt16, UInt32, UInt64 };

constexpr RLEStorageType SelectStorageType(uint64_t count) {
    using enum RLEStorageType;

    if (count <= (std::numeric_limits<uint8_t>::max() >> 2))
        return UInt8;
    else if (count <= (std::numeric_limits<uint16_t>::max() >> 4))
        return UInt16;
    else if (count <= (std::numeric_limits<uint32_t>::max() >> 8))
        return UInt32;
    else if (count <= (std::numeric_limits<uint64_t>::max() >> 16))
        return UInt64;
    throw std::invalid_argument(
        std::format("Cannot encode count >= {}",
                    std::numeric_limits<uint64_t>::max() >> 16));
}

template <std::unsigned_integral StorageType, std::integral ValueType>
constexpr StorageType FormatNumber(ValueType value) {
    constexpr uint64_t getData = 0b00111111;
    constexpr uint64_t indicator = 0b01000000;
    StorageType result = 0b0;

    for (auto i = 0UZ; i < sizeof(StorageType); i++) {
        result |= ((getData << (i * 6)) & value) << (i * 2);
        result |= indicator << (i * 8);
    }

    return result;
}

template <std::unsigned_integral StorageType>
constexpr std::vector<char> EncodeNumber(char indicator, int64_t dim) {
    std::vector<char> result{};
    result.push_back(indicator);

    const auto formatted = FormatNumber<StorageType>(dim);

    for (auto i = static_cast<int32_t>(sizeof(StorageType)) - 1; i >= 0;
         i -= sizeof(char)) {
        result.push_back(static_cast<char>(formatted >> (i * 8)));
    }
    return result;
}

constexpr std::vector<char> EncodeNumber(std::integral auto dim) {
    const auto storageType = SelectStorageType(dim);
    switch (storageType) {
        using enum RLEStorageType;
    case UInt8:
        return EncodeNumber<uint8_t>('A', dim);
    case UInt16:
        return EncodeNumber<uint16_t>('B', dim);
    case UInt32:
        return EncodeNumber<uint32_t>('C', dim);
    case UInt64:
        return EncodeNumber<uint64_t>('D', dim);
    }
    std::unreachable();
}

template <std::unsigned_integral StorageType>
constexpr StorageType DecodeNumber(std::string_view value) {
    constexpr StorageType getData = 0b00111111;
    auto result = static_cast<StorageType>(0b0);

    for (auto i = 0UZ; i < sizeof(StorageType); i++) {
        const auto strIndex = sizeof(StorageType) - 1 - i;
        result |=
            static_cast<StorageType>((getData & value[strIndex]) << (i * 6));
    }
    return result;
}

constexpr int64_t DecodeNumber(std::string_view value) {
    switch (value[0]) {
    case 'A':
        return static_cast<int64_t>(DecodeNumber<uint8_t>(value.substr(1)));
    case 'B':
        return static_cast<int64_t>(DecodeNumber<uint16_t>(value.substr(1)));
    case 'C':
        return static_cast<int64_t>(DecodeNumber<uint32_t>(value.substr(1)));
    case 'D':
        return static_cast<int64_t>(DecodeNumber<uint64_t>(value.substr(1)));
    }
    std::unreachable();
}

template <std::unsigned_integral StorageType>
inline std::vector<StorageType> EncodeRegion(const GameGrid& grid, Rect region,
                                             Vec2 offset) {
    constexpr static StorageType largestValue =
        std::numeric_limits<StorageType>::max() >> (2 * sizeof(StorageType));

    auto encoded = std::vector<StorageType>{};

    const auto offsetXDim = EncodeNumber<StorageType>(std::abs(offset.X));
    encoded.insert(encoded.end(), offsetXDim.begin(), offsetXDim.end());

    const auto offsetYDim = EncodeNumber<StorageType>(std::abs(offset.Y));
    encoded.insert(encoded.end(), offsetYDim.begin(), offsetYDim.end());

    const auto widthDim = EncodeNumber<StorageType>(region.Width);
    encoded.insert(encoded.end(), widthDim.begin(), widthDim.end());

    const auto heightDim = EncodeNumber<StorageType>(region.Height);
    encoded.insert(encoded.end(), heightDim.begin(), heightDim.end());

    encoded.push_back(FormatNumber<StorageType>('0'));
    encoded.push_back(FormatNumber<StorageType>(offset.X >= 0 ? '0' : '1'));
    encoded.push_back(FormatNumber<StorageType>(offset.Y >= 0 ? '0' : '1'));

    auto runStart = Vec2{region.X, region.Y - 1} - offset;
    bool running = false;
    bool first = true;
    for (auto pos : grid.SortedData()) {
        if (!region.InBounds(pos))
            continue;

        pos -= offset;

        if (!running) {
            auto count = static_cast<StorageType>(
                (region.Height * (pos.X - runStart.X) + pos.Y - runStart.Y) -
                1);
            if (count > largestValue)
                throw std::invalid_argument(
                    "Specified region contains data that is too large");
            if (count > 0)
                encoded.push_back(FormatNumber<StorageType>(count));
            else if (first)
                encoded[4 * sizeof(uint32_t) / sizeof(StorageType)] =
                    FormatNumber<StorageType>('1');
            running = true;
            runStart = pos;
        }

        first = false;
        auto nextPos = Vec2{pos.X, pos.Y + 1};
        if (nextPos.Y >= region.Y + region.Height - offset.Y) {
            nextPos.X++;
            nextPos.Y = region.Y - offset.Y;
        }

        if (!region.InBounds(nextPos + offset) ||
            grid.Data().find(nextPos + offset) == grid.Data().end()) {
            StorageType count = static_cast<StorageType>(
                (region.Height * (pos.X - runStart.X) + pos.Y - runStart.Y) +
                1);
            if (count > largestValue)
                throw std::invalid_argument(
                    "Specified region contains data that is too large");
            encoded.push_back(FormatNumber<StorageType>(count));
            running = false;
            runStart = pos;
        }
    }
    if (!running) {
        Vec2 pos = region.LowerRight() - offset;
        StorageType count = static_cast<StorageType>(
            (region.Height * (pos.X - 1 - runStart.X) + pos.Y - runStart.Y) -
            1);
        if (count > largestValue) {
            ERROR("{} * {} = {} > {}", region.Width, region.Height,
                  region.Width * region.Height, largestValue);
            throw std::invalid_argument(
                "Specified region contains data that is too large");
        }
        encoded.push_back(FormatNumber<StorageType>(count));
    }

    if (encoded.size() > 2) {
        encoded.push_back('\0');
        return encoded;
    }
    return {'\0'};
}

struct DecodeResult {
    GameGrid Grid;
    Vec2 Offset;
};

std::string EncodeRegion(const GameGrid& grid, Rect region,
                         Vec2 offset = {0, 0});

std::expected<RLEEncoder::DecodeResult, std::optional<uint32_t>>
DecodeRegion(std::string_view data, uint32_t warnThreshold);

bool WriteRegion(const GameGrid& grid, Rect region,
                 const std::filesystem::path& filePath, Vec2 offset = {0, 0});

std::expected<DecodeResult, std::string>
ReadRegion(const std::filesystem::path& filePath);
} // namespace gol::RLEEncoder

#endif
