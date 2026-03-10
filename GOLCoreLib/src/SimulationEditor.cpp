#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <glm/fwd.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

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
    : m_Grid(gridSize),
      m_Graphics(std::filesystem::path("resources") / "shader",
                 windowSize.Width, windowSize.Height, {0.1f, 0.1f, 0.1f, 1.f}),
      m_FileErrorWindow("File Error", [](auto) {}),
      m_PasteWarning(
          "Paste Warning",
          std::bind_front(&SimulationEditor::PasteWarnUpdated, this)),
      m_SaveWarning("Save Warning", [](auto) {}), m_CurrentFilePath(path),
      m_Worker(std::make_unique<SimulationWorker>()), m_EditorID(id) {}

bool SimulationEditor::IsSaved() const { return m_VersionManager.IsSaved(); }

bool SimulationEditor::operator==(const SimulationEditor& other) const {
    return m_EditorID == other.m_EditorID;
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
                            .GridSize = m_Grid.Size(),
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
        m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
        auto result = m_SelectionManager.Paste(
            Vec2{0, 0}, std::numeric_limits<uint32_t>::max(), true);
        if (result)
            m_VersionManager.PushChange(*result);
    }

    if (m_StopStepCommand) {
        m_StopStepCommand = false;
        const auto ogState = m_State;
        m_State = SimulationState::Simulation;
        if (ogState == SimulationState::Stepping) {
            StopSimulation(true);
            m_State = SimulationState::Paused;
        } else {
            StopSimulation(false);
            m_State = ogState;
        }
    }

    m_Worker->SetTickDelayMs(controlArgs.Settings.TickDelayMs);
    m_Worker->SetStepCount(controlArgs.Settings.StepCount);
    m_Grid.SetAlgorithm(controlArgs.Settings.Algorithm);

    if (controlArgs.Command &&
        ((activeOverride && *activeOverride) || displayResult.Selected))
        m_State = UpdateState(controlArgs);

    m_State = [this, &graphicsArgs]() {
        switch (m_State) {
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
    }();

    return {
        .Simulation = {.State = m_State},
        .Editing = {.SelectionActive = m_SelectionManager.CanDrawGrid(),
                    .UndosAvailable = m_VersionManager.UndosAvailable(),
                    .RedosAvailable = m_VersionManager.RedosAvailable()},
        .File = {.CurrentFilePath = m_CurrentFilePath,
                 .HasUnsavedChanges = !m_VersionManager.IsSaved()},
        .Active = (activeOverride && *activeOverride) || displayResult.Selected,
        .Closing = displayResult.Closing ||
                   (controlArgs.Command && std::holds_alternative<CloseCommand>(
                                               *controlArgs.Command))};
}

void SimulationEditor::DrawHashLifeData(const HashQuadtree& quadtree,
                                        const GraphicsHandlerArgs& args) {
    const auto viewBounds = args.ViewportBounds;
    const auto topLeftWorld = m_Graphics.Camera.ScreenToWorldPos(
        Vec2F{static_cast<float>(viewBounds.X),
              static_cast<float>(viewBounds.Y)},
        viewBounds);
    const auto bottomRightWorld = m_Graphics.Camera.ScreenToWorldPos(
        Vec2F{static_cast<float>(viewBounds.X + viewBounds.Width),
              static_cast<float>(viewBounds.Y + viewBounds.Height)},
        viewBounds);
    const auto [minWorldX, maxWorldX] =
        std::minmax(topLeftWorld.x, bottomRightWorld.x);
    const auto [minWorldY, maxWorldY] =
        std::minmax(topLeftWorld.y, bottomRightWorld.y);
    const auto minCellX =
        static_cast<int32_t>(std::floor(minWorldX / args.CellSize.Width));
    const auto minCellY =
        static_cast<int32_t>(std::floor(minWorldY / args.CellSize.Height));
    const auto maxCellX =
        static_cast<int32_t>(std::ceil(maxWorldX / args.CellSize.Width));
    const auto maxCellY =
        static_cast<int32_t>(std::ceil(maxWorldY / args.CellSize.Height));
    const auto visibleBounds =
        Rect{minCellX, minCellY, std::max(0, maxCellX - minCellX),
             std::max(0, maxCellY - minCellY)};

    const auto visibleRange =
        std::ranges::subrange(quadtree.begin(visibleBounds), quadtree.end());
    m_Graphics.DrawGrid({0, 0}, visibleRange, args);
}

