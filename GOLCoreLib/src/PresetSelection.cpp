#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <font-awesome/IconsFontAwesome7.h>
#include <format>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <string>
#include <utility>
#include <vector>

#include "DisabledScope.hpp"
#include "EditorResult.hpp"
#include "FileDialog.hpp"
#include "FileFormatHandler.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "LoadingSpinner.hpp"
#include "Logging.hpp"
#include "PresetSelection.hpp"
#include "PresetSelectionResult.hpp"

namespace Golde {
static bool ContainsIgnoreCase(std::string_view string,
                               std::string_view substring) {
    return std::ranges::search(string, substring, [](char left, char right) {
               return std::tolower(left) == std::tolower(right);
           }).begin() != string.end();
}

PresetDisplay::PresetDisplay(LoadedPreset&& preset, Size2 windowSize)
    : Grid(std::move(preset.Grid)), FileName(std::move(preset.FileName)),
      RelativeFolder(std::move(preset.NormalizedFolder)),
      Graphics(std::filesystem::path("resources") / "shader", windowSize.Width,
               windowSize.Height, Color{}) {}

PresetSelection::PresetSelection(const std::filesystem::path& defaultPath,
                                 Size2 windowSize)
    : m_DefaultPath(defaultPath), m_WindowSize(windowSize) {
    ReadFiles(m_DefaultPath);
}

std::string PresetSelection::CurrentFolderName() const {
    if (m_CurrentPath.empty()) {
        return m_DefaultPath.filename().generic_string();
    }
    return m_CurrentPath.filename().generic_string();
}

PresetSelectionResult PresetSelection::Update(const EditorResult& info) {
    DrainLoadQueue();

    ImGui::Begin("Patterns", nullptr, ImGuiWindowFlags_NoNav);

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 10.f);
    ImGui::SetNextItemWidth(400.f);
    ImGui::InputTextWithHint("##search", "Search...", &m_SearchText);
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FOLDER_OPEN)) {
        if (const auto result = FileDialog::SelectFolderDialog(
                m_DefaultPath.generic_string())) {
            m_DefaultPath = *result;
            ReadFiles(*result);
        }
    }
    ImGui::SetItemTooltip("Select Folder");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE)) {
        ReadFiles(m_DefaultPath);
    }
    ImGui::SetItemTooltip("Refresh Presets");

    const auto isLoading = !m_FinishedReading.load(std::memory_order_acquire);

    if (isLoading) {
        const auto spinnerRadius = ImGui::GetFontSize() * 0.7f;
        const auto availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (availWidth - spinnerRadius * 2.f) * 0.5f);
        LoadingSpinner("##loading", spinnerRadius, ImGui::GetFontSize() * 0.3f,
                       ImGui::GetColorU32(ImGuiCol_Text));
    }

    ImGui::PopStyleVar();

    if (!m_DirectoryContents.contains(m_CurrentPath)) {
        m_CurrentPath.clear();
    }

    const auto spacing = ImGui::GetStyle().ItemSpacing;
    const auto layoutWidth = std::max(1.f, ImGui::GetContentRegionAvail().x);
    const auto idealWidth = ImGui::GetFontSize() * 10.f;
    const auto numPerRow =
        std::max(1, static_cast<int>((layoutWidth + spacing.x) /
                                     (idealWidth + spacing.x)));
    const auto templateWidth =
        std::max(10.f, (layoutWidth - spacing.x * (numPerRow - 1)) / numPerRow);
    const auto folderTabHeight =
        std::max(ImGui::GetFrameHeight() * 1.6f, ImGui::GetFontSize() * 1.9f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{6.f, 2.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{6.f, 4.f});

    auto breadcrumbPath = std::filesystem::path{};
    auto drawFolderButton = [&](const std::filesystem::path& folderPath,
                                const std::string& label, bool current,
                                float buttonWidth) {
        ImGui::PushID(folderPath.generic_string().c_str());

        const auto buttonLabel = std::format("{} {}", ICON_FA_FOLDER, label);
        const auto buttonHeight = std::max(ImGui::GetFrameHeight() * 1.6f,
                                           ImGui::GetFontSize() * 1.9f);

        if (current) {
            const auto cursorScreenPos = ImGui::GetCursorScreenPos();

            ImGui::Dummy({buttonWidth, buttonHeight});

            const auto textSize = ImGui::CalcTextSize(buttonLabel.c_str());

            const ImVec2 textPos{
                cursorScreenPos.x + ImGui::GetStyle().FramePadding.x,
                cursorScreenPos.y +
                    std::max(0.f, (buttonHeight - textSize.y) * 0.5f)};

            ImGui::GetWindowDrawList()->AddText(
                textPos, ImGui::GetColorU32(ImGuiCol_Text),
                buttonLabel.c_str());
        } else {
            ImGui::Button(buttonLabel.c_str(), {buttonWidth, buttonHeight});
        }
        if (ImGui::IsItemHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            m_CurrentPath = folderPath;
        }

        const auto tooltip = folderPath.empty() ? m_DefaultPath.generic_string()
                                                : folderPath.generic_string();
        ImGui::SetItemTooltip("%s", tooltip.c_str());

        ImGui::PopID();
    };

    drawFolderButton({}, m_DefaultPath.filename().generic_string(),
                     m_CurrentPath.empty(), templateWidth);
    const auto currentPathCopy =
        m_CurrentPath; // Iterate over a copy to avoid invalidation
    for (const auto& component : currentPathCopy) {
        if (m_CurrentPath != currentPathCopy)
            break; // Path changed, stop rendering
        breadcrumbPath /= component;
        ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x * 1.5f);
        const auto arrowHeight = ImGui::CalcTextSize(ICON_FA_ANGLE_RIGHT).y;
        const auto arrowOffset =
            std::max(0.f, (folderTabHeight - arrowHeight) * 0.5f);
        const auto arrowCursorY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(arrowCursorY + arrowOffset);
        ImGui::TextUnformatted(ICON_FA_ANGLE_RIGHT);
        ImGui::SetCursorPosY(arrowCursorY);
        ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x * 1.5f);
        drawFolderButton(breadcrumbPath, component.generic_string(),
                         breadcrumbPath == m_CurrentPath, templateWidth);
    }

    ImGui::Dummy(ImVec2{0.f, ImGui::GetStyle().ItemSpacing.y * 2.f});
    const auto windowBounds = RectF{Vec2F{ImGui::GetCursorPos()},
                                    Size2F{templateWidth, templateWidth}};

    auto childFolders = std::vector<std::filesystem::path>{};
    auto visibleFiles = std::vector<size_t>{};

    if (const auto contents = m_DirectoryContents.find(m_CurrentPath);
        contents != m_DirectoryContents.end()) {
        childFolders.assign(contents->second.ChildFolders.begin(),
                            contents->second.ChildFolders.end());
        visibleFiles = contents->second.Files;
    }

    const auto isInSubtree = [](const std::filesystem::path& folder,
                                const std::filesystem::path& root) {
        if (root.empty())
            return true;

        auto folderIt = folder.begin();
        auto rootIt = root.begin();
        while (rootIt != root.end()) {
            if (folderIt == folder.end() || *folderIt != *rootIt) {
                return false;
            }
            ++folderIt;
            ++rootIt;
        }
        return true;
    };

    const auto matchesSearch = [&](const PresetDisplay& preset) {
        if (m_SearchText.empty())
            return true;
        return ContainsIgnoreCase(preset.FileName, m_SearchText) ||
               ContainsIgnoreCase(preset.RelativeFolder.generic_string(),
                                  m_SearchText);
    };

    if (!m_SearchText.empty()) {
        visibleFiles.clear();
        for (const auto& [folder, contents] : m_DirectoryContents) {
            if (!isInSubtree(folder, m_CurrentPath))
                continue;
            visibleFiles.insert(visibleFiles.end(), contents.Files.begin(),
                                contents.Files.end());
        }
    }

    const auto folderHasSearchHit =
        [&](const std::filesystem::path& folderPath) {
            if (ContainsIgnoreCase(folderPath.filename().generic_string(),
                                   m_SearchText))
                return true;

            for (const auto& [folder, contents] : m_DirectoryContents) {
                if (!isInSubtree(folder, folderPath))
                    continue;

                if (folder != folderPath &&
                    ContainsIgnoreCase(folder.filename().generic_string(),
                                       m_SearchText)) {
                    return true;
                }

                for (const auto fileIndex : contents.Files) {
                    if (matchesSearch(m_Library[fileIndex])) {
                        return true;
                    }
                }
            }
            return false;
        };

    std::erase_if(childFolders, [&](const auto& folder) {
        return !m_SearchText.empty() && !folderHasSearchHit(folder);
    });

    std::sort(childFolders.begin(), childFolders.end(),
              [](const auto& left, const auto& right) {
                  return left.filename().generic_string() <
                         right.filename().generic_string();
              });

    const auto renderWrappedFolders =
        [&](const std::vector<std::filesystem::path>& folders) {
            if (folders.empty())
                return;

            float rowWidth = 0.f;
            bool first = true;
            for (const auto& folder : folders) {
                const auto tabLabel =
                    std::format("{} {}", ICON_FA_FOLDER,
                                folder.filename().generic_string());

                if (!first) {
                    if (rowWidth + templateWidth > layoutWidth) {
                        rowWidth = 0.f;
                    } else {
                        ImGui::SameLine();
                    }
                }

                ImGui::PushID(folder.generic_string().c_str());
                ImGui::Button(tabLabel.c_str(),
                              {templateWidth, folderTabHeight});
                const bool openFolder =
                    ImGui::IsItemHovered(
                        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                if (openFolder) {
                    m_CurrentPath = folder;
                }
                ImGui::SetItemTooltip("%s", folder.generic_string().c_str());
                ImGui::PopID();
                if (openFolder)
                    break;

                rowWidth += templateWidth + spacing.x;
                first = false;
            }
        };

    renderWrappedFolders(childFolders);
    ImGui::Dummy(ImVec2{0.f, ImGui::GetStyle().ItemSpacing.y * 2.f});

    if (windowBounds != m_LastWindowBounds) {
        for (auto& preset : m_Library) {
            RedrawPreset(preset, windowBounds, false);
        }
    }
    m_LastWindowBounds = windowBounds;

    std::erase_if(visibleFiles, [&](size_t index) {
        return !matchesSearch(m_Library[index]);
    });

    std::string retString{};
    auto numAvailable = 0UZ;

    {
        DisabledScope disableIf{!Actions::Editable(info.Simulation.State)};
        for (const auto index : visibleFiles) {
            auto& preset = m_Library[index];

            if (numAvailable % numPerRow != 0)
                ImGui::SameLine();
            const auto cursorPos = ImGui::GetCursorPos();

            ImGui::Image(static_cast<ImTextureID>(preset.Graphics.TextureID()),
                         {windowBounds.Width, windowBounds.Height},
                         ImVec2{0, 1}, ImVec2{1, 0});

            const auto tooltipPath =
                preset.RelativeFolder.empty()
                    ? preset.FileName
                    : std::format("{}/{}",
                                  preset.RelativeFolder.generic_string(),
                                  preset.FileName);
            ImGui::SetItemTooltip("%s", tooltipPath.c_str());

            ImGui::SetCursorPos(cursorPos);

            const auto itemId = std::format("##{}", tooltipPath);
            if (ImGui::InvisibleButton(itemId.c_str(), {windowBounds.Width,
                                                        windowBounds.Height})) {
                retString = FileEncoder::EncodeRegion(
                                preset.Grid, {{0, 0}, preset.Grid.Size()})
                                .c_str();
            }

            const bool isHovered = ImGui::IsItemHovered();
            if (isHovered || preset.WasHovered) {
                preset.WasHovered = isHovered;
                RedrawPreset(preset, windowBounds, isHovered);
            }

            numAvailable++;
        }
    }

    if (visibleFiles.empty()) {
        ImGui::TextDisabled(m_SearchText.empty()
                                ? "No patterns in this folder."
                                : "No matching patterns in this folder tree.");
    }

    ImGui::PopStyleVar(2);
    ImGui::End();
    return {.ClipboardText = retString};
}

