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
      m_CurrentFilePath(path), m_EditorID(id) {
    // Seed history with the initial state so first undo restores correctly.
    m_VersionManager.PushChange(VersionState{.Universe = m_Grid});
    m_VersionManager.Save();
}

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
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid),
                                   m_State);
    m_Grid = GameGrid{m_Grid.Size()};
    m_VersionManager.TryPushChange(VersionState{.Universe = m_Grid}, m_State);
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

    m_Grid = GameGrid{std::move(m_Grid), newDimensions};
    m_VersionManager.TryPushChange(VersionState{.Universe = m_Grid}, m_State);
    if (m_SelectionManager.CanDrawSelection()) {
        auto selection = m_SelectionManager.SelectionBounds();
        if (!m_Grid.InBounds(selection.UpperLeft()) ||
            !m_Grid.InBounds(selection.UpperRight()) ||
            !m_Grid.InBounds(selection.LowerLeft()) ||
            !m_Grid.InBounds(selection.LowerRight()))
            m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid),
                                           m_State);
    }
    return SimulationState::Paint;
}

SimulationState EditorModel::HandleGenerateNoise(float density) {
    if (!m_SelectionManager.CanDrawGrid())
        return m_State;
    const auto selectionBounds = m_SelectionManager.SelectionBounds();
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid),
                                   m_State);
    m_VersionManager.TryPushChange(
        m_SelectionManager.InsertNoise(m_Grid, selectionBounds, density),
        m_State);
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

bool EditorModel::HandleSelectionAction(SelectionAction action,
                                        int32_t nudgeSize) {
    if (action == SelectionAction::SelectAll)
        m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid),
                                       m_State);

    const auto actionResult =
        m_SelectionManager.HandleAction(action, m_Grid, nudgeSize);
    m_VersionManager.TryPushChange(actionResult, m_State);

    if (!actionResult &&
        (action == SelectionAction::Copy || action == SelectionAction::Cut)) {
        return false;
    } else {
        return true;
    }
}

std::optional<std::string>
EditorModel::LoadFile(const std::filesystem::path& path) {
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid),
                                   m_State);
    auto loadResult = m_SelectionManager.Load(m_Grid, path);
    if (loadResult) {
        m_VersionManager.TryPushChange(*loadResult, m_State);
        return std::nullopt;
    }
    return loadResult.error().Message;
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

std::expected<void, RLEEncoder::DecodeError>
EditorModel::PasteSelection(std::optional<Vec2> cursorPos) {
    if (cursorPos || m_SelectionManager.CanDrawGrid()) {
        m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid),
                                       m_State);
    }
    auto pasteResult =
        m_SelectionManager.Paste(m_Grid, cursorPos, 100'000'000U);
    if (pasteResult) {
        m_VersionManager.TryPushChange(*pasteResult, m_State);
        return {};
    }
    return std::unexpected{pasteResult.error()};
}

void EditorModel::ForcePaste(std::optional<Vec2> cursorPos) {
    auto pasteResult = m_SelectionManager.Paste(
        m_Grid, cursorPos, std::numeric_limits<uint32_t>::max());
    if (pasteResult)
        m_VersionManager.TryPushChange(*pasteResult, m_State);
}

void EditorModel::InsertFromClipboard(Vec2 position) {
    m_VersionManager.TryPushChange(m_SelectionManager.Deselect(m_Grid),
                                   m_State);
    auto result = m_SelectionManager.Paste(
        m_Grid, position, std::numeric_limits<uint32_t>::max(), true);
    if (result)
        m_VersionManager.TryPushChange(*result, m_State);
}

SimulationState EditorModel::SetSelectionBounds(Rect bounds) {
    auto [change1, change2] =
        m_SelectionManager.ModifySelectionBounds(m_Grid, bounds);
    m_VersionManager.TryPushChange(change1, m_State);
    m_VersionManager.TryPushChange(change2, m_State);

    return m_State;
}

bool EditorModel::UpdateSelectionAreaTracked(Vec2 gridPos) {
    auto result = m_SelectionManager.UpdateSelectionArea(m_Grid, gridPos);
    m_VersionManager.TryPushChange(result.Change, m_State);
    return result.BeginSelection;
}

void EditorModel::TryResetSelection() {
    m_SelectionManager.TryResetSelection();
}

// TODO: Make sure this works
void EditorModel::BeginPaintChange() {
    m_VersionManager.BeginPaintChange(m_Grid, m_State);
}

void EditorModel::PaintCell(Vec2 pos, bool value) {
    if (*m_Grid.Get(pos.X, pos.Y) == value) {
        return;
    }

    m_Grid.Set(pos.X, pos.Y, value);
    m_VersionManager.AddPaintChange(m_Grid, m_State);
}

void EditorModel::MarkSaved() { m_VersionManager.Save(); }

} // namespace gol