SimulationState
SimulationEditor::SimulationUpdate(const GraphicsHandlerArgs& args) {
    const auto snapshot = m_Worker->GetResult();

    const auto data = snapshot->IterableData();
    if (std::holds_alternative<std::reference_wrapper<const LifeHashSet>>(data))
        m_Graphics.DrawGrid(
            {0, 0},
            std::get<std::reference_wrapper<const LifeHashSet>>(data).get(),
            args);
    else
        DrawHashLifeData(
            std::get<std::reference_wrapper<const HashQuadtree>>(data).get(),
            args);
    return SimulationState::Simulation;
}

SimulationState SimulationEditor::PaintUpdate(const GraphicsHandlerArgs& args) {
    auto gridPos = CursorGridPos();

    m_Graphics.DrawGrid({0, 0}, m_Grid.Data(), args);
    if (m_SelectionManager.CanDrawGrid())
        m_Graphics.DrawGrid(m_SelectionManager.SelectionBounds().UpperLeft(),
                            m_SelectionManager.GridData(), args);
    if (m_SelectionManager.CanDrawGrid())
        m_Graphics.DrawSelection(m_SelectionManager.SelectionBounds(), args);

    if (gridPos) {
        if (m_SelectionManager.CanDrawSelection())
            m_Graphics.DrawSelection(m_SelectionManager.SelectionBounds(),
                                     args);
        UpdateMouseState(*gridPos);
    } else {
        m_SelectionManager.TryResetSelection();
    }
    m_LeftDeltaLast = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

    return (m_Grid.Dead() && !m_SelectionManager.GridAlive())
               ? SimulationState::Empty
               : SimulationState::Paint;
}

SimulationState SimulationEditor::PauseUpdate(const GraphicsHandlerArgs& args) {
    auto gridPos = CursorGridPos();
    if (gridPos)
        m_VersionManager.TryPushChange(
            m_SelectionManager.UpdateSelectionArea(m_Grid, *gridPos).Change);

    const auto data = m_Grid.IterableData();
    if (std::holds_alternative<std::reference_wrapper<const LifeHashSet>>(data))
        m_Graphics.DrawGrid(
            {0, 0},
            std::get<std::reference_wrapper<const LifeHashSet>>(data).get(),
            args);
    else
        DrawHashLifeData(
            std::get<std::reference_wrapper<const HashQuadtree>>(data).get(),
            args);

    if (m_SelectionManager.CanDrawSelection())
        m_Graphics.DrawSelection(m_SelectionManager.SelectionBounds(), args);
    if (m_SelectionManager.CanDrawGrid())
        m_Graphics.DrawGrid(m_SelectionManager.SelectionBounds().UpperLeft(),
                            m_SelectionManager.GridData(), args);
    return m_State;
}

