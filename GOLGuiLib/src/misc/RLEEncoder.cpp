#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "RLEEncoder.hpp"

namespace gol::RLEEncoder {
std::string EncodeRegion(const GameGrid& grid, Rect region, Vec2 offset) {
    std::vector<char> result{'g', 'o', 'l', 'd', 'e'};

    const auto appendDim = [&](std::integral auto dim) {
        const auto dimData = EncodeNumber(dim);
        result.insert(result.end(), dimData.begin(), dimData.end());
    };

    const auto beginIndicator = result.size();
    appendDim('0');
    const auto beginIndicatorEnd = result.size();

    appendDim(std::abs(offset.X));
    appendDim(std::abs(offset.Y));
    appendDim(region.Width);
    appendDim(region.Height);

    appendDim(offset.X < 0 ? '1' : '0');
    appendDim(offset.Y < 0 ? '1' : '0');

    auto runStart = Vec2{region.X, region.Y - 1} - offset;
    bool running = false;
    bool first = true;
    for (auto pos : grid.SortedData()) {
        if (!region.InBounds(pos))
            continue;

        pos -= offset;

        if (!running) {
            const auto count =
                (region.Height * (pos.X - runStart.X) + pos.Y - runStart.Y) - 1;
            if (count > 0)
                appendDim(count);
            else if (first) {
                result.erase(result.begin() + beginIndicator,
                             result.begin() + beginIndicatorEnd);
                const auto formattedOne = EncodeNumber('1');
                result.insert(result.begin() + beginIndicator,
                              formattedOne.begin(), formattedOne.end());
            }
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
            const auto count =
                (region.Height * (pos.X - runStart.X) + pos.Y - runStart.Y) + 1;
            appendDim(count);
            running = false;
            runStart = pos;
        }
    }

    if (!running) {
        const auto pos = region.LowerRight() - offset;
        const auto count =
            (region.Height * (pos.X - 1 - runStart.X) + pos.Y - runStart.Y) - 1;
        appendDim(count);
    }

    result.push_back('\0');

    return std::string{result.data()};
}

std::expected<DecodeResult, std::optional<uint32_t>>
DecodeRegion(std::string_view data, uint32_t warnThreshold) {
    if (data[0] == '\0')
        return {};
    if (data.substr(0, 5) != std::string_view{"golde"})
        return std::unexpected{std::nullopt};

    auto pos = 5UZ;
    const auto getNextNumber = [&]() -> std::optional<int64_t> {
        if (pos >= data.size())
            return std::nullopt;

        const auto currentPos = pos;
        switch (data[pos]) {
        case 'A':
            pos += 1 + 1;
            return DecodeNumber(data.substr(currentPos, 1 + 1));
        case 'B':
            pos += 1 + 2;
            return DecodeNumber(data.substr(currentPos, 1 + 2));
        case 'C':
            pos += 1 + 4;
            return DecodeNumber(data.substr(currentPos, 1 + 4));
        case 'D':
            pos += 1 + 8;
            return DecodeNumber(data.substr(currentPos, 1 + 8));
        }
        throw std::invalid_argument{
            std::format("Expected 'A'-'D', Received {}", data[pos])};
    };

    bool running = *getNextNumber() == '1';

    Vec2 offset{static_cast<int32_t>(*getNextNumber()),
                static_cast<int32_t>(*getNextNumber())};

    const auto width = static_cast<int32_t>(*getNextNumber());
    const auto height = static_cast<int32_t>(*getNextNumber());

    if (*getNextNumber() == '1')
        offset.X = -offset.X;
    if (*getNextNumber() == '1')
        offset.Y = -offset.Y;

    auto xPtr = 0LL;
    auto yPtr = 0LL;

    auto warnCount = 0LL;
    GameGrid result{width, height};

    while (const auto count = getNextNumber()) {
        if (running)
            warnCount += *count;

        if (warnCount >= warnThreshold) {
            running = !running;
            continue;
        }

        if (running) {
            for (auto i = 0; i < *count; i++) {
                result.Set(static_cast<int32_t>(xPtr + (yPtr + i) / height),
                           static_cast<int32_t>((yPtr + i) % height), true);
            }
        }

        running = !running;
        xPtr += (yPtr + *count) / height;
        yPtr = (yPtr + *count) % height;
    }

    if (warnCount >= warnThreshold)
        return std::unexpected{static_cast<uint32_t>(warnCount)};

    return DecodeResult{std::move(result), offset};
}

bool WriteRegion(const GameGrid& grid, Rect region,
                 const std::filesystem::path& filePath, Vec2 offset) {
    auto out = std::ofstream{filePath};
    if (!out.is_open())
        return false;

    const auto encodedData = EncodeRegion(grid, region, offset);
    out << encodedData;

    return true;
}

std::expected<DecodeResult, std::string>
ReadRegion(const std::filesystem::path& filePath) {
    auto in = std::ifstream{filePath};
    if (!in.is_open())
        return std::unexpected{"Failed to open file for reading."};

    const std::string data{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};

    const auto decodeResult =
        DecodeRegion(data.c_str(), std::numeric_limits<uint32_t>::max());
    if (!decodeResult) {
        if (decodeResult.error().has_value())
            return std::unexpected{"File contains too many cells."};
        else
            return std::unexpected{"File is not in a valid format."};
    }
    return *decodeResult;
}
} // namespace gol::RLEEncoder
