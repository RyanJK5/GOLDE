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
#include <vector>

#include "BigInt.hpp"
#include "EditorModel.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "HashQuadtree.hpp"
#include "LoadingSpinner.hpp"
#include "PopupWindow.hpp"
#include "PresetSelectionResult.hpp"
#include "SimulationCommand.hpp"
#include "SimulationControlResult.hpp"
#include "SimulationEditor.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace gol {
namespace {
constexpr int32_t SelectionCoordLimit = std::numeric_limits<int32_t>::max() / 2;

std::optional<BigInt> FloorToBigInt(double value) {
    if (!std::isfinite(value)) {
        return std::nullopt;
    }

    const auto floored = std::floor(value);
    if (std::abs(floored) <=
        static_cast<double>(std::numeric_limits<int64_t>::max())) {
        return BigInt{static_cast<int64_t>(floored)};
    }

    return BigInt{std::format("{:.0f}", floored)};
}

std::string FormatSelectionPoint(Vec2 point) {
    return std::format("({}, {})", point.X, point.Y);
}

std::string FormatHoverPoint(glm::dvec2 worldCellPos) {
    const auto cursorX = FloorToBigInt(worldCellPos.x);
    const auto cursorY = FloorToBigInt(worldCellPos.y);
    if (!cursorX || !cursorY) {
        return "(out of range)";
    }

    const BigInt minCoord = -BigInt{SelectionCoordLimit};
    const BigInt maxCoord = BigInt{SelectionCoordLimit};
    const auto inRange = *cursorX >= minCoord && *cursorX <= maxCoord &&
                         *cursorY >= minCoord && *cursorY <= maxCoord;

    if (inRange) {
        return std::format("({}, {})", static_cast<int32_t>(*cursorX),
                           static_cast<int32_t>(*cursorY));
    }

    return std::format("{} [out of range]", BigVec2{*cursorX, *cursorY});
}
} // namespace

SimulationEditor::SimulationEditor(uint32_t id,
                                   const std::filesystem::path& path,
                                   Size2 windowSize, Size2 gridSize)
    : m_Model(id, path, gridSize),
      m_Graphics(std::filesystem::path("resources") / "shader",
                 windowSize.Width, windowSize.Height, {0.1f, 0.1f, 0.1f, 1.f}),
      m_FileErrorWindow("File Error", [](auto) {}),
      m_CopyErrorWindow("Copy Error", [](auto) {}),
      m_PasteWarning(
          "Paste Warning",
          std::bind_front(&SimulationEditor::PasteWarnUpdated, this)),
      m_GenerateNoiseError("Noise Generation Error", [](auto) {}),
      m_SaveWarning("Save Warning", [](auto) {}) {}

bool SimulationEditor::operator==(const SimulationEditor& other) const {
    return m_Model == other.m_Model;
}