SimulationEditor::DisplayResult
SimulationEditor::DisplaySimulation(bool grabFocus) {
    auto label = std::format(
        "{}{}###Simulation{}",
        m_CurrentFilePath.empty()
            ? "(untitled)"
            : m_CurrentFilePath.filename().string().c_str(),
        (!m_CurrentFilePath.empty() && !m_VersionManager.IsSaved()) ? "*" : "",
        m_EditorID);

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
                 ImGui::GetContentRegionAvail(), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::SetCursorPosY(0);
    ImGui::InvisibleButton("##SimulationViewport",
                           ImGui::GetContentRegionAvail());

    if (grabFocus || (m_TakeKeyboardInput && !ImGui::IsItemFocused() &&
                      ImGui::IsKeyPressed(ImGuiKey_Escape)))
        ImGui::SetKeyboardFocusHere(-1);

    m_TakeKeyboardInput = ImGui::IsItemFocused() || windowFocused;
    m_TakeMouseInput = ImGui::IsItemHovered();

    if ((m_State == SimulationState::Simulation ||
         m_State == SimulationState::Stepping) &&
        m_Worker->GetTimeSinceLastUpdate() > std::chrono::seconds{5}) {
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

    const auto snapshot = m_Worker->GetResult();
    const auto generation =
        snapshot ? snapshot->Generation() : m_Grid.Generation();
    const auto population =
        snapshot ? snapshot->Population() : m_Grid.Population();
    ImGui::Text(
        "%s",
        std::format(std::locale(""), "Generation: {:L}", generation).c_str());
    ImGui::Text(
        "%s", std::format(std::locale(""), "Population: {:L}",
                          population + m_SelectionManager.SelectedPopulation())
                  .c_str());

    if (m_SelectionManager.CanDrawSelection()) {
        const auto pos = m_SelectionManager.SelectionBounds().UpperLeft();
        auto text = std::format("({}, {})", pos.X, pos.Y);
        if (m_SelectionManager.CanDrawLargeSelection()) {
            const auto sentinel =
                m_SelectionManager.SelectionBounds().LowerRight();
            text += std::format(" X ({}, {})", sentinel.X, sentinel.Y);
        }
        ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y -
                             ImGui::CalcTextSize(text.c_str()).y);
        ImGui::Text("%s", text.c_str());
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

    glm::vec2 vec =
        m_Graphics.Camera.ScreenToWorldPos(screenPos, ViewportBounds());
    vec /= glm::vec2{DefaultCellWidth, DefaultCellHeight};

    Vec2 result = {static_cast<int32_t>(std::floor(vec.x)),
                   static_cast<int32_t>(std::floor(vec.y))};
    if (!m_Grid.InBounds(result))
        return std::nullopt;
    return result;
}

std::optional<Vec2> SimulationEditor::CursorGridPos() {
    return ConvertToGridPos(ImGui::GetMousePos());
}

SimulationState
SimulationEditor::UpdateState(const SimulationControlResult& result) {
    if (result.FromShortcut && !m_TakeKeyboardInput)
        return m_State;

    return std::visit(
        overloaded{
            [this](const StartCommand&) -> SimulationState {
                m_SelectionManager.Deselect(m_Grid);
                m_InitialGrid = m_Grid;
                return StartSimulation();
            },
            [this](const ClearCommand&) -> SimulationState {
                StopSimulation(false);
                m_VersionManager.TryPushChange(
                    m_SelectionManager.Deselect(m_Grid));
                m_VersionManager.PushChange(
                    {.Action = GameAction::Clear,
                     .SelectionBounds = m_InitialGrid.BoundingBox(),
                     .CellsInserted = {},
                     .CellsDeleted = m_InitialGrid.Data()});
                m_Grid = GameGrid{m_Grid.Size()};
                return SimulationState::Paint;
            },
            [this](const ResetCommand&) -> SimulationState {
                StopSimulation(false);
                m_SelectionManager.Deselect(m_Grid);
                m_Grid = m_InitialGrid;
                return SimulationState::Paint;
            },
            [this](const RestartCommand&) -> SimulationState {
                StopSimulation(false);
                m_SelectionManager.Deselect(m_Grid);
                m_Grid = m_InitialGrid;
                return StartSimulation();
            },
            [this](const PauseCommand&) -> SimulationState {
                StopSimulation(true);
                return SimulationState::Paused;
            },
            [this](const ResumeCommand&) -> SimulationState {
                m_SelectionManager.Deselect(m_Grid);
                return StartSimulation();
            },
            [this](const StepCommand&) -> SimulationState {
                m_SelectionManager.Deselect(m_Grid);
                if (m_State == SimulationState::Paint)
                    m_InitialGrid = m_Grid;
                m_Worker->Start(m_Grid, true,
                                [this] { m_StopStepCommand = true; });
                return SimulationState::Stepping;
            },
            [this](const ResizeCommand& cmd) -> SimulationState {
                return ResizeGrid(cmd.NewDimensions);
            },
            [this](const GenerateNoiseCommand& cmd) -> SimulationState {
                if (!m_SelectionManager.CanDrawGrid())
                    return m_State;
                const auto selectionBounds =
                    m_SelectionManager.SelectionBounds();
                m_VersionManager.TryPushChange(
                    m_SelectionManager.Deselect(m_Grid));
                m_VersionManager.TryPushChange(m_SelectionManager.InsertNoise(
                    selectionBounds, cmd.Density));
                return m_State;
            },
            [this](const UndoCommand&) -> SimulationState {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    return m_State;
                auto versionChanges = m_VersionManager.Undo();
                if (versionChanges)
                    m_SelectionManager.HandleVersionChange(
                        EditorAction::Undo, m_Grid, *versionChanges);
                return m_State;
            },
            [this](const RedoCommand&) -> SimulationState {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    return m_State;
                auto versionChanges = m_VersionManager.Redo();
                if (versionChanges)
                    m_SelectionManager.HandleVersionChange(
                        EditorAction::Redo, m_Grid, *versionChanges);
                return m_State;
            },
            [this](const SaveCommand& cmd) -> SimulationState {
                if (m_Grid.Population() > 10'000'000) {
                    m_SaveWarning.SetCallback(
                        [this, path = cmd.FilePath](PopupWindowState state) {
                            if (state != PopupWindowState::Success)
                                return;
                            SaveToFile(path, true);
                        });
                    m_SaveWarning.Activate();
                    m_SaveWarning.Message = std::format(
                        std::locale(""),
                        "This file has {:L} total cells. The saved file will "
                        "be\n"
                        "large and may take a long time to save. Are you sure\n"
                        "you want to continue?",
                        m_Grid.Population());
                    return m_State;
                }
                SaveToFile(cmd.FilePath, true);
                return m_State;
            },
            [this](const SaveAsNewCommand& cmd) -> SimulationState {
                if (m_Grid.Population() > 10'000'000) {
                    m_SaveWarning.SetCallback(
                        [this, path = cmd.FilePath](PopupWindowState state) {
                            if (state != PopupWindowState::Success)
                                return;
                            SaveToFile(path, false);
                        });
                    m_SaveWarning.Activate();
                    m_SaveWarning.Message = std::format(
                        std::locale(""),
                        "This file has {:L} total cells. The saved file will "
                        "be\n"
                        "large and may take a long time to save. Are you sure\n"
                        "you want to continue?",
                        m_Grid.Population());
                    return m_State;
                }
                SaveToFile(cmd.FilePath, false);
                return m_State;
            },
            [this](const LoadCommand& cmd) -> SimulationState {
                LoadFile(cmd.FilePath);
                m_VersionManager.Save();
                return m_State;
            },
            [this](const NewFileCommand&) -> SimulationState {
                return m_State;
            },
            [this](const CloseCommand&) -> SimulationState { return m_State; },
            [this](const SelectionCommand& cmd) -> SimulationState {
                if (cmd.Action == SelectionAction::Paste) {
                    PasteSelection();
                    return m_State;
                }
                if (cmd.Action == SelectionAction::SelectAll)
                    m_VersionManager.TryPushChange(
                        m_SelectionManager.Deselect(m_Grid));
                m_VersionManager.TryPushChange(m_SelectionManager.HandleAction(
                    cmd.Action, m_Grid, cmd.NudgeSize));
                return m_State;
            }},
        *result.Command);
}

SimulationState SimulationEditor::StartSimulation() {
    m_Worker->Start(m_Grid);
    return SimulationState::Simulation;
}

void SimulationEditor::StopSimulation(bool stealGrid) {
    if (m_State == SimulationState::Simulation) {
        if (stealGrid)
            m_Grid = m_Worker->Stop();
        else
            m_Worker->Stop();
    }
}

void SimulationEditor::PasteWarnUpdated(PopupWindowState state) {
    if (state != PopupWindowState::Success)
        return;

    auto pasteResult = m_SelectionManager.Paste(
        CursorGridPos(), std::numeric_limits<uint32_t>::max());
    if (pasteResult)
        m_VersionManager.PushChange(*pasteResult);
}

void SimulationEditor::PasteSelection() {
    auto gridPos = CursorGridPos();
    if (gridPos)
        m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    auto pasteResult = m_SelectionManager.Paste(gridPos, 100000000U);
    if (pasteResult)
        m_VersionManager.PushChange(*pasteResult);
    else if (pasteResult.error().has_value()) {
        m_PasteWarning.Activate();
        m_PasteWarning.Message =
            std::format(std::locale(""),
                        "Your selection ({:L} cells) is too large\n"
                        "to paste without potential performance issues.\n"
                        "Are you sure you want to continue?",
                        *pasteResult.error());
    } else {
        m_FileErrorWindow.Activate();
        m_FileErrorWindow.Message = "Failed to read from clipboard.";
    }
}

void SimulationEditor::LoadFile(const std::filesystem::path& path) {
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));

    auto loadResult = m_SelectionManager.Load(path);
    if (loadResult)
        m_VersionManager.PushChange(*loadResult);
    else {
        m_FileErrorWindow.Activate();
        m_FileErrorWindow.Message =
            std::format("Failed to load file:\n{}", loadResult.error());
    }
}

