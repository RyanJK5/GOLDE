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
    if (data.substr(0, 5) != std::string_view{"golde"}) {
        return std::unexpected{std::nullopt};
    }

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

static std::expected<RLEEncoder::DecodeResult, std::string>
LoadGollyRLE(const std::string& src) {
    // 1. Strip comment lines (#C, #c, #N, #O, #R, #P, #r ...).
    Vec2 explicitOffset{0, 0};
    bool hasExplicitOffset = false;

    std::string rleBody{};
    rleBody.reserve(src.size());

    auto lineStart = 0UZ;
    while (lineStart < src.size()) {
        const auto lineEnd = src.find('\n', lineStart);
        const auto lineLen =
            (lineEnd == std::string::npos ? src.size() : lineEnd) - lineStart;
        const std::string_view line{src.data() + lineStart, lineLen};

        // Trim leading whitespace
        const auto firstNonSpace = line.find_first_not_of(" \t\r");
        const auto trimmed = (firstNonSpace == std::string_view::npos)
                                             ? std::string_view{}
                                             : line.substr(firstNonSpace);

        if (!trimmed.empty() && trimmed[0] == '#') {
            // #P x y  or  #R x y — explicit pattern origin
            if (trimmed.size() >= 2 &&
                (trimmed[1] == 'P' || trimmed[1] == 'R')) {
                const auto coords = trimmed.substr(2);
                auto pointX = 0; 
                auto pointY = 0;
                
                auto [p1, ec1] = std::from_chars(
                    coords.data(), coords.data() + coords.size(), pointX);
                if (ec1 == std::errc{}) {
                    // skip whitespace between coords
                    while (p1 < coords.data() + coords.size() &&
                           (*p1 == ' ' || *p1 == '\t'))
                        ++p1;
                    std::from_chars(p1, coords.data() + coords.size(), pointY);
                    explicitOffset = {pointX, pointY};
                    hasExplicitOffset = true;
                }
            }
        } else {
            rleBody += line;
            rleBody += '\n';
        }

        lineStart = (lineEnd == std::string::npos) ? src.size() : lineEnd + 1;
    }

    // 2. Parse the header line:  x = W, y = H[, rule = ...]
    int32_t patternWidth = 0, patternHeight = 0;
    {
        const auto xEq = rleBody.find("x =");
        const auto yEq = rleBody.find("y =");
        if (xEq == std::string::npos || yEq == std::string::npos)
            return std::unexpected{"Missing RLE header (x = ..., y = ...)."};

        const char* xPtr = rleBody.data() + xEq + 3;
        const char* yPtr = rleBody.data() + yEq + 3;
        while (*xPtr == ' ')
            ++xPtr;
        while (*yPtr == ' ')
            ++yPtr;

        const auto [pW, ecW] = std::from_chars(
            xPtr, rleBody.data() + rleBody.size(), patternWidth);
        const auto [pH, ecH] = std::from_chars(
            yPtr, rleBody.data() + rleBody.size(), patternHeight);

        if (ecW != std::errc{} || ecH != std::errc{})
            return std::unexpected{"Malformed header dimensions."};
    }

    // 3. Locate the RLE data — everything after the first newline that
    //    follows the header, up to and including '!'.
    const auto headerNewline = rleBody.find('\n');
    if (headerNewline == std::string::npos)
        return std::unexpected{"No RLE data found after header."};

    const std::string_view rleData{rleBody.data() + headerNewline + 1,
                                   rleBody.size() - headerNewline - 1};

    // 4. Decode the Golly RLE into a set of live cell coordinates.
    //    Golly RLE:  <count><tag>
    //      b = dead cell(s)   o = alive cell(s)   $ = end of row   ! = end
    //    Rows advance in Y; within a row cells advance in X.
    //    (Row-major, top-left origin.)
    GameGrid result{patternWidth, patternHeight};

    auto currentX = 0;
    auto currentY = 0;
    auto run = 0; // 0 means "1" (default when no count prefix)
    bool ended = false;

    for (const auto ch : rleData) {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            continue;

        if (ch >= '0' && ch <= '9') {
            run = run * 10 + (ch - '0');
            continue;
        }

        const auto count = (run == 0) ? 1 : run;
        run = 0;

        switch (ch) {
        case 'b': [[fallthrough]]; // dead cells — just advance X
        case '.':
            currentX += count;
            break;

        case 'o': [[fallthrough]]; // alive cells
        case 'A': // some extended RLEs use 'A' for the first state
            for (auto i = 0; i < count; ++i)
                result.Set(currentX + i, currentY, true);
            currentX += count;
            break;

        case '$': // end of row(s)
            currentY += count;
            currentX = 0;
            break;

        case '!': // end of pattern
            ended = true;
            break;

        default:
            // Multi-state RLE uses uppercase letters for states 2+;
            // treat any unrecognised uppercase as alive for plain Life.
            if (ch >= 'A' && ch <= 'Z') {
                for (int32_t i = 0; i < count; ++i)
                    result.Set(currentX + i, currentY, true);
                currentX += count;
            }
            // Unknown characters silently ignored (comments can bleed in)
            break;
        }

        if (ended) {
            break;
        }
    }

    if (!ended) {
        return std::unexpected{"RLE data has no terminating '!'."};
    }

    const auto offset = hasExplicitOffset ? explicitOffset : Vec2{0, 0};

    return RLEEncoder::DecodeResult{std::move(result), offset};
}

std::expected<DecodeResult, std::string>
ReadRegion(const std::filesystem::path& filePath) {

    auto in = std::ifstream{filePath};
    if (!in.is_open()) {
        return std::unexpected{"Failed to open file for reading."};
    }

    const std::string data{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};

    if (filePath.extension().string() == ".rle") {
        return LoadGollyRLE(data);
    }

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
