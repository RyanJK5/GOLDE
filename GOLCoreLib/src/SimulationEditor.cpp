#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <glm/fwd.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "EditorModel.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "HashQuadtree.hpp"
#include "LifeHashSet.hpp"
#include "LoadingSpinner.hpp"
#include "PopupWindow.hpp"
#include "PresetSelectionResult.hpp"
#include "SimulationCommand.hpp"
#include "SimulationControlResult.hpp"
#include "SimulationEditor.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace gol {
SimulationEditor::SimulationEditor(uint32_t id,
                                   const std::filesystem::path& path,
                                   Size2 windowSize, Size2 gridSize)
    : m_Model(id, path, gridSize),
      m_Graphics(std::filesystem::path("resources") / "shader",
                 windowSize.Width, windowSize.Height, {0.1f, 0.1f, 0.1f, 1.f}),
      m_FileErrorWindow("File Error", [](auto) {}),
      m_PasteWarning(
          "Paste Warning",
          std::bind_front(&SimulationEditor::PasteWarnUpdated, this)),
      m_SaveWarning("Save Warning", [](auto) {}) {}

bool SimulationEditor::operator==(const SimulationEditor& other) const {
    return m_Model == other.m_Model;
}

EditorResult
SimulationEditor::Update(std::optional<bool> activeOverride,
                         const SimulationControlResult& controlArgs,
                         const PresetSelectionResult& presetArgs) {
    auto displayResult = DisplaySimulation(
        (controlArgs.Command || !presetArgs.ClipboardText.empty()) &&
        activeOverride && (*activeOverride));
    if (!displayResult.Visible || (activeOverride && !(*activeOverride)))
        return {.Active = false, .Closing = displayResult.Closing};

    const auto graphicsArgs =
        GraphicsHandlerArgs{.ViewportBounds = ViewportBounds(),
                            .GridSize = m_Model.Grid().Size(),
                            .CellSize = {SimulationEditor::DefaultCellWidth,
                                         SimulationEditor::DefaultCellHeight},
                            .ShowGridLines = controlArgs.Settings.GridLines};
    UpdateViewport();
    UpdateDragState();
    m_Graphics.RescaleFrameBuffer(WindowBounds(), ViewportBounds());
    m_Graphics.ClearBackground(graphicsArgs);

    m_PasteWarning.Update();
    m_FileErrorWindow.Update();
    m_SaveWarning.Update();

    if (presetArgs.ClipboardText.length() > 0) {
        ImGui::SetClipboardText(presetArgs.ClipboardText.c_str());
        m_Model.InsertFromClipboard(Vec2{0, 0});
    }

    m_Model.CheckStopStep();
    m_Model.ApplySettings(controlArgs.Settings);

    if (controlArgs.Command &&
        ((activeOverride && *activeOverride) || displayResult.Selected))
        m_Model.SetState(UpdateState(controlArgs));

    m_Model.SetState([this, &graphicsArgs]() {
        switch (m_Model.State()) {
            using enum SimulationState;
        case Simulation:
            return SimulationUpdate(graphicsArgs);
        case Paint:
            return PaintUpdate(graphicsArgs);
        case Stepping:
            [[fallthrough]];
        case Paused:
            return PauseUpdate(graphicsArgs);
        case Empty:
            return PaintUpdate(graphicsArgs);
        case None:
            return Empty;
        };
        std::unreachable();
    }());

    return {
        .Simulation = {.State = m_Model.State()},
        .Editing = {.SelectionActive = m_Model.Selection().CanDrawGrid(),
                    .UndosAvailable = m_Model.Versions().UndosAvailable(),
                    .RedosAvailable = m_Model.Versions().RedosAvailable()},
        .File = {.CurrentFilePath = m_Model.CurrentFilePath(),
                 .HasUnsavedChanges = !m_Model.IsSaved()},
        .Active = (activeOverride && *activeOverride) || displayResult.Selected,
        .Closing = displayResult.Closing ||
                   (controlArgs.Command && std::holds_alternative<CloseCommand>(
                                               *controlArgs.Command))};
}

