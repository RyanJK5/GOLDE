#include <algorithm>
#include <charconv>
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

    std::string out{};

    if (offset.X != 0 || offset.Y != 0)
        out += std::format("#CXRLE Pos = {}, {}\n", offset.X, offset.Y);

    out += std::format("x = {}, y = {}, rule = B3/S23\n", region.Width,
                       region.Height);

    constexpr static auto lineWidth = 70UZ;
    std::string body{};
    auto currentLineWidth = 0UZ;

    const auto appendRun = [&](char tag, int32_t count) {
        const auto token =
            count == 1 ? std::string{tag} : std::format("{}{}", count, tag);
        if (currentLineWidth + token.size() > lineWidth) {
            body += '\n';
            currentLineWidth = 0;
        }
        body += token;
        currentLineWidth += token.size();
    };

    Vec2 prevPos{region.X, region.Y};
    auto aliveRun = 0;

    for (auto pos : grid.SortedData()) {
        if (!region.InBounds(pos))
            continue;

        if (const auto rowGap = pos.Y - prevPos.Y; rowGap > 0) {
            if (aliveRun > 0) {
                appendRun('o', aliveRun);
                aliveRun = 0;
            }
            appendRun('$', rowGap);
            prevPos.X = region.X;
            prevPos.Y = pos.Y;
        }

        if (const auto deadCount = pos.X - prevPos.X; deadCount > 0) {
            if (aliveRun > 0) {
                appendRun('o', aliveRun);
                aliveRun = 0;
            }
            appendRun('b', deadCount);
        }

        aliveRun++;
        prevPos.X = pos.X + 1;
        prevPos.Y = pos.Y;
    }

    if (aliveRun > 0)
        appendRun('o', aliveRun);

    body += '!';

    return out + body + '\n';
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

std::expected<DecodeResult, DecodeError> DecodeRegion(std::string_view src,
                                                      uint32_t warnThreshold) {
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
            if (trimmed.starts_with("CXRLE")) {
                const auto posField = trimmed.find("Pos");

                const auto eq = trimmed.find('=', posField);
                if (eq != std::string_view::npos) {

                    const char* start = trimmed.data() + eq + 1;
                    const char* end = trimmed.data() + trimmed.size();

                    while (start < end && (*start == ' ' || *start == '\t'))
                        ++start;

                    auto pointX = 0;
                    auto pointY = 0;

                    auto [p1, ec1] = std::from_chars(start, end, pointX);

                    if (ec1 == std::errc{}) {

                        while (p1 < end &&
                               (*p1 == ',' || *p1 == ' ' || *p1 == '\t'))
                            ++p1;

                        std::from_chars(p1, end, pointY);

                        explicitOffset = {pointX, pointY};
                        hasExplicitOffset = true;
                    }
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
        if (xEq == std::string::npos || yEq == std::string::npos) {
            return std::unexpected{DecodeError{
                .ErrorType = DecodeError::Type::MissingHeader,
                .Message = "Missing RLE header (x = ..., y = ...)."}};
        }

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

        if (ecW != std::errc{} || ecH != std::errc{}) {
            return std::unexpected{
                DecodeError{.ErrorType = DecodeError::Type::IncorrectHeader,
                            .Message = "Malformed header dimensions."}};
        }
    }

    // 3. Locate the RLE data — everything after the first newline that
    //    follows the header, up to and including '!'.
    const auto headerNewline = rleBody.find('\n');
    if (headerNewline == std::string::npos) {
        return std::unexpected{
            DecodeError{.ErrorType = DecodeError::Type::NoData,
                        .Message = "No RLE data found after header."}};
    }

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

    auto warnCount = uint32_t{0};

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
        case 'b':
            [[fallthrough]]; // dead cells — just advance X
        case '.':
            if (warnCount <= warnThreshold) {
                currentX += count;
            }
            break;
        case 'o':
            [[fallthrough]]; // alive cells
        case 'A':            // some extended RLEs use 'A' for the first state
            warnCount += count;
            if (warnCount < warnThreshold) {
                for (auto i = 0; i < count; ++i)
                    result.Set(currentX + i, currentY, true);
                currentX += count;
            }
            break;
        case '$': // end of row(s)
            if (warnCount < warnThreshold) {
                currentY += count;
                currentX = 0;
            }
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
        return std::unexpected{DecodeError{
            .ErrorType = DecodeError::Type::NoTermination,
            .Message = "RLE data has no terminating exclamation point."}};
    }
    if (warnCount >= warnThreshold) {
        return std::unexpected{DecodeError{
            .ErrorType = DecodeError::Type::TooManyCells,
            .Message =
                std::format(std::locale{""},
                            "Your selection ({:L} cells) is too large\n"
                            "to paste without potential performance issues.\n",
                            warnCount)}};
    }

    const auto offset = hasExplicitOffset ? explicitOffset : Vec2{0, 0};

    return DecodeResult{std::move(result), offset};
}

std::expected<DecodeResult, DecodeError>
ReadRegion(const std::filesystem::path& filePath) {
    auto in = std::ifstream{filePath};
    if (!in.is_open()) {
        return std::unexpected{
            DecodeError{.ErrorType = DecodeError::Type::CantOpenFile,
                        .Message = "Failed to open file for reading."}};
    }

    const std::string data{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};

    return DecodeRegion(data, std::numeric_limits<uint32_t>::max());
}
} // namespace gol::RLEEncoder
