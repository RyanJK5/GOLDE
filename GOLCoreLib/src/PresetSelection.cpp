#include <algorithm>
#include <cstdint>
#include <font-awesome/IconsFontAwesome7.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <string>
#include <utility>

#include "DisabledScope.hpp"
#include "EditorResult.hpp"
#include "FileDialog.hpp"
#include "FileFormatHandler.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "Logging.hpp"
#include "PresetSelection.hpp"
#include "PresetSelectionResult.hpp"

namespace Golde {
PresetDisplay::PresetDisplay(const GameGrid& grid, const std::string& fileName,
                             Size2 windowSize)
    : Grid(grid), FileName(fileName),
      Graphics(std::filesystem::path("resources") / "shader", windowSize.Width,
               windowSize.Height, Color{}) {}

PresetSelection::PresetSelection(const std::filesystem::path& defaultPath,
                                 Size2 windowSize)
    : m_DefaultPath(defaultPath), m_WindowSize(windowSize) {
    ReadFiles(m_DefaultPath);
}

PresetSelectionResult PresetSelection::Update(const EditorResult& info) {
    ImGui::Begin("Presets", nullptr, ImGuiWindowFlags_NoNav);

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 10.f);

    ImGui::SetNextItemWidth(400.f);

    ImGui::InputTextWithHint("##search", "Search...", &m_SearchText);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_OPEN)) {
        if (const auto result =
                FileDialog::SelectFolderDialog(m_DefaultPath.string())) {
            m_Library.clear();
            m_DefaultPath = *result;
            ReadFiles(*result);
        }
    }
    ImGui::SetItemTooltip("Select Folder");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE)) {
        m_Library.clear();
        ReadFiles(m_DefaultPath);
    }
    ImGui::SetItemTooltip("Refresh Presets");

    ImGui::PopStyleVar();

    const bool enabled = Actions::Editable(info.Simulation.State);
    auto retString = std::string{};
    auto cursorPos = ImGui::GetCursorPos();
    auto numAvailable = 0;

    const auto spacing = ImGui::GetStyle().ItemSpacing;
    const auto layoutWidth = std::max(1.f, ImGui::GetContentRegionAvail().x);
    // Base ideal width (e.g. 10em) to prevent thumbnails from getting tiny
    const auto idealWidth = ImGui::GetFontSize() * 10.f;

    const auto numPerRow =
        std::max(1, static_cast<int>((layoutWidth + spacing.x) /
                                     (idealWidth + spacing.x)));

    // Calculate item width to optimally fit exactly `numPerRow` columns
    const auto templateWidth =
        std::max(10.f, (layoutWidth - spacing.x * (numPerRow - 1)) / numPerRow);

    const auto windowBounds =
        RectF{Vec2F{cursorPos}, Size2F{templateWidth, templateWidth}};

    {
        DisabledScope disableIf{!enabled};
        for (auto i = 0UZ; i < m_Library.size(); i++) {
            if (!m_Library[i].FileName.contains(m_SearchText))
                continue;

            const auto cellSize =
                std::min({10.f, windowBounds.Width / m_Library[i].Grid.Width(),
                          windowBounds.Height / m_Library[i].Grid.Height()});

            GraphicsHandlerArgs graphicsArgs{.ViewportBounds = windowBounds,
                                             .GridSize =
                                                 m_Library[i].Grid.Size(),
                                             .CellSize = {cellSize, cellSize},
                                             .ShowGridLines = false};

            m_Library[i].Graphics.RescaleFrameBuffer(windowBounds,
                                                     windowBounds);
            m_Library[i].Graphics.CenterCamera(graphicsArgs);
            m_Library[i].Graphics.ClearBackground(graphicsArgs);

            m_Library[i].Graphics.DrawGrid(Vec2{}, m_Library[i].Grid.Data(),
                                           graphicsArgs);

            if (numAvailable % numPerRow != 0)
                ImGui::SameLine();
            cursorPos = ImGui::GetCursorPos();

            ImGui::Image(
                static_cast<ImTextureID>(m_Library[i].Graphics.TextureID()),
                {windowBounds.Width, windowBounds.Height}, ImVec2{0, 1},
                ImVec2{1, 0});
            ImGui::SetItemTooltip("%s", m_Library[i].FileName.c_str());

            ImGui::SetCursorPos(cursorPos);

            if (ImGui::InvisibleButton(
                    std::format("##{}", m_Library[i].FileName).c_str(),
                    {windowBounds.Width, windowBounds.Height}))
                retString =
                    FileEncoder::EncodeRegion(
                        m_Library[i].Grid, {{0, 0}, m_Library[i].Grid.Size()})
                        .c_str();

            if (ImGui::IsItemHovered())
                m_Library[i].Graphics.DrawSelection(
                    {{0, 0}, graphicsArgs.GridSize}, graphicsArgs);

            numAvailable++;
        }
    }
    ImGui::End();
    return {.ClipboardText = retString};
}

void PresetSelection::ReadFiles(const std::filesystem::path& path) {
    m_MaxGridDimensions = Size2F{};
    for (const auto& file :
         std::filesystem::recursive_directory_iterator(path)) {
        if (!FileEncoder::IsFormatSupported(file.path().extension().string()))
            continue;

        auto result = FileEncoder::ReadRegion(file.path());
        if (!result) {
            ERROR("Failed to read file {}: {}", file.path().filename().string(),
                  result.error().Message);
            continue;
        }

        m_MaxGridDimensions.Width =
            std::max(m_MaxGridDimensions.Width,
                     static_cast<float>(result->Grid.Width()));
        m_MaxGridDimensions.Height =
            std::max(m_MaxGridDimensions.Height,
                     static_cast<float>(result->Grid.Height()));

        m_Library.emplace_back(std::move(result->Grid),
                               file.path().filename().string(), m_WindowSize);
    }
}
} // namespace Golde