SimulationState
SimulationEditor::SimulationUpdate(const GraphicsHandlerArgs& args) {
    const auto snapshot = m_Model.Worker().GetResult();

    const auto data = snapshot->IterableData();
    if (std::holds_alternative<std::reference_wrapper<const LifeHashSet>>(data))
        m_Graphics.DrawGrid(
            {0, 0},
            std::get<std::reference_wrapper<const LifeHashSet>>(data).get(),
            args);
    else
        m_Graphics.DrawGrid(
            {0, 0},
            std::get<std::reference_wrapper<const HashQuadtree>>(data).get(),
            args);
    return SimulationState::Simulation;
}

SimulationState SimulationEditor::PaintUpdate(const GraphicsHandlerArgs& args) {
    auto gridPos = CursorGridPos();

    m_Graphics.DrawGrid({0, 0}, m_Model.Grid().Data(), args);
    if (m_Model.Selection().CanDrawGrid())
        m_Graphics.DrawGrid(m_Model.Selection().SelectionBounds().UpperLeft(),
                            m_Model.Selection().GridData(), args);
    if (m_Model.Selection().CanDrawSelection())
        m_Graphics.DrawSelection(m_Model.Selection().SelectionBounds(), args);

    if (gridPos) {
        UpdateMouseState(*gridPos);
    } else {
        m_Model.TryResetSelection();
    }
    m_LeftDeltaLast = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

    return (m_Model.Grid().Dead() && !m_Model.Selection().GridAlive())
               ? SimulationState::Empty
               : SimulationState::Paint;
}

SimulationState SimulationEditor::PauseUpdate(const GraphicsHandlerArgs& args) {
    auto gridPos = CursorGridPos();
    if (gridPos)
        m_Model.UpdateSelectionAreaTracked(*gridPos);

    const auto data = m_Model.Grid().IterableData();
    if (std::holds_alternative<std::reference_wrapper<const LifeHashSet>>(data))
        m_Graphics.DrawGrid(
            {0, 0},
            std::get<std::reference_wrapper<const LifeHashSet>>(data).get(),
            args);
    else
        m_Graphics.DrawGrid(
            {0, 0},
            std::get<std::reference_wrapper<const HashQuadtree>>(data).get(),
            args);

    if (m_Model.Selection().CanDrawSelection())
        m_Graphics.DrawSelection(m_Model.Selection().SelectionBounds(), args);
    if (m_Model.Selection().CanDrawGrid())
        m_Graphics.DrawGrid(m_Model.Selection().SelectionBounds().UpperLeft(),
                            m_Model.Selection().GridData(), args);
    return m_Model.State();
}

