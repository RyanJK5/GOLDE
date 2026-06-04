#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "EditorCommandExecutor.hpp"
#include "EditorModel.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "SimulationCommand.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace Golde {

namespace {

bool ShouldExecuteInline(const SimulationCommand& cmd) {
    // Simulation lifecycle
    if (std::holds_alternative<StartCommand>(cmd) ||
        std::holds_alternative<PauseCommand>(cmd) ||
        std::holds_alternative<ResumeCommand>(cmd) ||
        std::holds_alternative<StepCommand>(cmd) ||
        std::holds_alternative<ClearCommand>(cmd) ||
        std::holds_alternative<ResetCommand>(cmd) ||
        std::holds_alternative<RestartCommand>(cmd)) {
        return true;
    }

    // History
    if (std::holds_alternative<UndoCommand>(cmd) ||
        std::holds_alternative<RedoCommand>(cmd)) {
        return true;
    }

    // Rule changes are cheap and need VersionManager immediately.
    if (std::holds_alternative<RuleCommand>(cmd)) {
        return true;
    }

    // SelectAll is instant.
    if (const auto* selection = std::get_if<SelectionCommand>(&cmd)) {
        return selection->Action == SelectionAction::SelectAll;
    }

    // Camera commands have no side effects on state.
    if (std::holds_alternative<CameraPositionCommand>(cmd) ||
        std::holds_alternative<CameraZoomCommand>(cmd)) {
        return true;
    }

    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

EditorModel::EditorModel(uint32_t id, const std::filesystem::path& path,
                         Size2 gridSize)
    : m_InitialGrid(m_LifeCache), m_Executor(m_LifeCache, id, path, gridSize),
      m_Worker(std::make_unique<SimulationWorker>()) {
    // Seed history with the initial state so first undo restores correctly.
    m_VersionManager.PushChange(VersionState{.Universe = m_Executor.Grid()});
    m_VersionManager.Save();
}

bool EditorModel::operator==(const EditorModel& other) const {
    return m_Executor.EditorID() == other.m_Executor.EditorID();
}

// ---------------------------------------------------------------------------
// Settings / per-frame
// ---------------------------------------------------------------------------

void EditorModel::ApplySettings(const SimulationSettings& settings) {
    m_Worker->SetTickDelayMs(settings.TickDelayMs);
    m_Worker->SetStepCount(settings.StepCount);
}

void EditorModel::CheckStopStep() {
    if (!m_StopStepCommand.load(std::memory_order_acquire))
        return;
    m_StopStepCommand.store(false, std::memory_order_release);

    const auto ogState = m_Executor.State();
    m_Executor.SetState(SimulationState::Simulation);
    if (ogState == SimulationState::Stepping) {
        StopSimulation(true);
        m_Executor.SetState(SimulationState::Paused);
    } else {
        StopSimulation(false);
        m_Executor.SetState(ogState);
    }
}

// ---------------------------------------------------------------------------
// Direct-access methods (called by SimulationEditor outside command dispatch)
// ---------------------------------------------------------------------------

bool EditorModel::UpdateSelectionAreaTracked(Vec2 gridPos) {
    if (IsEditBusy())
        return false;
    std::vector<VersionState> changes;
    const bool result = m_Executor.UpdateSelectionAreaTracked(gridPos, changes);
    for (auto& change : changes)
        m_VersionManager.PushChange(change);
    return result;
}

void EditorModel::TryResetSelection() { m_Executor.TryResetSelection(); }

void EditorModel::BeginPaintChange() {
    m_Executor.BeginPaintChange(m_VersionManager);
}

void EditorModel::PaintCell(Vec2 pos, bool value) {
    m_Executor.PaintCell(pos, value, m_VersionManager);
}

void EditorModel::MarkSaved() { m_Executor.MarkSaved(m_VersionManager); }

// ---------------------------------------------------------------------------
// Simulation lifecycle (main-thread only)
// ---------------------------------------------------------------------------

SimulationState EditorModel::StartSimulation() {
    m_Worker->Start(m_Executor.Grid());
    return SimulationState::Simulation;
}

void EditorModel::StopSimulation(bool stealGrid) {
    if (m_Executor.State() == SimulationState::Simulation) {
        if (stealGrid)
            m_Executor.Grid() = m_Worker->Stop();
        else
            m_Worker->Stop();
    }
}

SimulationState EditorModel::HandleStart() {
    if (auto change = m_Executor.Deselect()) {
        m_VersionManager.PushChange(*change);
    }
    m_InitialGrid = m_Executor.Grid();
    return StartSimulation();
}

SimulationState EditorModel::HandleClear() {
    StopSimulation(false);
    // Push a version change for the pre-clear state, then clear.
    m_VersionManager.PushChange(VersionState{.Universe = m_Executor.Grid()});

    const std::string oldRuleStr{m_Executor.CurrentRuleString()};
    m_Executor.Grid() = GameGrid{m_LifeCache, m_Executor.GridSize()};
    m_Executor.Grid().SetRule(*LifeRule::Make(oldRuleStr), oldRuleStr);

    m_VersionManager.PushChange(VersionState{.Universe = m_Executor.Grid()});
    m_Executor.SetState(SimulationState::Paint);
    return SimulationState::Paint;
}

SimulationState EditorModel::HandleReset() {
    StopSimulation(false);
    m_Executor.Grid() = m_InitialGrid;
    m_Executor.SetState(SimulationState::Paint);
    return SimulationState::Paint;
}

SimulationState EditorModel::HandleRestart() {
    StopSimulation(false);
    m_Executor.Grid() = m_InitialGrid;
    return StartSimulation();
}

SimulationState EditorModel::HandlePause() {
    StopSimulation(true);
    m_Executor.SetState(SimulationState::Paused);
    return SimulationState::Paused;
}

SimulationState EditorModel::HandleResume() {
    if (auto change = m_Executor.Deselect()) {
        m_VersionManager.PushChange(*change);
    }
    const auto state = StartSimulation();
    m_Executor.SetState(state);
    return state;
}

SimulationState EditorModel::HandleStep() {
    if (m_Executor.State() == SimulationState::Paint)
        m_InitialGrid = m_Executor.Grid();
    m_Worker->Start(m_Executor.Grid(), true, [this] {
        m_StopStepCommand.store(true, std::memory_order_release);
    });
    m_Executor.SetState(SimulationState::Stepping);
    return SimulationState::Stepping;
}

SimulationState EditorModel::HandleUndo() {
    auto versionChange = m_VersionManager.Undo();
    if (versionChange)
        m_Executor.ApplyVersionChange(*versionChange);
    return m_Executor.State();
}

SimulationState EditorModel::HandleRedo() {
    auto versionChange = m_VersionManager.Redo();
    if (versionChange)
        m_Executor.ApplyVersionChange(*versionChange);
    return m_Executor.State();
}

// ---------------------------------------------------------------------------
// Inline execution — simulation lifecycle, undo/redo, rule, camera, SelectAll
// ---------------------------------------------------------------------------

ExecuteCommandResult
EditorModel::ExecuteInline(const SimulationCommand& cmd,
                           const ExecuteCommandContext& context) {
    return std::visit(
        Overloaded{
            [this](const StartCommand&) {
                return ExecuteCommandResult{.State = HandleStart()};
            },
            [this](const ClearCommand&) {
                return ExecuteCommandResult{.State = HandleClear()};
            },
            [this](const ResetCommand&) {
                return ExecuteCommandResult{.State = HandleReset()};
            },
            [this](const RestartCommand&) {
                return ExecuteCommandResult{.State = HandleRestart()};
            },
            [this](const PauseCommand&) {
                return ExecuteCommandResult{.State = HandlePause()};
            },
            [this](const ResumeCommand&) {
                return ExecuteCommandResult{.State = HandleResume()};
            },
            [this](const StepCommand&) {
                return ExecuteCommandResult{.State = HandleStep()};
            },
            [this, &context](const UndoCommand&) {
                if (context.PrimaryMouseDown)
                    return ExecuteCommandResult{.State = m_Executor.State()};
                return ExecuteCommandResult{.State = HandleUndo()};
            },
            [this, &context](const RedoCommand&) {
                if (context.PrimaryMouseDown)
                    return ExecuteCommandResult{.State = m_Executor.State()};
                return ExecuteCommandResult{.State = HandleRedo()};
            },
            [this](const RuleCommand& command) {
                // Rule changes go through the executor to reuse its handler,
                // but run inline so VersionManager is available immediately.
                const auto oldWidth = m_Executor.GridWidth();
                const auto oldHeight = m_Executor.GridHeight();
                auto result = m_Executor.Execute(command, {});
                for (auto& change : result.VersionChanges)
                    m_VersionManager.PushChange(change);
                result.VersionChanges.clear();
                result.RecenterCameraToGridCenter =
                    m_Executor.GridWidth() != oldWidth ||
                    m_Executor.GridHeight() != oldHeight;
                return result;
            },
            [this](const CameraPositionCommand& command) {
                return ExecuteCommandResult{.State = m_Executor.State(),
                                            .CameraPositionCell =
                                                command.Position};
            },
            [this](const CameraZoomCommand& command) {
                return ExecuteCommandResult{.State = m_Executor.State(),
                                            .CameraZoom = command.Zoom};
            },
            [this](const SelectionCommand& command) {
                // Only SelectAll reaches here via ShouldExecuteInline.
                auto result = m_Executor.Execute(command, {});
                for (auto& change : result.VersionChanges)
                    m_VersionManager.PushChange(change);
                result.VersionChanges.clear();
                return result;
            },
            // All other commands are handled async — these arms are
            // unreachable from ExecuteInline but required for exhaustiveness.
            [this](const auto&) {
                return ExecuteCommandResult{.State = m_Executor.State()};
            }},
        cmd);
}

// ---------------------------------------------------------------------------
// Async result application
// ---------------------------------------------------------------------------

void EditorModel::ApplyAsyncResult(AsyncCommandResult&& asyncResult) {
    m_Executor = std::move(asyncResult.UpdatedExecutor);
    for (auto& change : asyncResult.Result.VersionChanges)
        m_VersionManager.PushChange(change);
    asyncResult.Result.VersionChanges.clear();
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------

bool EditorModel::TryStartCommand(const SimulationCommand& cmd,
                                  const ExecuteCommandContext& context) {
    if (m_EditBusy.exchange(true, std::memory_order_acq_rel))
        return false;

    std::scoped_lock lock{m_CommandMutex};

    if (ShouldExecuteInline(cmd)) {
        m_InlineCommandResult = ExecuteInline(cmd, context);
        return true;
    }

    // Snapshot the executor for the async thread.
    auto executorCopy = m_Executor;
    m_InFlightCommand = std::async(
        std::launch::async,
        [exec = std::move(executorCopy), command = cmd,
         commandContext = context]() mutable -> AsyncCommandResult {
            auto result = exec.Execute(command, commandContext);
            return AsyncCommandResult{.UpdatedExecutor = std::move(exec),
                                      .Result = std::move(result)};
        });
    return true;
}

std::optional<ExecuteCommandResult> EditorModel::PollCommandResult() {
    std::scoped_lock lock{m_CommandMutex};

    if (m_InlineCommandResult) {
        auto result = std::move(*m_InlineCommandResult);
        m_InlineCommandResult.reset();
        m_EditBusy.store(false, std::memory_order_release);
        return result;
    }

    if (!m_InFlightCommand)
        return std::nullopt;

    if (m_InFlightCommand->wait_for(std::chrono::seconds{0}) !=
        std::future_status::ready) {
        return std::nullopt;
    }

    auto asyncResult = m_InFlightCommand->get();
    m_InFlightCommand.reset();
    m_EditBusy.store(false, std::memory_order_release);

    auto publicResult = asyncResult.Result; // copy before move
    ApplyAsyncResult(std::move(asyncResult));
    return publicResult;
}

// ---------------------------------------------------------------------------
// Work state / dispatch guards
// ---------------------------------------------------------------------------

EditWorkState EditorModel::WorkState() const {
    if (m_EditBusy.load(std::memory_order_acquire))
        return EditWorkState::Working;
    return EditWorkState::Idle;
}

bool EditorModel::IsEditBusy() const {
    return WorkState() == EditWorkState::Working;
}

EditDispatchResult EditorModel::CanDispatchEdit() const {
    if (m_Executor.State() == SimulationState::Simulation ||
        m_Executor.State() == SimulationState::Stepping) {
        return {.Accepted = false,
                .RejectedReason = EditRejectReason::SimulationRunning};
    }
    if (IsEditBusy()) {
        return {.Accepted = false, .RejectedReason = EditRejectReason::Busy};
    }
    return {.Accepted = true, .RejectedReason = std::nullopt};
}

bool EditorModel::IsMutatingCommand(const SimulationCommand& cmd) const {
    return std::visit(
        Overloaded{[](const StartCommand&) { return false; },
                   [](const PauseCommand&) { return false; },
                   [](const ResumeCommand&) { return false; },
                   [](const StepCommand&) { return false; },
                   [](const CameraPositionCommand&) { return false; },
                   [](const CameraZoomCommand&) { return false; },
                   [](const SaveCommand&) { return false; },
                   [](const SaveAsNewCommand&) { return false; },
                   [](const NewFileCommand&) { return false; },
                   [](const CloseCommand&) { return false; },
                   [](const ClearCommand&) { return true; },
                   [](const ResetCommand&) { return true; },
                   [](const RestartCommand&) { return true; },
                   [](const SelectionBoundsCommand&) { return true; },
                   [](const GenerateNoiseCommand&) { return true; },
                   [](const UndoCommand&) { return true; },
                   [](const RedoCommand&) { return true; },
                   [](const LoadCommand&) { return true; },
                   [](const RuleCommand&) { return true; },
                   [](const PaintStrokeCommand&) { return true; },
                   [](const SelectionCommand&) { return true; }},
        cmd);
}

bool EditorModel::CanDispatchMutatingCommand(
    const SimulationCommand& cmd) const {
    if (std::holds_alternative<ClearCommand>(cmd) ||
        std::holds_alternative<ResetCommand>(cmd) ||
        std::holds_alternative<RestartCommand>(cmd)) {
        return true;
    }
    if (!IsMutatingCommand(cmd))
        return true;
    return CanDispatchEdit().Accepted;
}

} // namespace Golde