void SimulationEditor::SaveToFile(const std::filesystem::path& path,
                                  bool markAsSaved) {
    if (m_SelectionManager.Save(m_Grid, path)) {
        if (m_CurrentFilePath.empty())
            m_CurrentFilePath = path;
        if (markAsSaved)
            m_VersionManager.Save();
    } else {
        m_FileErrorWindow.Activate();
        m_FileErrorWindow.Message =
            std::format("Failed to save file to \n{}", path.string());
    }
}

SimulationState SimulationEditor::ResizeGrid(Size2 newDimensions) {
    if (newDimensions.Width == m_Grid.Width() &&
        newDimensions.Height == m_Grid.Height())
        return SimulationState::Paint;
    if ((newDimensions.Width == 0 || newDimensions.Height == 0) &&
        (m_Grid.Width() == 0 || m_Grid.Height() == 0))
        return SimulationState::Paint;

    m_VersionManager.PushChange({.Action = EditorAction::Resize,
                                 .GridResize = {{m_Grid, newDimensions}}});

    m_Grid = GameGrid{std::move(m_Grid), newDimensions};
    if (m_SelectionManager.CanDrawSelection()) {
        auto selection = m_SelectionManager.SelectionBounds();
        if (!m_Grid.InBounds(selection.UpperLeft()) ||
            !m_Grid.InBounds(selection.UpperRight()) ||
            !m_Grid.InBounds(selection.LowerLeft()) ||
            !m_Grid.InBounds(selection.LowerRight()))
            m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    }
    m_Graphics.Camera.Center = {newDimensions.Width * DefaultCellWidth / 2.f,
                                newDimensions.Height * DefaultCellHeight / 2.f};
    return SimulationState::Paint;
}