SimulationEditor::DisplayResult
SimulationEditor::DisplaySimulation(bool grabFocus) {
    auto label = std::format(
        "{}{}###Simulation{}",
        m_Model.CurrentFilePath().empty()
            ? "(untitled)"
            : m_Model.CurrentFilePath().filename().string().c_str(),
        (!m_Model.CurrentFilePath().empty() && !m_Model.IsSaved()) ? "*" : "",
        m_Model.EditorID());

    bool stayOpen = true;
    auto windowClass = ImGuiWindowClass{};
    windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
    ImGui::SetNextWindowClass(&windowClass);
    if (!ImGui::Begin(label.c_str(), &stayOpen)) {
        ImGui::End();
        return {.Visible = false, .Closing = !stayOpen};
    }
    bool windowFocused = ImGui::IsWindowFocused();

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
    ImGui::BeginChild("Render");

    ImDrawListSplitter splitter{};
    splitter.Split(ImGui::GetWindowDrawList(), 2);

    splitter.SetCurrentChannel(ImGui::GetWindowDrawList(), 0);
    m_WindowBounds = {Vec2F(ImGui::GetWindowPos()),
                      Size2F(ImGui::GetContentRegionAvail())};

    ImGui::Image(static_cast<ImTextureID>(m_Graphics.TextureID()),
                 ImGui::GetContentRegionAvail(), {0, 1}, {1, 0});
    ImGui::SetCursorPosY(0);
    ImGui::InvisibleButton("##SimulationViewport",
                           ImGui::GetContentRegionAvail());

    if (grabFocus || (m_TakeKeyboardInput && !ImGui::IsItemFocused() &&
                      ImGui::IsKeyPressed(ImGuiKey_Escape)))
        ImGui::SetKeyboardFocusHere(-1);

    m_TakeKeyboardInput = ImGui::IsItemFocused() || windowFocused;
    m_TakeMouseInput = ImGui::IsItemHovered();

    if ((m_Model.State() == SimulationState::Simulation ||
         m_Model.State() == SimulationState::Stepping) &&
        m_Model.Worker().GetTimeSinceLastUpdate() > std::chrono::seconds{5}) {
        constexpr static auto radius = 30.f;
        constexpr static auto thickness = 12.f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - radius * 2 -
                             thickness);
        ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - radius * 2 -
                             thickness);
        LoadingSpinner("##LoadingSpinner", radius, thickness,
                       ImGui::GetColorU32(ImGuiCol_Text));
    }

    splitter.SetCurrentChannel(ImGui::GetWindowDrawList(), 1);
    ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());

    const auto snapshot = m_Model.Worker().GetResult();
    const auto generation =
        snapshot ? snapshot->Generation() : m_Model.Grid().Generation();
    const auto population =
        snapshot ? snapshot->Population() : m_Model.Grid().Population();
    ImGui::Text(
        "%s",
        std::format(std::locale{""}, "Generation: {:L}", generation).c_str());

    const auto totalPopulation =
        BigInt{population + m_Model.Selection().SelectedPopulation()};
    ImGui::Text(
        "%s", std::format(std::locale{""}, "Population: {:L}", totalPopulation)
                  .c_str());

    if (m_Model.Selection().CanDrawSelection()) {
        const auto pos = m_Model.Selection().SelectionBounds().UpperLeft();
        auto text = std::format("({}, {})", pos.X, pos.Y);
        if (m_Model.Selection().CanDrawLargeSelection()) {
            const auto sentinel =
                m_Model.Selection().SelectionBounds().LowerRight();
            text += std::format(" X ({}, {})", sentinel.X, sentinel.Y);
            text += std::format(", width: {}, height: {}",
                                std::abs(sentinel.X - pos.X),
                                std::abs(sentinel.Y - pos.Y));
        }
        ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y -
                             ImGui::CalcTextSize(text.c_str()).y);
        ImGui::Text("%s", text.c_str());
    }

    splitter.Merge(ImGui::GetWindowDrawList());
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();

    return {
        .Visible = true, .Selected = m_TakeKeyboardInput, .Closing = !stayOpen};
}

Rect SimulationEditor::WindowBounds() const {
    return Rect(static_cast<int32_t>(m_WindowBounds.X),
                static_cast<int32_t>(m_WindowBounds.Y),
                static_cast<int32_t>(m_WindowBounds.Width),
                static_cast<int32_t>(m_WindowBounds.Height));
}

Rect SimulationEditor::ViewportBounds() const { return WindowBounds(); }

std::optional<Vec2> SimulationEditor::ConvertToGridPos(Vec2F screenPos) {
    if (!ViewportBounds().InBounds(screenPos.X, screenPos.Y))
        return std::nullopt;

    glm::dvec2 vec =
        m_Graphics.Camera.ScreenToWorldPos(screenPos, ViewportBounds());
    vec /= glm::dvec2{DefaultCellWidth, DefaultCellHeight};

    Vec2 result = {static_cast<int32_t>(std::floor(vec.x)),
                   static_cast<int32_t>(std::floor(vec.y))};
    if (!m_Model.Grid().InBounds(result))
        return std::nullopt;
    return result;
}

std::optional<Vec2> SimulationEditor::CursorGridPos() {
    return ConvertToGridPos(ImGui::GetMousePos());
}