void PresetSelection::RedrawPreset(PresetDisplay& preset, RectF windowBounds,
                                   bool hovered) {
    const auto cellSize =
        std::min({10.f, windowBounds.Width / preset.Grid.Width(),
                  windowBounds.Height / preset.Grid.Height()});

    GraphicsHandlerArgs graphicsArgs{.ViewportBounds = windowBounds,
                                     .GridSize = preset.Grid.Size(),
                                     .CellSize = {cellSize, cellSize},
                                     .ShowGridLines = false};

    preset.Graphics.RescaleFrameBuffer(windowBounds, windowBounds);
    preset.Graphics.CenterCamera(graphicsArgs);
    preset.Graphics.ClearBackground(graphicsArgs);
    preset.Graphics.DrawGrid(Vec2{}, preset.Grid.Data(), graphicsArgs);

    if (hovered) {
        preset.Graphics.DrawSelection({{0, 0}, graphicsArgs.GridSize},
                                      graphicsArgs);
    }
}

void PresetSelection::ReadFiles(const std::filesystem::path& path) {
    m_MaxGridDimensions = Size2F{};
    m_Library.clear();
    m_DirectoryContents.clear();
    m_CurrentPath.clear();
    m_DirectoryContents[m_CurrentPath];
    m_FinishedReading.store(false, std::memory_order_relaxed);

    m_LoadThread = std::jthread{[this, path](std::stop_token stopToken) {
        for (const auto& file :
             std::filesystem::recursive_directory_iterator(path)) {
            if (stopToken.stop_requested()) {
                break;
            }

            if (!FileEncoder::IsFormatSupported(
                    file.path().extension().generic_string())) {
                continue;
            }

            auto result = FileEncoder::ReadRegion(m_Cache, file.path());
            if (!result) {
                ERROR("Failed to read file {}: {}",
                      file.path().filename().generic_string(),
                      result.error().Message);
                continue;
            }

            auto relativeFolder =
                file.path().parent_path().lexically_relative(path);

            auto normalizedFolder = relativeFolder == "."
                                        ? std::filesystem::path{}
                                        : std::move(relativeFolder);
            auto fileName = file.path().filename().generic_string();

            std::scoped_lock lock{m_QueueMutex};
            m_LoadQueue.emplace(std::move(result->Grid), std::move(fileName),
                                std::move(normalizedFolder));
        }
        m_FinishedReading.store(true, std::memory_order_release);
    }};
}