void SimulationEditor::UpdateMouseState(Vec2 gridPos) {
    if (!m_TakeMouseInput ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        return;

    auto result = m_SelectionManager.UpdateSelectionArea(m_Grid, gridPos);
    if (result.BeginSelection) {
        m_EditorMode = EditorMode::Select;
        return;
    }
    m_VersionManager.TryPushChange(result.Change);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !ImGui::IsKeyDown(ImGuiKey_LeftShift) &&
        !ImGui::IsKeyDown(ImGuiKey_RightShift)) {
        if (m_EditorMode == EditorMode::None ||
            m_EditorMode == EditorMode::Select) {
            m_EditorMode = *m_Grid.Get(gridPos.X, gridPos.Y)
                               ? EditorMode::Delete
                               : EditorMode::Insert;
            m_VersionManager.BeginPaintChange(gridPos, m_EditorMode ==
                                                           EditorMode::Insert);
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
        if (*m_Grid.Get(currentGridPos->X, currentGridPos->Y) !=
            (m_EditorMode == EditorMode::Insert))
            m_VersionManager.AddPaintChange(*currentGridPos);
        m_Grid.Set(currentGridPos->X, currentGridPos->Y,
                   m_EditorMode == EditorMode::Insert);
        return;
    }

    for (int32_t i = 0; i <= steps; i++) {
        Vec2 gridPos = {lastGridPos->X + (gridDelta.X * i) / steps,
                        lastGridPos->Y + (gridDelta.Y * i) / steps};

        if (!m_Grid.InBounds(gridPos))
            continue;

        if (*m_Grid.Get(gridPos.X, gridPos.Y) !=
            (m_EditorMode == EditorMode::Insert))
            m_VersionManager.AddPaintChange(gridPos);
        m_Grid.Set(gridPos.X, gridPos.Y, m_EditorMode == EditorMode::Insert);
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