SimulationState
SimulationEditor::UpdateState(const SimulationControlResult& result) {
    if (result.FromShortcut && !m_TakeKeyboardInput)
        return m_Model.State();

    return std::visit(
        Overloaded{
            [this](const StartCommand&) { return m_Model.HandleStart(); },
            [this](const ClearCommand&) { return m_Model.HandleClear(); },
            [this](const ResetCommand&) { return m_Model.HandleReset(); },
            [this](const RestartCommand&) { return m_Model.HandleRestart(); },
            [this](const PauseCommand&) { return m_Model.HandlePause(); },
            [this](const ResumeCommand&) { return m_Model.HandleResume(); },
            [this](const StepCommand&) { return m_Model.HandleStep(); },
            [this](const ResizeCommand& cmd) {
                auto oldWidth = m_Model.Grid().Width();
                auto oldHeight = m_Model.Grid().Height();
                auto state = m_Model.HandleResize(cmd.NewDimensions);
                if (m_Model.Grid().Width() != oldWidth ||
                    m_Model.Grid().Height() != oldHeight) {
                    m_Graphics.Camera.Center = {
                        m_Model.Grid().Width() * DefaultCellWidth / 2.f,
                        m_Model.Grid().Height() * DefaultCellHeight / 2.f};
                }
                return state;
            },
            [this](const GenerateNoiseCommand& cmd) {
                return m_Model.HandleGenerateNoise(cmd.Density);
            },
            [this](const UndoCommand&) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    return m_Model.State();
                return m_Model.HandleUndo();
            },
            [this](const RedoCommand&) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    return m_Model.State();
                return m_Model.HandleRedo();
            },
            [this](const SaveCommand& cmd) {
                const static BigInt threshold{10'000'000U};
                if (m_Model.Grid().Population() > threshold) {
                    m_SaveWarning.SetCallback(
                        [this, path = cmd.FilePath](PopupWindowState state) {
                            if (state != PopupWindowState::Success)
                                return;
                            SaveWithErrorHandling(path, true);
                        });
                    m_SaveWarning.Activate();
                    m_SaveWarning.Message = std::format(
                        std::locale{""},
                        "This file has {:L} total cells. The saved file will "
                        "be\n"
                        "large and may take a long time to save. Are you sure\n"
                        "you want to continue?",
                        m_Model.Grid().Population());
                    return m_Model.State();
                }
                SaveWithErrorHandling(cmd.FilePath, true);
                return m_Model.State();
            },
            [this](const SaveAsNewCommand& cmd) {
                const static BigInt threshold{10'000'000U};
                if (m_Model.Grid().Population() > threshold) {
                    m_SaveWarning.SetCallback(
                        [this, path = cmd.FilePath](PopupWindowState state) {
                            if (state != PopupWindowState::Success)
                                return;
                            SaveWithErrorHandling(path, false);
                        });
                    m_SaveWarning.Activate();
                    m_SaveWarning.Message = std::format(
                        std::locale{""},
                        "This file has {:L} total cells. The saved file will "
                        "be\n"
                        "large and may take a long time to save. Are you sure\n"
                        "you want to continue?",
                        m_Model.Grid().Population());
                    return m_Model.State();
                }
                SaveWithErrorHandling(cmd.FilePath, false);
                return m_Model.State();
            },
            [this](const LoadCommand& cmd) {
                auto error = m_Model.LoadFile(cmd.FilePath);
                if (error) {
                    m_FileErrorWindow.Activate();
                    m_FileErrorWindow.Message =
                        std::format("Failed to load file:\n{}", *error);
                }
                m_Model.MarkSaved();
                return m_Model.State();
            },
            [this](const NewFileCommand&) { return m_Model.State(); },
            [this](const CloseCommand&) { return m_Model.State(); },
            [this](const SelectionCommand& cmd) {
                if (cmd.Action == SelectionAction::Paste) {
                    HandlePasteResult(m_Model.PasteSelection(CursorGridPos()));
                    return m_Model.State();
                }
                m_Model.HandleSelectionAction(cmd.Action, cmd.NudgeSize);
                return m_Model.State();
            }},
        *result.Command);
}

