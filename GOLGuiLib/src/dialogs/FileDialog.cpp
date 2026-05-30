#include <algorithm>
#include <expected>
#include <filesystem>
#include <functional>
#include <nfd.hpp>
#include <ranges>
#include <string>

#include <print>
#include <pthread.h>

#include "FileDialog.hpp"

namespace Golde {

std::expected<std::filesystem::path, FileDialogFailure>
FileDialog::OpenFileDialog(std::span<const FilterItem> filters,
                           const std::string& defaultPath) {

    const auto nfdFilters = filters |
                            std::views::transform([](FilterItem item) {
                                return nfdfilteritem_t{.name = item.Identifier,
                                                       .spec = item.Filter};
                            }) |
                            std::ranges::to<std::vector<nfdfilteritem_t>>();

    NFD::UniquePathU8 outPath{};
    const auto result = NFD::OpenDialog(
        outPath, nfdFilters.empty() ? nullptr : nfdFilters.data(),
        static_cast<nfdfiltersize_t>(nfdFilters.size()),
        defaultPath.empty() ? nullptr : defaultPath.c_str());

    if (result == NFD_OKAY) {
        std::println("Success");

        auto ret = std::filesystem::path{outPath.get()};
        const auto extension = ret.extension().string().substr(1);

        const auto filterContainsWord = [&](auto&& filterItem) {
            return std::string_view{filterItem.Filter}.find(extension) !=
                   std::string::npos;
        };

        if (std::ranges::none_of(filters, filterContainsWord)) {
            return std::unexpected<FileDialogFailure>{
                {.Type = FileFailureType::Error,
                 .Message = "Invalid file type selected."}};
        }
        return ret;
    } else if (result == NFD_CANCEL) {
        bool isMainThread = pthread_main_np() != 0;
        std::println("Cancelled: isMainThread = {}", isMainThread);

        return std::unexpected<FileDialogFailure>{
            {.Type = FileFailureType::Cancelled}};
    }

    auto message = NFD::GetError();
    std::println("Error: {}", message);

    return std::unexpected<FileDialogFailure>{
        {.Type = FileFailureType::Error, .Message = message}};
}

std::expected<std::filesystem::path, FileDialogFailure>
FileDialog::SaveFileDialog(std::span<const FilterItem> filters,
                           const std::string& defaultPath) {

    const auto nfdFilters = filters |
                            std::views::transform([](FilterItem item) {
                                return nfdfilteritem_t{.name = item.Identifier,
                                                       .spec = item.Filter};
                            }) |
                            std::ranges::to<std::vector<nfdfilteritem_t>>();
    NFD::UniquePathU8 outPath{};
    const auto result = NFD::SaveDialog(
        outPath, nfdFilters.empty() ? nullptr : nfdFilters.data(),
        static_cast<nfdfiltersize_t>(nfdFilters.size()),
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
} // namespace Golde
