#include <algorithm>
#include <cstdint>
#include <font-awesome/IconsFontAwesome7.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <utility>

#include "EditorResult.hpp"
#include "FileDialog.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "InputString.hpp"
#include "Logging.hpp"
#include "PresetSelection.hpp"
#include "PresetSelectionResult.hpp"
#include "RLEEncoder.hpp"

namespace gol {
PresetDisplay::PresetDisplay(const GameGrid& grid, const std::string& fileName,
                             Size2 windowSize)
    : Grid(grid), FileName(fileName),
      Graphics(std::filesystem::path("resources") / "shader", windowSize.Width,
               windowSize.Height, Color{}) {}

PresetSelection::PresetSelection(const std::filesystem::path& defaultPath,
                                 Size2 windowSize)
    : m_DefaultPath(defaultPath), m_WindowSize(windowSize) {
    ReadFiles(m_DefaultPath);
    m_SearchText = InputString{m_MaxFileName};
}

PresetSelectionResult PresetSelection::Update(const EditorResult& info) {
    ImGui::Begin("Presets", nullptr, ImGuiWindowFlags_NoNav);

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 10.f);

    ImGui::SetNextItemWidth(m_MaxFileName * 20.f);
    ImGui::InputTextWithHint("##search", "Search...", m_SearchText.Data,
                             m_SearchText.Length + 1);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_OPEN)) {
        auto result = FileDialog::SelectFolderDialog(m_DefaultPath.string());
        if (result) {
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

    const bool enabled = info.Simulation.State == SimulationState::Paint ||
                         info.Simulation.State == SimulationState::Empty;
    auto retString = std::string{};
    auto cursorPos = ImGui::GetCursorPos();
    auto numAvailable = 0;

    const auto windowBounds =
        RectF{Vec2F{cursorPos},
              Size2F{static_cast<float>(TemplateDimensions.Width),
                     static_cast<float>(TemplateDimensions.Height)}};
    const auto cellSize =
        std::min(windowBounds.Width / m_MaxGridDimensions.Width,
                 windowBounds.Height / m_MaxGridDimensions.Height);
    const auto spacing = ImGui::GetStyle().ItemSpacing;
    const auto numPerRow =
        std::max(1, static_cast<int32_t>(std::floor(
                        ImGui::GetContentRegionMax().x /
                        (cellSize * m_MaxGridDimensions.Width + spacing.x))));

    if (!enabled) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
    }
    for (auto i = 0UZ; i < m_Library.size(); i++) {
        if (!m_Library[i].FileName.contains(m_SearchText.Data))
            continue;

        auto graphicsArgs =
            GraphicsHandlerArgs{.ViewportBounds = windowBounds,
                                .GridSize = m_Library[i].Grid.Size(),
                                .CellSize = {cellSize, cellSize},
                                .ShowGridLines = false};

        m_Library[i].Graphics.RescaleFrameBuffer(windowBounds, windowBounds);
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
        ImGui::SetItemTooltip(
            "%s", std::format("{}.gol", m_Library[i].FileName).c_str());

        ImGui::SetCursorPos(cursorPos);

        if (ImGui::InvisibleButton(
                std::format("##{}", m_Library[i].FileName).c_str(),
                {windowBounds.Width, windowBounds.Height}))
            retString =
                RLEEncoder::EncodeRegion(m_Library[i].Grid,
                                         {{0, 0}, m_Library[i].Grid.Size()})
                    .c_str();

        if (ImGui::IsItemHovered())
            m_Library[i].Graphics.DrawSelection({{0, 0}, graphicsArgs.GridSize},
                                                graphicsArgs);

        numAvailable++;
    }
    if (!enabled) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
    ImGui::End();
    return {.ClipboardText = retString};
}

void PresetSelection::ReadFiles(const std::filesystem::path& path) {
    m_MaxGridDimensions = Size2F{};
    for (const auto& file :
         std::filesystem::recursive_directory_iterator(path)) {
        if (file.path().extension() != ".gol")
            continue;

        auto result = RLEEncoder::ReadRegion(file.path());
        if (!result) {
            ERROR("Failed to open file {}: {}", file.path().filename().string(),
                  result.error());
            continue;
        }

        m_MaxGridDimensions.Width =
            std::max(m_MaxGridDimensions.Width,
                     static_cast<float>(result->Grid.Width()));
        m_MaxGridDimensions.Height =
            std::max(m_MaxGridDimensions.Height,
                     static_cast<float>(result->Grid.Height()));

        std::string name = file.path().filename().string();
        name = name.substr(0, name.find_last_of('.'));
        m_MaxFileName = std::max(m_MaxFileName, name.length());
        m_Library.emplace_back(std::move(result->Grid), std::move(name),
                               m_WindowSize);
    }
}
} // namespace gol
