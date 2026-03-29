#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>

#include "EditorModel.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace gol {

EditorModel::EditorModel(uint32_t id, const std::filesystem::path& path,
                         Size2 gridSize)
    : m_Grid(gridSize), m_Worker(std::make_unique<SimulationWorker>()),
      m_CurrentFilePath(path), m_EditorID(id) {}

bool EditorModel::operator==(const EditorModel& other) const {
    return m_EditorID == other.m_EditorID;
}

SimulationState EditorModel::StartSimulation() {
    m_Worker->Start(m_Grid);
    return SimulationState::Simulation;
}

void EditorModel::StopSimulation(bool stealGrid) {
    if (m_State == SimulationState::Simulation) {
        if (stealGrid)
            m_Grid = m_Worker->Stop();
        else
            m_Worker->Stop();
    }
}

void EditorModel::CheckStopStep() {
    if (!m_StopStepCommand)
        return;
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

void EditorModel::ApplySettings(const SimulationSettings& settings) {
    m_Worker->SetTickDelayMs(settings.TickDelayMs);
    m_Worker->SetStepCount(settings.StepCount);
    m_Grid.SetAlgorithm(settings.Algorithm);
}

SimulationState EditorModel::HandleStart() {
    m_SelectionManager.Deselect(m_Grid);
    m_InitialGrid = m_Grid;
    return StartSimulation();
}

SimulationState EditorModel::HandleClear() {
    StopSimulation(false);
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    m_VersionManager.PushChange({.Action = GameAction::Clear,
                                 .SelectionBounds = m_InitialGrid.BoundingBox(),
                                 .CellsInserted = {},
                                 .CellsDeleted = m_InitialGrid.Data()});
    m_Grid = GameGrid{m_Grid.Size()};
    return SimulationState::Paint;
}

SimulationState EditorModel::HandleReset() {
    StopSimulation(false);
    m_SelectionManager.Deselect(m_Grid);
    m_Grid = m_InitialGrid;
    return SimulationState::Paint;
}

SimulationState EditorModel::HandleRestart() {
    StopSimulation(false);
    m_SelectionManager.Deselect(m_Grid);
    m_Grid = m_InitialGrid;
    return StartSimulation();
}

SimulationState EditorModel::HandlePause() {
    StopSimulation(true);
    return SimulationState::Paused;
}

SimulationState EditorModel::HandleResume() {
    m_SelectionManager.Deselect(m_Grid);
    return StartSimulation();
}

SimulationState EditorModel::HandleStep() {
    m_SelectionManager.Deselect(m_Grid);
    if (m_State == SimulationState::Paint)
        m_InitialGrid = m_Grid;
    m_Worker->Start(m_Grid, true, [this] { m_StopStepCommand = true; });
    return SimulationState::Stepping;
}

SimulationState EditorModel::HandleResize(Size2 newDimensions) {
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
    return SimulationState::Paint;
}

SimulationState EditorModel::HandleGenerateNoise(float density) {
    if (!m_SelectionManager.CanDrawGrid())
        return m_State;
    const auto selectionBounds = m_SelectionManager.SelectionBounds();
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    m_VersionManager.TryPushChange(
        m_SelectionManager.InsertNoise(selectionBounds, density));
    return m_State;
}

SimulationState EditorModel::HandleUndo() {
    auto versionChanges = m_VersionManager.Undo();
    if (versionChanges)
        m_SelectionManager.HandleVersionChange(EditorAction::Undo, m_Grid,
                                               *versionChanges);
    return m_State;
}

SimulationState EditorModel::HandleRedo() {
    auto versionChanges = m_VersionManager.Redo();
    if (versionChanges)
        m_SelectionManager.HandleVersionChange(EditorAction::Redo, m_Grid,
                                               *versionChanges);
    return m_State;
}

void EditorModel::HandleSelectionAction(SelectionAction action,
                                        int32_t nudgeSize) {
    if (action == SelectionAction::SelectAll)
        m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    m_VersionManager.TryPushChange(
        m_SelectionManager.HandleAction(action, m_Grid, nudgeSize));
}

std::optional<std::string>
EditorModel::LoadFile(const std::filesystem::path& path) {
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    auto loadResult = m_SelectionManager.Load(path);
    if (loadResult) {
        m_VersionManager.PushChange(*loadResult);
        return std::nullopt;
    }
    return loadResult.error();
}

bool EditorModel::SaveToFile(const std::filesystem::path& path,
                             bool markAsSaved) {
    if (m_SelectionManager.Save(m_Grid, path)) {
        if (m_CurrentFilePath.empty())
            m_CurrentFilePath = path;
        if (markAsSaved)
            m_VersionManager.Save();
        return true;
    }
    return false;
}

PasteResult EditorModel::PasteSelection(std::optional<Vec2> cursorPos) {
    if (cursorPos || m_SelectionManager.CanDrawGrid()) {
        m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    }
    auto pasteResult = m_SelectionManager.Paste(cursorPos, 100'000'000U);
    if (pasteResult) {
        m_VersionManager.PushChange(*pasteResult);
        return {.Value = PasteResult::Status::Success};
    }
    if (pasteResult.error().has_value())
        return {.Value = PasteResult::Status::TooLarge,
                .CellCount = *pasteResult.error()};
    return {.Value = PasteResult::Status::ClipboardError};
}

void EditorModel::ForcePaste(std::optional<Vec2> cursorPos) {
    auto pasteResult = m_SelectionManager.Paste(
        cursorPos, std::numeric_limits<uint32_t>::max());
    if (pasteResult)
        m_VersionManager.PushChange(*pasteResult);
}

void EditorModel::InsertFromClipboard(Vec2 position) {
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid));
    auto result = m_SelectionManager.Paste(
        position, std::numeric_limits<uint32_t>::max(), true);
    if (result)
        m_VersionManager.PushChange(*result);
}

SimulationState EditorModel::SetSelectionBounds(Rect bounds) {
    auto [change1, change2] =
        m_SelectionManager.ModifySelectionBounds(m_Grid, bounds);
    m_VersionManager.TryPushChange(change1);
    m_VersionManager.TryPushChange(change2);

    return m_State;
}

bool EditorModel::UpdateSelectionAreaTracked(Vec2 gridPos) {
    auto result = m_SelectionManager.UpdateSelectionArea(m_Grid, gridPos);
    m_VersionManager.TryPushChange(result.Change);
    return result.BeginSelection;
}

void EditorModel::TryResetSelection() {
    m_SelectionManager.TryResetSelection();
}

void EditorModel::BeginPaintChange(Vec2 pos, bool insert) {
    m_VersionManager.BeginPaintChange(pos, insert);
}

void EditorModel::PaintCell(Vec2 pos, bool value) {
    if (*m_Grid.Get(pos.X, pos.Y) != value)
        m_VersionManager.AddPaintChange(pos);
    m_Grid.Set(pos.X, pos.Y, value);
}

void EditorModel::MarkSaved() { m_VersionManager.Save(); }

} // namespace gol
