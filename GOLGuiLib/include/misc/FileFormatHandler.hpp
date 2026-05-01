#ifndef FileFormatHandler_hpp_
#define FileFormatHandler_hpp_

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

namespace Golde::FileEncoder {
struct DecodeResult {
    GameGrid Grid;
    Vec2 Offset;
};

enum class FileFormat { RLE, Macrocell };

struct DecodeError {
    enum class Type {
        CantOpenFile,
        MissingHeader,
        IncorrectHeader,
        InvalidRule,
        NoData,
        NoTermination,
        TooManyCells
    };

    Type ErrorType;
    std::string Message;
};

bool IsFormatSupported(std::string_view format);

std::string EncodeRegion(const GameGrid& grid, Rect region,
                         Vec2 offset = {0, 0},
                         FileFormat fileFormat = FileFormat::RLE);

std::expected<DecodeResult, DecodeError>
DecodeRegion(std::string_view data, uint32_t warnThreshold,
             FileFormat fileFormat = FileFormat::RLE);

bool WriteRegion(const GameGrid& grid, Rect region,
                 const std::filesystem::path& filePath, Vec2 offset = {0, 0});

std::expected<DecodeResult, DecodeError>
ReadRegion(const std::filesystem::path& filePath);
} // namespace Golde::FileEncoder

#endif
