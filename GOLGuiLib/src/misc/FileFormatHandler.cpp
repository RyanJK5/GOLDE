#include <algorithm>
#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "FileFormatHandler.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"

namespace gol::FileEncoder {
static std::string EncodeRLE(const GameGrid& grid, Rect region, Vec2 offset) {
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

namespace {
// Encodes a level-3 node (8x8) into the macrocell leaf line format.
// Each uint16_t quadrant is row-major, MSB = top-left of that 4x4 quadrant.
// The four quadrants tile the 8x8 grid: NW|NE / SW|SE.
std::string EncodeLeafNode(const LifeNode* node) {
    const auto [nw, ne, sw, se] = EncodeLevel3(node);

    // Reconstruct the full 8x8 grid as 8 bytes, one per row.
    // Each quadrant contributes 4 rows of 4 bits.
    // NW -> top 4 bits of rows 0-3, NE -> bottom 4 bits of rows 0-3
    // SW -> top 4 bits of rows 4-7, SE -> bottom 4 bits of rows 4-7
    std::array<uint8_t, 8> rows{};
    for (int r = 0; r < 4; ++r) {
        // NW: bits 15-12 are row 0, 11-8 are row 1, etc.
        uint8_t nwRow = static_cast<uint8_t>((nw >> (12 - r * 4)) & 0xF);
        // NE: bits 15-12 are row 0, etc. — placed in the low nibble
        uint8_t neRow = static_cast<uint8_t>((ne >> (12 - r * 4)) & 0xF);
        rows[r] = (nwRow << 4) | neRow;
        uint8_t swRow = static_cast<uint8_t>((sw >> (12 - r * 4)) & 0xF);
        uint8_t seRow = static_cast<uint8_t>((se >> (12 - r * 4)) & 0xF);
        rows[r + 4] = (swRow << 4) | seRow;
    }

    std::string result;
    result.reserve(72); // Upper bound: 8 cells + '$' per row * 8 rows

    for (int r = 0; r < 8; ++r) {
        uint8_t row = rows[r];
        // Find the last live cell in this row to suppress trailing dead cells.
        int lastLive = -1;
        for (int c = 0; c < 8; ++c) {
            if ((row >> (7 - c)) & 1)
                lastLive = c;
        }
        for (int c = 0; c <= lastLive; ++c) {
            result += ((row >> (7 - c)) & 1) ? '*' : '.';
        }
        result += '$';
    }

    // Suppress trailing '$' sequences (empty rows at end of grid).
    // The spec suppresses empty cells at end of rows; by extension a macrocell
    // file typically also omits trailing empty rows, matching Golly's output.
    while (result.size() >= 2 && result.back() == '$' &&
           result[result.size() - 2] == '$') {
        result.pop_back();
    }

    return result;
}

// Recursive worker. Populates `nodeIndex` (pointer -> 1-based file order)
// and appends lines to `out`.
void EncodeNode(
    const LifeNode* node, int32_t level,
    ankerl::unordered_dense::map<const LifeNode*, int32_t, LifeNodeHash,
                                 LifeNodeEqual>& nodeIndex,
    std::string& out) {
    // Node 0 (empty) is implicit; never emit or recurse into empty nodes.
    if (node == FalseNode || node->IsEmpty)
        return;

    // Already emitted — nothing to do.
    if (nodeIndex.contains(node))
        return;

    if (level == 3) {
        // Leaf node: emit the 8x8 RLE-like grid line.
        out += EncodeLeafNode(node);
        out += '\n';
    } else {
        // Recurse children first (child-first ordering required by spec).
        const int32_t childLevel = level - 1;
        EncodeNode(node->NorthWest, childLevel, nodeIndex, out);
        EncodeNode(node->NorthEast, childLevel, nodeIndex, out);
        EncodeNode(node->SouthWest, childLevel, nodeIndex, out);
        EncodeNode(node->SouthEast, childLevel, nodeIndex, out);

        // Resolve each child to its node number (0 if empty/null).
        auto resolveIndex = [&](const LifeNode* child) -> int32_t {
            if (child == FalseNode || child->IsEmpty)
                return 0;
            return nodeIndex.at(child);
        };

        out += std::format(
            "{} {} {} {} {}\n", level, resolveIndex(node->NorthWest),
            resolveIndex(node->NorthEast), resolveIndex(node->SouthWest),
            resolveIndex(node->SouthEast));
    }

    // Register this node with the next available 1-based index.
    nodeIndex.emplace(node, static_cast<int32_t>(nodeIndex.size()) + 1);
}

} // namespace

std::string EncodeMacrocell(const LifeNode* node, int32_t level) {
    std::string out{};
    // Reserve a modest initial buffer; will grow as needed.
    out.reserve(1024);

    // Header
    out += "[M2]\n";
    out += "#R B3/S23\n";

    // Tree: child-first traversal
    ankerl::unordered_dense::map<const LifeNode*, int32_t, LifeNodeHash,
                                 LifeNodeEqual>
        nodeIndex;
    EncodeNode(node, level, nodeIndex, out);

    return out;
}

std::string EncodeRegion(const GameGrid& grid, Rect region, Vec2 offset,
                         FileFormat fileFormat) {
    switch (fileFormat) {
    case FileFormat::RLE:
        return EncodeRLE(grid, region, offset);
    case FileFormat::Macrocell:
        return EncodeMacrocell(grid.Data().Data(),
                               grid.Data().CalculateDepth());
    default:
        throw std::logic_error{"Unsupported file format"};
    }
}

static FileFormat ParseFileExtension(const std::filesystem::path& extension) {
    const auto str = extension.string();
    if (str == ".rle") {
        return FileFormat::RLE;
    } else if (str == ".mc") {
        return FileFormat::Macrocell;
    } else {
        throw std::logic_error{"Unkown file extension"};
    }
}

bool WriteRegion(const GameGrid& grid, Rect region,
                 const std::filesystem::path& filePath, Vec2 offset) {
    auto out = std::ofstream{filePath};
    if (!out.is_open())
        return false;

    const auto fileFormat = ParseFileExtension(filePath.extension());
    const auto encodedData = EncodeRegion(grid, region, offset, fileFormat);
    out << encodedData;

    return true;
}

static std::expected<DecodeResult, DecodeError>
DecodeRLE(std::string_view src, uint32_t warnThreshold) {
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
            if (trimmed.starts_with("#CXRLE")) {
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

namespace {
std::vector<std::string_view> SplitLines(std::string_view text) {
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        auto pos = text.find('\n');
        lines.push_back(text.substr(0, pos));
        text.remove_prefix(pos == std::string_view::npos ? text.size()
                                                         : pos + 1);
    }
    return lines;
}

std::expected<const LifeNode*, DecodeError>
DecodeLeafNode(std::string_view line, const HashQuadtree& qt) {
    using E = DecodeError::Type;

    std::array<std::array<bool, 8>, 8> grid{};
    int row = 0, col = 0;
    for (char c : line) {
        if (row >= 8)
            return std::unexpected(
                DecodeError{E::TooManyCells, "Leaf node has more than 8 rows"});
        switch (c) {
        case '.':
            if (col >= 8)
                return std::unexpected(DecodeError{
                    E::TooManyCells, "Leaf node row exceeds 8 columns"});
            grid[row][col++] = false;
            break;
        case '*':
            if (col >= 8)
                return std::unexpected(DecodeError{
                    E::TooManyCells, "Leaf node row exceeds 8 columns"});
            grid[row][col++] = true;
            break;
        case '$':
            ++row;
            col = 0;
            break;
        default:
            return std::unexpected(DecodeError{
                E::IncorrectHeader,
                std::format("Unexpected character '{}' in leaf node", c)});
        }
    }

    auto makeLevel1 = [&](int r, int c) -> const LifeNode* {
        return qt.FindOrCreate(grid[r][c] ? TrueNode : FalseNode,
                               grid[r][c + 1] ? TrueNode : FalseNode,
                               grid[r + 1][c] ? TrueNode : FalseNode,
                               grid[r + 1][c + 1] ? TrueNode : FalseNode);
    };

    auto makeLevel2 = [&](int r, int c) -> const LifeNode* {
        return qt.FindOrCreate(makeLevel1(r, c), makeLevel1(r, c + 2),
                               makeLevel1(r + 2, c), makeLevel1(r + 2, c + 2));
    };

    return qt.FindOrCreate(makeLevel2(0, 0), makeLevel2(0, 4), makeLevel2(4, 0),
                           makeLevel2(4, 4));
}

std::expected<DecodeResult, DecodeError>
DecodeMacrocell(std::string_view fileContents) {
    auto lines = SplitLines(fileContents);
    if (lines.empty())
        return std::unexpected{
            DecodeError{DecodeError::Type::MissingHeader, "File is empty"}};
    if (!lines[0].starts_with("[M2]"))
        return std::unexpected{DecodeError{
            DecodeError::Type::IncorrectHeader,
            std::format("Expected [M2] header, got '{}'", lines[0])}};

    HashQuadtree qt({}, {0, 0});

    // ---- Header parsing ----
    size_t lineIdx = 1;
    for (; lineIdx < lines.size(); ++lineIdx) {
        auto line = lines[lineIdx];
        if (line.empty())
            continue;
        if (!line.starts_with('#'))
            break;
        // #R and #G recognised but not acted on for now.
    }

    if (lineIdx >= lines.size())
        return std::unexpected{
            DecodeError{DecodeError::Type::NoData,
                        "File contains a header but no tree data"}};

    // ---- Tree parsing ----
    std::vector<const LifeNode*> nodeTable;

    const auto resolveNode =
        [&](int32_t num,
            int32_t childLevel) -> std::expected<const LifeNode*, DecodeError> {
        if (num == 0)
            return qt.EmptyTree(childLevel);
        if (num < 0 || static_cast<size_t>(num) > nodeTable.size())
            return std::unexpected{DecodeError{
                DecodeError::Type::NoTermination,
                std::format("Reference to undefined node {}", num)}};
        return nodeTable[num - 1];
    };

    auto rootLevel = -1;
    const auto* root = FalseNode;

    for (; lineIdx < lines.size(); ++lineIdx) {
        auto line = lines[lineIdx];
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.empty())
            continue;

        const char first = line[0];

        if (first == '.' || first == '*' || first == '$') {
            auto result = DecodeLeafNode(line, qt);
            if (!result)
                return std::unexpected{std::move(result.error())};
            nodeTable.push_back(*result);
            rootLevel = 3;
            root = *result;
        } else if (first >= '0' && first <= '9') {
            int32_t level, nwNum, neNum, swNum, seNum;

            auto parse = [&](std::string_view sv, int32_t& out)
                -> std::expected<std::string_view, DecodeError> {
                sv = sv.substr(sv.find_first_not_of(' '));
                auto [ptr, ec] =
                    std::from_chars(sv.data(), sv.data() + sv.size(), out);
                if (ec != std::errc{})
                    return std::unexpected{DecodeError{
                        DecodeError::Type::IncorrectHeader,
                        std::format("Failed to parse integer from '{}'", sv)}};
                return sv.substr(ptr - sv.data());
            };

            auto rem = line;
            if (auto r = parse(rem, level))
                rem = *r;
            else
                return std::unexpected{r.error()};
            if (auto r = parse(rem, nwNum))
                rem = *r;
            else
                return std::unexpected{r.error()};
            if (auto r = parse(rem, neNum))
                rem = *r;
            else
                return std::unexpected{r.error()};
            if (auto r = parse(rem, swNum))
                rem = *r;
            else
                return std::unexpected{r.error()};
            if (auto r = parse(rem, seNum))
                rem = *r;
            else
                return std::unexpected{r.error()};

            if (level < 4)
                return std::unexpected{DecodeError{
                    DecodeError::Type::IncorrectHeader,
                    std::format(
                        "Non-leaf node has invalid level {} (must be >= 4)",
                        level)}};

            const int32_t childLevel = level - 1;
            const auto nw = resolveNode(nwNum, childLevel);
            if (!nw)
                return std::unexpected{nw.error()};
            const auto ne = resolveNode(neNum, childLevel);
            if (!ne)
                return std::unexpected{ne.error()};
            const auto sw = resolveNode(swNum, childLevel);
            if (!sw)
                return std::unexpected{sw.error()};
            const auto se = resolveNode(seNum, childLevel);
            if (!se)
                return std::unexpected{se.error()};

            const auto* node = qt.FindOrCreate(*nw, *ne, *sw, *se);
            nodeTable.push_back(node);
            rootLevel = level;
            root = node;
        } else {
            return std::unexpected{
                DecodeError{DecodeError::Type::IncorrectHeader,
                            std::format("Unexpected line: '{}'", line)}};
        }
    }

    if (root == FalseNode || rootLevel < 0)
        return std::unexpected{
            DecodeError{DecodeError::Type::NoData, "File contained no nodes"}};

    qt.OverwriteData(root, rootLevel);
    const auto boundingBox = qt.FindBoundingBox();
    qt.OverwriteData(root, rootLevel, -boundingBox.Pos());
    return DecodeResult{GameGrid{std::move(qt), boundingBox.Size()}, Vec2{}};
}
} // namespace

std::expected<DecodeResult, DecodeError> DecodeRegion(std::string_view src,
                                                      uint32_t warnThreshold,
                                                      FileFormat fileFormat) {
    switch (fileFormat) {
    case FileFormat::RLE:
        return DecodeRLE(src, warnThreshold);
    case FileFormat::Macrocell:
        return DecodeMacrocell(src);
    default:
        throw std::logic_error{"Unsupported file format"};
    }
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

    return DecodeRegion(data, std::numeric_limits<uint32_t>::max(),
                        ParseFileExtension(filePath.extension()));
}
} // namespace gol::FileEncoder