EditorResult
SimulationEditor::Update(std::optional<bool> activeOverride,
                         const SimulationControlResult& controlArgs,
                         const PresetSelectionResult& presetArgs) {
    PollPendingCommandResult();
    HashQuadtree::SetCacheIndex(m_Model.EditorID());

    auto displayResult = DisplaySimulation(
        (controlArgs.Command || !presetArgs.ClipboardText.empty()) &&
        activeOverride && (*activeOverride));
    if (!displayResult.Visible || (activeOverride && !(*activeOverride)))
        return {.Active = false, .Closing = displayResult.Closing};

    const auto graphicsArgs =
        GraphicsHandlerArgs{.ViewportBounds = ViewportBounds(),
                            .GridSize = m_Model.GridSize(),
                            .CellSize = {SimulationEditor::DefaultCellWidth,
                                         SimulationEditor::DefaultCellHeight},
                            .ShowGridLines = controlArgs.Settings.GridLines};
    UpdateViewport();
    UpdateDragState();
    m_Graphics.RescaleFrameBuffer(WindowBounds(), ViewportBounds());
    m_Graphics.ClearBackground(graphicsArgs);

    m_PasteWarning.Update();
    m_CopyErrorWindow.Update();
    m_GenerateNoiseError.Update();
    m_FileErrorWindow.Update();
    m_SaveWarning.Update();

    if (presetArgs.ClipboardText.length() > 0) {
        const auto dispatch = m_Model.CanDispatchEdit();
        if (dispatch.Accepted) {
            ImGui::SetClipboardText(presetArgs.ClipboardText.c_str());
            m_Model.InsertFromClipboard(Vec2{0, 0}, ImGui::GetClipboardText());
        }
    }

    m_Model.CheckStopStep();
    m_Model.ApplySettings(controlArgs.Settings);
    PollPendingCommandResult();

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
        .Simulation = {.State = m_Model.State(),
                       .OutOfBounds = m_Model.IsSimulationOutOfBounds(),
                       .RuleString = m_Model.CurrentRuleString()},
        .Editing = {.SelectionActive = m_Model.SelectionActive(),
                    .UndosAvailable = m_Model.UndosAvailable(),
                    .RedosAvailable = m_Model.RedosAvailable()},
        .File = {.CurrentFilePath = m_Model.CurrentFilePath(),
                 .HasUnsavedChanges = !m_Model.IsSaved()},
        .SelectionBounds = m_Model.SelectionBoundsOpt(),
        .Zoom = m_Graphics.Camera.Zoom,
        .Active = (activeOverride && *activeOverride) || displayResult.Selected,
        .Closing = displayResult.Closing ||
                   (controlArgs.Command && std::holds_alternative<CloseCommand>(
                                               *controlArgs.Command))};
}

SimulationState
SimulationEditor::SimulationUpdate(const GraphicsHandlerArgs& args) {
    const auto snapshot = m_Model.SimulationSnapshot();

    m_Graphics.DrawGrid({0, 0}, snapshot->Data(), args);
    return SimulationState::Simulation;
}

SimulationState SimulationEditor::PaintUpdate(const GraphicsHandlerArgs& args) {
    auto gridPos = CursorGridPos();
    const auto selectionBounds = m_Model.SelectionBoundsOpt();

    m_Graphics.DrawGrid({0, 0}, m_Model.GridData(), args);

    if (selectionBounds && m_Model.SelectionActive()) {
        m_Graphics.DrawGrid(selectionBounds->UpperLeft(),
                            m_Model.SelectionGridData(), args);
    }
    if (selectionBounds && m_Model.CanDrawSelection()) {
        m_Graphics.DrawSelection(*selectionBounds, args);
    }

    if (gridPos) {
        UpdateMouseState(*gridPos);
    } else {
        m_Model.TryResetSelection();
    }
    m_LeftDeltaLast = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

    return (m_Model.GridDead() && !m_Model.SelectionGridAlive())
               ? SimulationState::Empty
               : SimulationState::Paint;
}