void SimulationEditor::SaveWithErrorHandling(const std::filesystem::path& path,
                                             bool markAsSaved) {
    if (!m_Model.SaveToFile(path, markAsSaved)) {
        m_FileErrorWindow.Activate();
        m_FileErrorWindow.Message =
            std::format("Failed to save file to \n{}", path.string());
    }
}

void SimulationEditor::HandlePasteResult(const PasteResult& result) {
    if (result.Value == PasteResult::Status::TooLarge) {
        m_PasteWarning.Activate();
        m_PasteWarning.Message =
            std::format(std::locale{""},
                        "Your selection ({:L} cells) is too large\n"
                        "to paste without potential performance issues.\n"
                        "Are you sure you want to continue?",
                        *result.CellCount);
    } else if (result.Value == PasteResult::Status::ClipboardError) {
        m_FileErrorWindow.Activate();
        m_FileErrorWindow.Message = "Failed to read from clipboard.";
    }
}

void SimulationEditor::PasteWarnUpdated(PopupWindowState state) {
    if (state != PopupWindowState::Success)
        return;
    m_Model.ForcePaste(CursorGridPos());
}

void SimulationEditor::UpdateMouseState(Vec2 gridPos) {
    if (!m_TakeMouseInput ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        return;

    if (m_Model.UpdateSelectionAreaTracked(gridPos)) {
        m_EditorMode = EditorMode::Select;
        return;
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !ImGui::IsKeyDown(ImGuiKey_LeftShift) &&
        !ImGui::IsKeyDown(ImGuiKey_RightShift)) {
        if (m_EditorMode == EditorMode::None ||
            m_EditorMode == EditorMode::Select) {
            m_EditorMode = *m_Model.Grid().Get(gridPos.X, gridPos.Y)
                               ? EditorMode::Delete
                               : EditorMode::Insert;
            m_Model.BeginPaintChange(gridPos,
                                     m_EditorMode == EditorMode::Insert);
            m_LeftDeltaLast = {};
        }

        FillCells();
        return;
    }
    m_EditorMode = EditorMode::None;
}

void SimulationEditor::FillCells() {
    const auto mousePos = Vec2F{ImGui::GetMousePos()};
    const auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

    const auto realDelta =
        Vec2F{delta.x - m_LeftDeltaLast.X, delta.y - m_LeftDeltaLast.Y};
    const auto lastPos =
        Vec2F{mousePos.X - realDelta.X, mousePos.Y - realDelta.Y};

    auto currentGridPos = ConvertToGridPos(mousePos);
    auto lastGridPos = ConvertToGridPos(lastPos);

    if (!currentGridPos || !lastGridPos)
        return;

    auto gridDelta = Vec2{currentGridPos->X - lastGridPos->X,
                          currentGridPos->Y - lastGridPos->Y};
    int32_t steps = std::max(std::abs(gridDelta.X), std::abs(gridDelta.Y));

    if (steps == 0) {
        m_Model.PaintCell(*currentGridPos, m_EditorMode == EditorMode::Insert);
        return;
    }

    for (int32_t i = 0; i <= steps; i++) {
        Vec2 gridPos = {lastGridPos->X + (gridDelta.X * i) / steps,
                        lastGridPos->Y + (gridDelta.Y * i) / steps};

        if (!m_Model.Grid().InBounds(gridPos))
            continue;

        m_Model.PaintCell(gridPos, m_EditorMode == EditorMode::Insert);
    }
}

void SimulationEditor::UpdateDragState() {
    if (!m_TakeMouseInput || !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        m_RightDeltaLast = {0.f, 0.f};
        return;
    }

    Vec2F delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    m_Graphics.Camera.Translate(glm::vec2(delta) - glm::vec2(m_RightDeltaLast));
    m_RightDeltaLast = delta;
}

void SimulationEditor::UpdateViewport() {
    const Rect bounds = ViewportBounds();

    auto mousePos = ImGui::GetIO().MousePos;
    if (bounds.InBounds(mousePos.x, mousePos.y) &&
        ImGui::GetIO().MouseWheel != 0)
        m_Graphics.Camera.ZoomBy(mousePos, bounds,
                                 ImGui::GetIO().MouseWheel / 10.f);
}
} // namespace gol
