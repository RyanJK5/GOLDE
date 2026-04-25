#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <locale>
#include <optional>
#include <string>

#include "EditorModel.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "SimulationCommand.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace gol {
namespace {
bool ShouldExecuteInline(const SimulationCommand& cmd) {
    if (std::holds_alternative<UndoCommand>(cmd) ||
        std::holds_alternative<RedoCommand>(cmd)) {
        return true;
    }

    if (const auto* selection = std::get_if<SelectionCommand>(&cmd)) {
        return selection->Action == SelectionAction::SelectAll;
    }

    return false;
}
} // namespace

EditorModel::EditorModel(uint32_t id, const std::filesystem::path& path,
                         Size2 gridSize)
    : m_Grid(gridSize), m_Worker(std::make_unique<SimulationWorker>(id)),
      m_CurrentFilePath(path), m_EditorID(id) {
    // Seed history with the initial state so first undo restores correctly.
    m_VersionManager.PushChange(VersionState{.Universe = m_Grid});
    m_VersionManager.Save();
}

bool EditorModel::operator==(const EditorModel& other) const {
    return m_EditorID == other.m_EditorID;
}

bool EditorModel::IsSimulationOutOfBounds() const {
    return m_Grid.BoundingBox() == Rect{};
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
    if (!m_StopStepCommand.load(std::memory_order_acquire))
        return;
    m_StopStepCommand.store(false, std::memory_order_release);
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
}

void EditorModel::TryPushVersionChange(
    const std::optional<VersionState>& change) {
    m_VersionManager.TryPushChange(change, m_State);
}

void EditorModel::TryPushVersionChange(const VersionState& change) {
    TryPushVersionChange(std::optional<VersionState>{change});
}

SimulationState EditorModel::HandleStart() {
    m_SelectionManager.Deselect(m_Grid);
    m_InitialGrid = m_Grid;
    return StartSimulation();
}

SimulationState EditorModel::HandleClear() {
    StopSimulation(false);
    TryPushVersionChange(m_SelectionManager.Deselect(m_Grid));

    const std::string oldRuleStr{m_Grid.GetRuleString()};
    m_Grid = GameGrid{m_Grid.Size()};
    m_Grid.SetRule(*LifeRule::Make(oldRuleStr), oldRuleStr);

    TryPushVersionChange(VersionState{.Universe = m_Grid});
    return SimulationState::Paint;
}

SimulationState EditorModel::HandleReset() {
    StopSimulation(false);
    m_SelectionManager.Deselect(m_Grid);
    m_Grid = m_InitialGrid;
    m_Worker->BufferRule(
        std::make_unique<LifeRule>(*LifeRule::Make(m_Grid.GetRuleString())));
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
    m_Worker->Start(m_Grid, true, [this] {
        m_StopStepCommand.store(true, std::memory_order_release);
    });
    return SimulationState::Stepping;
}

SimulationState EditorModel::HandleRuleChange(std::string_view ruleStr) {
    const auto rule = *LifeRule::Make(ruleStr);
    m_Worker->BufferRule(std::make_unique<LifeRule>(rule));

    const auto oldSize = m_Grid.Size();
    m_Grid = GameGrid{std::move(m_Grid),
                      rule.Bounds() ? rule.Bounds()->Size() : Size2{}};
    m_Grid.SetRule(rule, ruleStr);

    TryPushVersionChange(VersionState{.Universe = m_Grid});

    if (m_Grid.Size() == oldSize) {
        return m_State;
    }

    if (m_SelectionManager.CanDrawSelection()) {
        auto selection = m_SelectionManager.SelectionBounds();
        if (!m_Grid.InBounds(selection.UpperLeft()) ||
            !m_Grid.InBounds(selection.UpperRight()) ||
            !m_Grid.InBounds(selection.LowerLeft()) ||
            !m_Grid.InBounds(selection.LowerRight()))
            TryPushVersionChange(m_SelectionManager.Deselect(m_Grid));
    }
    return m_State;
}

bool EditorModel::HandleGenerateNoise(float density, uint32_t warnThreshold) {
    if (!m_SelectionManager.CanDrawGrid())
        return false;
    const auto selectionBounds = m_SelectionManager.SelectionBounds();
    TryPushVersionChange(m_SelectionManager.Deselect(m_Grid));

    const auto result = m_SelectionManager.InsertNoise(m_Grid, selectionBounds,
                                                       warnThreshold, density);
    if (result) {
        TryPushVersionChange(result);
        return true;
    } else {
        m_SelectionManager.ModifySelectionBounds(m_Grid, selectionBounds);
        return false;
    }
}

SimulationState EditorModel::HandleUndo() {
    auto versionChanges = m_VersionManager.Undo();
    if (versionChanges) {
        m_SelectionManager.HandleVersionChange(m_Grid, *versionChanges);
        m_Worker->BufferRule(std::make_unique<LifeRule>(
            *LifeRule::Make(m_Grid.GetRuleString())));
    }
    return m_State;
}