SimulationState SimulationEditor::PauseUpdate(const GraphicsHandlerArgs& args) {
    const auto gridPos = CursorGridPos();
    if (gridPos)
        m_Model.UpdateSelectionAreaTracked(*gridPos);
    const auto selectionBounds = m_Model.SelectionBoundsOpt();

    m_Graphics.DrawGrid({0, 0}, m_Model.GridData(), args);

    if (selectionBounds && m_Model.CanDrawSelection()) {
        m_Graphics.DrawSelection(*selectionBounds, args);
    }
    if (selectionBounds && m_Model.SelectionActive()) {
        m_Graphics.DrawGrid(selectionBounds->UpperLeft(),
                            m_Model.SelectionGridData(), args);
    }

    if (m_Model.State() == SimulationState::Paused) {
        if (gridPos) {
            UpdateMouseState(*gridPos);
        } else {
            m_Model.TryResetSelection();
        }
    }
    m_LeftDeltaLast = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

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

    if (((m_Model.State() == SimulationState::Simulation ||
          m_Model.State() == SimulationState::Stepping) &&
         m_Model.SimulationLag() >= std::chrono::seconds{3}) ||
        m_Model.IsEditBusy()) {
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

    const auto snapshot = m_Model.SimulationSnapshot();
    const auto generation =
        snapshot ? snapshot->Generation() : m_Model.GridGeneration();
    const auto population =
        snapshot ? snapshot->Population() : m_Model.GridPopulation();
    ImGui::Text(
        "%s",
        std::format(std::locale{""}, "Generation: {:L}", generation).c_str());

    const auto totalPopulation =
        BigInt{population + m_Model.SelectedPopulation()};
    ImGui::Text(
        "%s", std::format(std::locale{""}, "Population: {:L}", totalPopulation)
                  .c_str());

#ifdef _DEBUG
    auto currentTime = glfwGetTime();
    frameCounter++;
    if (currentTime - lastTime > 1.0) {
        fps = frameCounter;
        frameCounter = 0;
        lastTime += 1.0;
    }
    ImGui::Text("%d FPS", fps);
#endif

    const auto mousePos = Vec2F{ImGui::GetMousePos()};
    const auto viewportBounds = ViewportBounds();
    const auto hoverText = [&] -> std::optional<std::string> {
        if (!viewportBounds.InBounds(mousePos.X, mousePos.Y)) {
            return std::nullopt;
        }

        auto worldPos =
            m_Graphics.Camera.ScreenToWorldPos(mousePos, viewportBounds);
        worldPos /= glm::dvec2{DefaultCellWidth, DefaultCellHeight};
        return FormatHoverPoint(worldPos);
    }();

    const auto selectionBounds = m_Model.SelectionBoundsOpt();
    if (selectionBounds && m_Model.CanDrawSelection()) {
        const auto bounds = *selectionBounds;
        const auto anchor = bounds.UpperLeft();
        const auto sentinel = bounds.LowerRight() - Vec2{1, 1};

        auto text = FormatSelectionPoint(anchor);
        if (m_Model.CanDrawLargeSelection()) {
            text += std::format(" X {}", FormatSelectionPoint(sentinel));
            text += std::format(std::locale{""}, ", width: {:L}; height: {:L}",
                                std::abs(sentinel.X - anchor.X) + 1,
                                std::abs(sentinel.Y - anchor.Y) + 1);
            if (m_Model.SelectionActive()) {
                text += std::format(std::locale{""}, " ({:L} cells)",
                                    m_Model.SelectedPopulation());
            }
        }

        ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y -
                             ImGui::CalcTextSize(text.c_str()).y);
        ImGui::Text("%s", text.c_str());
    } else if (hoverText) {
        ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y -
                             ImGui::CalcTextSize(hoverText->c_str()).y);
        ImGui::Text("%s", hoverText->c_str());
    }

    splitter.Merge(ImGui::GetWindowDrawList());
    ImGui::EndChild();
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

    const auto cellX = std::floor(vec.x);
    const auto cellY = std::floor(vec.y);
    const auto minCoord = static_cast<double>(-SelectionCoordLimit);
    const auto maxCoord = static_cast<double>(SelectionCoordLimit);
    if (!std::isfinite(cellX) || !std::isfinite(cellY) || cellX < minCoord ||
        cellX > maxCoord || cellY < minCoord || cellY > maxCoord) {
        return std::nullopt;
    }

    const Vec2 result{static_cast<int32_t>(cellX), static_cast<int32_t>(cellY)};
    if (!m_Model.InBounds(result))
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
    if (!result.Command) {
        return m_Model.State();
    }
    if (!m_Model.CanDispatchMutatingCommand(*result.Command)) {
        return m_Model.State();
    }

    ExecuteCommandContext context{
        .CursorPos = CursorGridPos(),
        .PrimaryMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left)};

    return ExecuteEditorCommand(*result.Command, context);
}

SimulationState
SimulationEditor::ExecuteEditorCommand(const SimulationCommand& command,
                                       const ExecuteCommandContext& context) {
    if (!m_Model.TryStartCommand(command, context)) {
        return m_Model.State();
    }
    PollPendingCommandResult();
    return m_Model.State();
}