void PresetSelection::DrainLoadQueue() {
    const auto registerFolderHierarchy =
        [&](const std::filesystem::path& relativeFolder) {
            auto parent = std::filesystem::path{};
            for (const auto& component : relativeFolder) {
                auto child = parent / component;
                m_DirectoryContents[parent].ChildFolders.insert(child);
                parent = child;
            }
            m_DirectoryContents.try_emplace(parent);
        };

    std::scoped_lock lock{m_QueueMutex};

    while (!m_LoadQueue.empty()) {
        auto loaded = std::move(m_LoadQueue.front());
        m_LoadQueue.pop();

        m_MaxGridDimensions.Width = std::max(
            m_MaxGridDimensions.Width, static_cast<float>(loaded.Grid.Width()));
        m_MaxGridDimensions.Height =
            std::max(m_MaxGridDimensions.Height,
                     static_cast<float>(loaded.Grid.Height()));

        const auto index = m_Library.size();
        m_DirectoryContents[loaded.NormalizedFolder].Files.push_back(index);
        m_Library.emplace_back(std::move(loaded), m_WindowSize);
        registerFolderHierarchy(m_Library.back().RelativeFolder);

        RedrawPreset(m_Library.back(), m_LastWindowBounds, false);
    }
}
} // namespace Golde
