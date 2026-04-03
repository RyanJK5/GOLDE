#include <expected>
#include <filesystem>
#include <functional>
#include <nfd.hpp>
#include <string>

#include "FileDialog.hpp"

namespace gol {

std::expected<std::filesystem::path, FileDialogFailure>
FileDialog::OpenFileDialog(const std::string& filters,
                           const std::string& defaultPath) {
    const nfdfilteritem_t filterItem[]{{"RLE files", filters.c_str()}};

    NFD::UniquePathU8 outPath{};
    const auto result =
        NFD::OpenDialog(outPath, filters.empty() ? nullptr : filterItem,
                        filters.empty() ? 0 : 1,
                        defaultPath.empty() ? nullptr : defaultPath.c_str());

    if (result == NFD_OKAY) {
        auto ret = std::filesystem::path{outPath.get()};
        const auto extension = ret.extension().string();
        if (extension != ".rle") {
            return std::unexpected<FileDialogFailure>{
                {.Type = FileFailureType::Error,
                 .Message = "Invalid file type selected. Please select a .rle "
                            "file. "}};
        }
        return ret;
    } else if (result == NFD_CANCEL) {
        return std::unexpected<FileDialogFailure>{
            {.Type = FileFailureType::Cancelled}};
    }

    return std::unexpected<FileDialogFailure>{
        {.Type = FileFailureType::Error, .Message = NFD::GetError()}};
}

std::expected<std::filesystem::path, FileDialogFailure>
FileDialog::SaveFileDialog(const std::string& filters,
                           const std::string& defaultPath) {
    const nfdfilteritem_t filterItem[]{{"RLE files", filters.c_str()}};

    NFD::UniquePathU8 outPath{};
    const auto result = NFD::SaveDialog(
        outPath, filters.empty() ? nullptr : filterItem,
        filters.empty() ? 0 : 1,
        defaultPath.empty() ? nullptr : defaultPath.c_str(), nullptr);

    if (result == NFD_OKAY) {
        auto ret = std::filesystem::path{outPath.get()};
        if (ret.extension().empty())
            ret += ".rle";
        else if (ret.extension().string() == ".")
            ret += "rle";
        return ret;
    } else if (result == NFD_CANCEL) {
        return std::unexpected<FileDialogFailure>{
            {.Type = FileFailureType::Cancelled}};
    }

    return std::unexpected<FileDialogFailure>{
        {.Type = FileFailureType::Error, .Message = NFD::GetError()}};
}

std::expected<std::filesystem::path, FileDialogFailure>
FileDialog::SelectFolderDialog(const std::string& defaultPath) {

    NFD::UniquePathU8 outPath{};
    const auto result = NFD::PickFolder(
        outPath, defaultPath.empty() ? nullptr : defaultPath.c_str());

    if (result == NFD_OKAY) {
        auto ret = std::filesystem::path{outPath.get()};
        return ret;
    } else if (result == NFD_CANCEL) {
        return std::unexpected<FileDialogFailure>{
            {.Type = FileFailureType::Cancelled}};
    }

    return std::unexpected<FileDialogFailure>{
        {.Type = FileFailureType::Error, .Message = NFD::GetError()}};
}
} // namespace gol