void SimulationEditor::ApplyCommandResult(
    const ExecuteCommandResult& commandResult) {
    m_Model.SetState(commandResult.State);

    if (commandResult.ClipboardText) {
        ImGui::SetClipboardText(commandResult.ClipboardText->c_str());
    }
    if (commandResult.CameraPositionCell) {
        glm::dvec2 precisePos{
            commandResult.CameraPositionCell->X * DefaultCellWidth,
            commandResult.CameraPositionCell->Y * DefaultCellHeight};
        m_Graphics.Camera.Center = precisePos;
    }
    if (commandResult.CameraZoom) {
        m_Graphics.Camera.Zoom =
            std::clamp(*commandResult.CameraZoom, GraphicsCamera::MinZoom,
                       GraphicsCamera::MaxZoom);
    }
    if (commandResult.RecenterCameraToGridCenter) {
        m_Graphics.Camera.Center = {
            m_Model.GridWidth() * DefaultCellWidth / 2.f,
            m_Model.GridHeight() * DefaultCellHeight / 2.f};
    }

    if (commandResult.ErrorType != ExecuteCommandErrorType::None &&
        commandResult.ErrorMessage) {
        switch (commandResult.ErrorType) {
            using enum ExecuteCommandErrorType;
        case Noise:
            m_GenerateNoiseError.Activate();
            m_GenerateNoiseError.Message = *commandResult.ErrorMessage;
            break;
        case File:
            m_FileErrorWindow.Activate();
            m_FileErrorWindow.Message = *commandResult.ErrorMessage;
            break;
        case Copy:
            m_CopyErrorWindow.Activate();
            m_CopyErrorWindow.Message = *commandResult.ErrorMessage;
            break;
        case PasteTooManyCells:
            m_PasteWarning.Activate();
            m_PasteWarning.Message =
                std::format("{}\nAre you sure you want to continue?",
                            *commandResult.ErrorMessage);
            break;
        case Paste:
            m_FileErrorWindow.Activate();
            m_FileErrorWindow.Message = *commandResult.ErrorMessage;
            break;
        case None:
            break;
        }
    }
    if (commandResult.SaveAsWarning) {
        const auto saveAsRequest = *commandResult.SaveAsWarning;
        m_SaveWarning.SetCallback([this, path = saveAsRequest.FilePath](
                                      PopupWindowState state) {
            if (state != PopupWindowState::Success)
                return;
            ExecuteEditorCommand(
                SaveAsNewCommand{.FilePath = path},
                {.CursorPos = CursorGridPos(),
                 .PrimaryMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left),
                 .ConfirmSaveAsWarning = true});
        });
        m_SaveWarning.Activate();
        m_SaveWarning.Message =
            std::format(std::locale{""},
                        "This file has {:L} total cells. The saved file will "
                        "be\n"
                        "large and may take a long time to save. The Macrocell "
                        "(.mc)\n"
                        "format may be more efficient. Are you sure you want "
                        "to \n"
                        "continue?",
                        saveAsRequest.Population);
    }
}

void SimulationEditor::PollPendingCommandResult() {
    auto result = m_Model.PollCommandResult();
    if (result) {
        ApplyCommandResult(*result);
    }
}

void SimulationEditor::PasteWarnUpdated(PopupWindowState state) {
    if (state != PopupWindowState::Success)
        return;

    ExecuteCommandContext context{.CursorPos = CursorGridPos(),
                                  .PrimaryMouseDown =
                                      ImGui::IsMouseDown(ImGuiMouseButton_Left),
                                  .ForcePasteSelection = true};
    ExecuteEditorCommand(
        SelectionCommand{.ClipboardText = ImGui::GetClipboardText(),
                         .Action = SelectionAction::Paste},
        context);
}

