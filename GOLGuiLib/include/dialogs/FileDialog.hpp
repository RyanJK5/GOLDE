#ifndef FileDialog_hpp_
#define FileDialog_hpp_

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace Golde {
enum class FileFailureType { Cancelled, Error };

struct FileDialogFailure {
    FileFailureType Type;
    std::string Message{};
};

struct FilterItem {
    const char* Identifier;
    const char* Filter;
};

namespace FileDialog {
std::expected<std::filesystem::path, FileDialogFailure>
OpenFileDialog(std::span<const FilterItem> filters,
               const std::string& defaultPath);

std::expected<std::filesystem::path, FileDialogFailure>
SaveFileDialog(std::span<const FilterItem> filters,
               const std::string& defaultPath);

std::expected<std::filesystem::path, FileDialogFailure>
SelectFolderDialog(const std::string& defaultPath);
}; // namespace FileDialog
} // namespace Golde

#endif