SimulationState EditorModel::HandleRedo() {
    auto versionChanges = m_VersionManager.Redo();
    if (versionChanges) {
        m_SelectionManager.HandleVersionChange(m_Grid, *versionChanges);
        m_Worker->BufferRule(std::make_unique<LifeRule>(
            *LifeRule::Make(m_Grid.GetRuleString())));
    }
    return m_State;
}

bool EditorModel::HandleSelectionAction(SelectionAction action,
                                        int32_t nudgeSize) {
    if (action == SelectionAction::SelectAll)
        TryPushVersionChange(m_SelectionManager.Deselect(m_Grid));

    const auto actionResult =
        m_SelectionManager.HandleAction(action, m_Grid, nudgeSize);
    TryPushVersionChange(actionResult);

    if (!actionResult && (action == SelectionAction::FlipHorizontally ||
                          action == SelectionAction::FlipVertically ||
                          action == SelectionAction::Rotate)) {
        return false;
    } else {
        return true;
    }
}

std::optional<std::string>
EditorModel::LoadFile(const std::filesystem::path& path) {
    TryPushVersionChange(m_SelectionManager.Deselect(m_Grid));
    auto loadResult = m_SelectionManager.Load(m_Grid, path);
    if (loadResult) {
        TryPushVersionChange(*loadResult);
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

std::expected<void, FileEncoder::DecodeError>
EditorModel::PasteSelection(std::optional<Vec2> cursorPos,
                            std::string_view clipboardText) {
    if (cursorPos || m_SelectionManager.CanDrawGrid()) {
        TryPushVersionChange(m_SelectionManager.Deselect(m_Grid));
    }
    auto pasteResult = m_SelectionManager.Paste(m_Grid, clipboardText,
                                                cursorPos, 100'000'000U);
    if (pasteResult) {
        TryPushVersionChange(*pasteResult);
        return {};
    }
    return std::unexpected{pasteResult.error()};
}

void EditorModel::ForcePaste(std::optional<Vec2> cursorPos,
                             std::string_view clipboardText) {
    auto pasteResult = m_SelectionManager.Paste(
        m_Grid, clipboardText, cursorPos, std::numeric_limits<uint32_t>::max());
    if (pasteResult)
        TryPushVersionChange(*pasteResult);
}

void EditorModel::InsertFromClipboard(Vec2 position,
                                      std::string_view clipboardText) {
    TryPushVersionChange(m_SelectionManager.Deselect(m_Grid));
    auto result =
        m_SelectionManager.Paste(m_Grid, clipboardText, position,
                                 std::numeric_limits<uint32_t>::max(), true);
    if (result)
        TryPushVersionChange(*result);
}

SimulationState EditorModel::SetSelectionBounds(Rect bounds) {
    auto [change1, change2] =
        m_SelectionManager.ModifySelectionBounds(m_Grid, bounds);
    TryPushVersionChange(change1);
    TryPushVersionChange(change2);

    return m_State;
}

bool EditorModel::UpdateSelectionAreaTracked(Vec2 gridPos) {
    if (IsEditBusy()) {
        return false;
    }

    auto result = m_SelectionManager.UpdateSelectionArea(m_Grid, gridPos);
    TryPushVersionChange(result.Change);
    return result.BeginSelection;
}

void EditorModel::TryResetSelection() {
    m_SelectionManager.TryResetSelection();
}

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

EditWorkState EditorModel::WorkState() const {
    if (m_EditBusy.load(std::memory_order_acquire)) {
        return EditWorkState::Working;
    }
    return EditWorkState::Idle;
}

bool EditorModel::IsEditBusy() const {
    return WorkState() == EditWorkState::Working;
}

EditDispatchResult EditorModel::CanDispatchEdit() const {
    if (m_State == SimulationState::Simulation ||
        m_State == SimulationState::Stepping) {
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

    if (!IsMutatingCommand(cmd)) {
        return true;
    }
    return CanDispatchEdit().Accepted;
}

bool EditorModel::TryStartCommand(const SimulationCommand& cmd,
                                  const ExecuteCommandContext& context) {
    if (m_EditBusy.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }

    std::scoped_lock lock{m_CommandMutex};
    if (ShouldExecuteInline(cmd)) {
        HashQuadtree::SetCacheIndex(m_EditorID);
        m_InlineCommandResult = ExecuteCommandImmediate(cmd, context);
        return true;
    }

    m_InFlightCommand = std::async(
        std::launch::async, [this, command = cmd, commandContext = context]() {
            HashQuadtree::SetCacheIndex(m_EditorID);
            return ExecuteCommandImmediate(command, commandContext);
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

    if (!m_InFlightCommand) {
        return std::nullopt;
    }

    if (m_InFlightCommand->wait_for(std::chrono::seconds{0}) !=
        std::future_status::ready) {
        return std::nullopt;
    }

    auto result = m_InFlightCommand->get();
    m_InFlightCommand.reset();
    m_EditBusy.store(false, std::memory_order_release);
    return result;
}

ExecuteCommandResult
EditorModel::ExecuteCommand(const SimulationCommand& cmd,
                            const ExecuteCommandContext& context) {
    HashQuadtree::SetCacheIndex(m_EditorID);
    return ExecuteCommandImmediate(cmd, context);
}

ExecuteCommandResult
EditorModel::ExecuteCommandImmediate(const SimulationCommand& cmd,
                                     const ExecuteCommandContext& context) {
    const static BigInt threshold{10'000'000U};
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
            [this](const SelectionBoundsCommand& command) {
                return ExecuteCommandResult{
                    .State = SetSelectionBounds(command.Bounds)};
            },
            [this](const CameraPositionCommand& command) {
                return ExecuteCommandResult{
                    .State = m_State, .CameraPositionCell = command.Position};
            },
            [this](const CameraZoomCommand& command) {
                return ExecuteCommandResult{.State = m_State,
                                            .CameraZoom = command.Zoom};
            },
            [this](const GenerateNoiseCommand& command) {
                const auto result = HandleGenerateNoise(
                    command.Density, static_cast<uint32_t>(threshold));
                if (!result) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::Noise,
                        .ErrorMessage =
                            "The region you have selected is too large to "
                            "generate noise.\n"};
                }
                return ExecuteCommandResult{.State = m_State};
            },
            [this, &context](const UndoCommand&) {
                if (context.PrimaryMouseDown) {
                    return ExecuteCommandResult{.State = m_State};
                }
                return ExecuteCommandResult{.State = HandleUndo()};
            },
            [this, &context](const RedoCommand&) {
                if (context.PrimaryMouseDown) {
                    return ExecuteCommandResult{.State = m_State};
                }
                return ExecuteCommandResult{.State = HandleRedo()};
            },
            [this](const SaveCommand& command) {
                if (!SaveToFile(command.FilePath, true)) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::File,
                        .ErrorMessage =
                            std::format("Failed to save file to \n{}",
                                        command.FilePath.string())};
                }
                return ExecuteCommandResult{.State = m_State};
            },
            [this, &context](const SaveAsNewCommand& command) {
                if (GridPopulation() > threshold &&
                    command.FilePath.extension().string() == ".rle" &&
                    !context.ConfirmSaveAsWarning) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .SaveAsWarning = SaveAsWarningRequest{
                            .FilePath = command.FilePath,
                            .Population = GridPopulation()}};
                }
                if (!SaveToFile(command.FilePath, false)) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::File,
                        .ErrorMessage =
                            std::format("Failed to save file to \n{}",
                                        command.FilePath.string())};
                }
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const LoadCommand& command) {
                auto error = LoadFile(command.FilePath);
                if (error) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::File,
                        .ErrorMessage =
                            std::format("Failed to load file:\n{}", *error)};
                }
                MarkSaved();
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const NewFileCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const CloseCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const RuleCommand& command) {
                const auto oldWidth = GridWidth();
                const auto oldHeight = GridHeight();
                const auto state = HandleRuleChange(command.RuleString);
                return ExecuteCommandResult{.State = state,
                                            .RecenterCameraToGridCenter =
                                                GridWidth() != oldWidth ||
                                                GridHeight() != oldHeight};
            },
            [this, &context](const SelectionCommand& command) {
                if (command.Action == SelectionAction::Paste) {
                    if (context.ForcePasteSelection) {
                        ForcePaste(context.CursorPos, command.ClipboardText);
                        return ExecuteCommandResult{.State = m_State};
                    }

                    auto result = PasteSelection(context.CursorPos,
                                                 command.ClipboardText);
                    if (!result) {
                        const auto errorType =
                            result.error().ErrorType ==
                                    FileEncoder::DecodeError::Type::TooManyCells
                                ? ExecuteCommandErrorType::PasteTooManyCells
                                : ExecuteCommandErrorType::Paste;
                        return ExecuteCommandResult{.State = m_State,
                                                    .ErrorType = errorType,
                                                    .ErrorMessage =
                                                        result.error().Message};
                    }
                    return ExecuteCommandResult{.State = m_State};
                }

                {
                    auto actionResult = [&] -> std::optional<CopyResult> {
                        if (command.Action == SelectionAction::Copy) {
                            return m_SelectionManager.Copy(m_Grid);
                        } else if (command.Action == SelectionAction::Cut) {
                            return m_SelectionManager.Cut(m_Grid);
                        } else {
                            return std::nullopt;
                        }
                    }();
                    if (actionResult) {
                        TryPushVersionChange(actionResult->Change);
                        return ExecuteCommandResult{
                            .State = m_State,
                            .ClipboardText =
                                std::move(actionResult->ClipboardText)};
                    }
                }

                if (!HandleSelectionAction(command.Action, command.NudgeSize)) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::Copy,
                        .ErrorMessage =
                            std::format(std::locale{""},
                                        "Tried editing too many cells ({:L})",
                                        SelectedPopulation())};
                }
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const PaintStrokeCommand& command) {
                if (command.Points.empty()) {
                    return ExecuteCommandResult{.State = m_State};
                }

                if (command.BeginStroke) {
                    BeginPaintChange();
                }

                for (const auto point : command.Points) {
                    if (!InBounds(point)) {
                        continue;
                    }
                    PaintCell(point, command.Value);
                }

                return ExecuteCommandResult{.State = m_State};
            }},
        cmd);
}

} // namespace gol