void SimulationEditor::UpdateMouseState(Vec2 gridPos) {
    if (!m_TakeMouseInput ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        return;

    const auto paintingInput = ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                               !ImGui::IsKeyDown(ImGuiKey_LeftShift) &&
                               !ImGui::IsKeyDown(ImGuiKey_RightShift);

    if (m_Model.UpdateSelectionAreaTracked(gridPos)) {
        m_EditorMode = EditorMode::Select;
        return;
    }

    if (m_Graphics.Camera.Zoom < 0.001f) {
        return;
    }

    const auto dispatch = m_Model.CanDispatchEdit();
    if (!dispatch.Accepted) {
        if (dispatch.RejectedReason == EditRejectReason::Busy &&
            paintingInput &&
            (m_EditorMode == EditorMode::Insert ||
             m_EditorMode == EditorMode::Delete)) {
            FillCells(gridPos, false);
            return;
        }

        m_BeginPaintStroke = false;
        m_LastPaintGridPos = std::nullopt;
        m_BufferedPaintPoints.clear();
        m_EditorMode = EditorMode::None;
        return;
    }

    if (paintingInput) {
        if (m_EditorMode == EditorMode::None ||
            m_EditorMode == EditorMode::Select) {
            m_EditorMode = *m_Model.CellAt(gridPos) ? EditorMode::Delete
                                                    : EditorMode::Insert;
            m_BeginPaintStroke = true;
            m_LastPaintGridPos = gridPos;
            m_BufferedPaintPoints.clear();
        }

        FillCells(gridPos, true);
        return;
    }

    if (!m_BufferedPaintPoints.empty() &&
        (m_EditorMode == EditorMode::Insert ||
         m_EditorMode == EditorMode::Delete)) {
        ExecuteEditorCommand(
            PaintStrokeCommand{.Points = std::move(m_BufferedPaintPoints),
                               .Value = m_EditorMode == EditorMode::Insert,
                               .BeginStroke = m_BeginPaintStroke},
            {.CursorPos = gridPos,
             .PrimaryMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left)});
        m_BeginPaintStroke = false;
        m_BufferedPaintPoints.clear();
    }

    m_BeginPaintStroke = false;
    m_LastPaintGridPos = std::nullopt;
    m_BufferedPaintPoints.clear();
    m_EditorMode = EditorMode::None;
}

void SimulationEditor::FillCells(Vec2 gridPos, bool canDispatch) {
    if (!m_LastPaintGridPos) {
        m_LastPaintGridPos = gridPos;
    }

    const auto lastGridPos = *m_LastPaintGridPos;
    auto gridDelta = Vec2{gridPos.X - lastGridPos.X, gridPos.Y - lastGridPos.Y};
    int32_t steps = std::max(std::abs(gridDelta.X), std::abs(gridDelta.Y));

    if (steps == 0) {
        if (m_BufferedPaintPoints.empty() ||
            m_BufferedPaintPoints.back() != gridPos) {
            m_BufferedPaintPoints.push_back(gridPos);
        }
    } else {
        for (int32_t i = 0; i <= steps; i++) {
            Vec2 point = {lastGridPos.X + (gridDelta.X * i) / steps,
                          lastGridPos.Y + (gridDelta.Y * i) / steps};

            if (!m_Model.InBounds(point)) {
                continue;
            }
            if (m_BufferedPaintPoints.empty() ||
                m_BufferedPaintPoints.back() != point) {
                m_BufferedPaintPoints.push_back(point);
            }
        }
    }

    m_LastPaintGridPos = gridPos;

    if (!canDispatch || m_BufferedPaintPoints.empty()) {
        return;
    }

    ExecuteEditorCommand(
        PaintStrokeCommand{.Points = std::move(m_BufferedPaintPoints),
                           .Value = m_EditorMode == EditorMode::Insert,
                           .BeginStroke = m_BeginPaintStroke},
        {.CursorPos = gridPos,
         .PrimaryMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left)});
    m_BeginPaintStroke = false;
    m_BufferedPaintPoints.clear();
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
