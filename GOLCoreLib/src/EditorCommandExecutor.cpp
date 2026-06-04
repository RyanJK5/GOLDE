#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <locale>
#include <optional>
#include <string>
#include <vector>

#include "EditorCommandExecutor.hpp"
#include "EditorModel.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "SimulationCommand.hpp"
#include "VersionManager.hpp"

namespace Golde {

EditorCommandExecutor::EditorCommandExecutor(HashLifeCache& cache,
                                             uint32_t editorID,
                                             const std::filesystem::path& path,
                                             Size2 gridSize)
    : m_LifeCache(cache), m_Grid(cache, gridSize), m_CurrentFilePath(path),
      m_EditorID(editorID) {}

Size2 EditorCommandExecutor::GridSize() const { return m_Grid.Size(); }
int32_t EditorCommandExecutor::GridWidth() const { return m_Grid.Width(); }
int32_t EditorCommandExecutor::GridHeight() const { return m_Grid.Height(); }
const HashQuadtree& EditorCommandExecutor::GridData() const {
    return m_Grid.Data();
}
bool EditorCommandExecutor::GridDead() const { return m_Grid.Dead(); }
bool EditorCommandExecutor::InBounds(Vec2 pos) const {
    return m_Grid.InBounds(pos);
}
std::optional<bool> EditorCommandExecutor::CellAt(Vec2 pos) const {
    return m_Grid.Get(pos.X, pos.Y);
}

bool EditorCommandExecutor::SelectionActive() const {
    return m_SelectionManager.CanDrawGrid();
}
bool EditorCommandExecutor::CanDrawSelection() const {
    return m_SelectionManager.CanDrawSelection();
}
bool EditorCommandExecutor::CanDrawLargeSelection() const {
    return m_SelectionManager.CanDrawLargeSelection();
}
bool EditorCommandExecutor::SelectionGridAlive() const {
    return m_SelectionManager.GridAlive();
}
const HashQuadtree& EditorCommandExecutor::SelectionGridData() const {
    return m_SelectionManager.GridData();
}
const BigInt& EditorCommandExecutor::SelectedPopulation() const {
    return m_SelectionManager.SelectedPopulation();
}
std::optional<Rect> EditorCommandExecutor::SelectionBoundsOpt() const {
    if (!m_SelectionManager.CanDrawSelection())
        return std::nullopt;
    return m_SelectionManager.SelectionBounds();
}

BigInt EditorCommandExecutor::GridPopulation() const {
    return m_Grid.Population();
}
BigInt EditorCommandExecutor::GridGeneration() const {
    return m_Grid.Generation();
}
std::string_view EditorCommandExecutor::CurrentRuleString() const {
    return m_Grid.GetRuleString();
}

SimulationState EditorCommandExecutor::State() const { return m_State; }
void EditorCommandExecutor::SetState(SimulationState state) { m_State = state; }

uint32_t EditorCommandExecutor::EditorID() const { return m_EditorID; }

const std::filesystem::path& EditorCommandExecutor::CurrentFilePath() const {
    return m_CurrentFilePath;
}

bool EditorCommandExecutor::IsSimulationOutOfBounds() const {
    return m_Grid.BoundingBox() == Rect{};
}

const GameGrid& EditorCommandExecutor::Grid() const { return m_Grid; }
GameGrid& EditorCommandExecutor::Grid() { return m_Grid; }

void EditorCommandExecutor::PushVersionChange(
    const std::optional<VersionState>& change) {
    if (change)
        m_PendingVersionChanges.push_back(*change);
}

void EditorCommandExecutor::PushVersionChange(const VersionState& change) {
    m_PendingVersionChanges.push_back(change);
}

bool EditorCommandExecutor::UpdateSelectionAreaTracked(
    Vec2 gridPos, std::vector<VersionState>& outChanges) {
    auto result = m_SelectionManager.UpdateSelectionArea(m_Grid, gridPos);
    if (result.Change)
        outChanges.push_back(*result.Change);
    return result.BeginSelection;
}

void EditorCommandExecutor::TryResetSelection() {
    m_SelectionManager.TryResetSelection();
}

void EditorCommandExecutor::BeginPaintChange(VersionManager& versionManager) {
    versionManager.BeginPaintChange(m_Grid, m_State);
}

void EditorCommandExecutor::PaintCell(Vec2 pos, bool value,
                                      VersionManager& versionManager) {
    if (*m_Grid.Get(pos.X, pos.Y) == value)
        return;
    m_Grid.Set(pos.X, pos.Y, value);
    versionManager.AddPaintChange(m_Grid, m_State);
}

void EditorCommandExecutor::MarkSaved(VersionManager& versionManager) {
    versionManager.Save();
}

void EditorCommandExecutor::ApplyVersionChange(const VersionState& change) {
    m_SelectionManager.HandleVersionChange(m_Grid, change);
}

// ---------------------------------------------------------------------------
// Private command handlers
// ---------------------------------------------------------------------------

SimulationState
EditorCommandExecutor::HandleRuleChange(std::string_view ruleStr) {
    const auto rule = *LifeRule::Make(ruleStr);
    const auto oldSize = m_Grid.Size();
    const auto ruleBounds = rule.Bounds().value_or(Rect{});

    m_Grid.SetRule(rule, ruleStr);

    if (oldSize != ruleBounds.Size()) {
        m_Grid = GameGrid{std::move(m_Grid),
                          rule.Bounds() ? rule.Bounds()->Size() : Size2{}};
    }

    m_SelectionManager.SetSelectionRule(rule, ruleStr);
    PushVersionChange(VersionState{.Universe = m_Grid});

    if (m_Grid.Size() == oldSize) {
        return m_State;
    }

    if (m_SelectionManager.CanDrawSelection()) {
        auto selection = m_SelectionManager.SelectionBounds();
        if (!m_Grid.InBounds(selection.UpperLeft()) ||
            !m_Grid.InBounds(selection.UpperRight()) ||
            !m_Grid.InBounds(selection.LowerLeft()) ||
            !m_Grid.InBounds(selection.LowerRight()))
            PushVersionChange(m_SelectionManager.Deselect(m_Grid));
    }
    return m_State;
}

bool EditorCommandExecutor::HandleGenerateNoise(float density,
                                                uint32_t warnThreshold) {
    if (!m_SelectionManager.CanDrawGrid())
        return false;
    const auto selectionBounds = m_SelectionManager.SelectionBounds();
    PushVersionChange(m_SelectionManager.Deselect(m_Grid));

    const auto result = m_SelectionManager.InsertNoise(m_Grid, selectionBounds,
                                                       warnThreshold, density);
    if (result) {
        PushVersionChange(result);
        return true;
    } else {
        m_SelectionManager.ModifySelectionBounds(m_Grid, selectionBounds);
        return false;
    }
}

std::expected<void, std::string>
EditorCommandExecutor::HandleSelectionAction(SelectionAction action,
                                             int32_t nudgeSize) {
    if (action == SelectionAction::SelectAll)
        PushVersionChange(m_SelectionManager.Deselect(m_Grid));

    const auto actionResult =
        m_SelectionManager.HandleAction(action, m_Grid, nudgeSize);
    PushVersionChange(actionResult);

    if (!actionResult && (action == SelectionAction::FlipHorizontally ||
                          action == SelectionAction::FlipVertically ||
                          action == SelectionAction::RotateClockwise ||
                          action == SelectionAction::RotateCounterclockwise)) {
        return std::unexpected{
            std::format(std::locale{""}, "Tried editing too many cells ({:L})",
                        SelectedPopulation())};
    } else if (!actionResult) {
        return std::unexpected{GenerateDepthError()};
    } else {
        return {};
    }
}

std::optional<std::string>
EditorCommandExecutor::LoadFile(const std::filesystem::path& path) {
    PushVersionChange(m_SelectionManager.Deselect(m_Grid));
    auto loadResult = m_SelectionManager.Load(m_Grid, path);
    if (loadResult) {
        PushVersionChange(*loadResult);
        return std::nullopt;
    }
    return loadResult.error().Message;
}

bool EditorCommandExecutor::SaveToFile(const std::filesystem::path& path,
                                       bool) {
    if (m_SelectionManager.Save(m_Grid, path)) {
        if (m_CurrentFilePath.empty())
            m_CurrentFilePath = path;
        // markAsSaved is intentionally unused here — VersionManager::Save()
        // lives in EditorModel. Callers that need save-marking pass the flag
        // through ExecuteCommandResult::MarkSaved (not implemented; see plan).
        // For now, save-state tracking is EditorModel's responsibility.
        return true;
    }
    return false;
}

std::expected<void, FileEncoder::DecodeError>
EditorCommandExecutor::PasteSelection(std::optional<Vec2> cursorPos,
                                      std::string_view clipboardText,
                                      bool unlock) {
    if (cursorPos || m_SelectionManager.CanDrawGrid()) {
        PushVersionChange(m_SelectionManager.Deselect(m_Grid));
    }
    auto pasteResult = m_SelectionManager.Paste(
        m_Grid, clipboardText, cursorPos, 100'000'000U, unlock);
    if (pasteResult) {
        PushVersionChange(*pasteResult);
        return {};
    }
    return std::unexpected{pasteResult.error()};
}

void EditorCommandExecutor::ForcePaste(std::optional<Vec2> cursorPos,
                                       std::string_view clipboardText) {
    auto pasteResult = m_SelectionManager.Paste(
        m_Grid, clipboardText, cursorPos, std::numeric_limits<uint32_t>::max());
    if (pasteResult)
        PushVersionChange(*pasteResult);
}

void EditorCommandExecutor::InsertFromClipboard(
    Vec2 position, std::string_view clipboardText) {
    PushVersionChange(m_SelectionManager.Deselect(m_Grid));
    auto result =
        m_SelectionManager.Paste(m_Grid, clipboardText, position,
                                 std::numeric_limits<uint32_t>::max(), true);
    if (result)
        PushVersionChange(*result);
}

SimulationState EditorCommandExecutor::SetSelectionBounds(Rect bounds) {
    auto [change1, change2] =
        m_SelectionManager.ModifySelectionBounds(m_Grid, bounds);
    PushVersionChange(change1);
    PushVersionChange(change2);
    return m_State;
}

std::optional<ExecuteCommandResult> EditorCommandExecutor::HandleIncomingRule(
    std::optional<std::string_view> incomingRule,
    bool hadExistingUniverseData) {
    if (!incomingRule)
        return std::nullopt;

    const std::string originalRule{m_Grid.GetRuleString()};
    if (*incomingRule == originalRule)
        return std::nullopt;

    if (hadExistingUniverseData) {
        return ExecuteCommandResult{
            .State = m_State,
            .LoadRuleWarning = LoadRuleWarningRequest{
                .OriginalRuleString = originalRule,
                .LoadedRuleString = std::string{*incomingRule}}};
    }

    const auto oldWidth = GridWidth();
    const auto oldHeight = GridHeight();
    const auto state = HandleRuleChange(*incomingRule);

    return ExecuteCommandResult{.State = state,
                                .RecenterCameraToGridCenter =
                                    GridWidth() != oldWidth ||
                                    GridHeight() != oldHeight};
}

std::string EditorCommandExecutor::GenerateDepthError() const {
    return std::format(
        "GOLDE does not currently support stable editing to universes greater "
        "than 2^4096\ncells across (currently 2^{} cells across)",
        m_Grid.UniverseDepth());
}

std::optional<VersionState> EditorCommandExecutor::Deselect() {
    return m_SelectionManager.Deselect(m_Grid);
}

// ---------------------------------------------------------------------------
// Execute — main dispatch
// ---------------------------------------------------------------------------

ExecuteCommandResult
EditorCommandExecutor::Execute(const SimulationCommand& cmd,
                               const ExecuteCommandContext& context) {
    m_PendingVersionChanges.clear();

    const static BigInt threshold{10'000'000U};

    auto result = std::visit(
        Overloaded{
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
                const static BigInt noiseThreshold{10'000'000U};
                const auto result = HandleGenerateNoise(
                    command.Density, static_cast<uint32_t>(noiseThreshold));
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
                const static BigInt saveThreshold{10'000'000U};
                if (GridPopulation() > saveThreshold &&
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
                const bool hadExistingUniverseData =
                    !m_Grid.Dead() || !SelectedPopulation().is_zero() ||
                    m_Grid.Size() != Size2{};
                auto error = LoadFile(command.FilePath);
                if (error) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::File,
                        .ErrorMessage =
                            std::format("Failed to load file:\n{}", *error)};
                }

                if (auto incomingRuleResult = HandleIncomingRule(
                        m_SelectionManager.SelectionRuleString(),
                        hadExistingUniverseData)) {
                    return *incomingRuleResult;
                }

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
                    if (!m_Grid.ShouldAllowUniverseEdits()) {
                        return ExecuteCommandResult{
                            .State = m_State,
                            .ErrorType = ExecuteCommandErrorType::Paste,
                            .ErrorMessage = GenerateDepthError()};
                    }

                    const bool hadExistingUniverseData =
                        !m_Grid.Dead() || !SelectedPopulation().is_zero() ||
                        m_Grid.Size() != Size2{};

                    if (context.ForcePasteSelection) {
                        ForcePaste(context.CursorPos, command.ClipboardText);
                        return ExecuteCommandResult{.State = m_State};
                    }

                    auto result =
                        PasteSelection(context.CursorPos, command.ClipboardText,
                                       context.UnlockPasteSelection);
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

                    if (auto incomingRuleResult = HandleIncomingRule(
                            m_SelectionManager.SelectionRuleString(),
                            hadExistingUniverseData)) {
                        return *incomingRuleResult;
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
                        PushVersionChange(actionResult->Change);
                        return ExecuteCommandResult{
                            .State = m_State,
                            .ClipboardText =
                                std::move(actionResult->ClipboardText)};
                    }
                }

                if (auto cmdResult = HandleSelectionAction(command.Action,
                                                           command.NudgeSize);
                    !cmdResult) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::FailedEdit,
                        .ErrorMessage = std::move(cmdResult.error())};
                }
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const PaintStrokeCommand& command) {
                if (!m_Grid.ShouldAllowUniverseEdits()) {
                    return ExecuteCommandResult{
                        .State = m_State,
                        .ErrorType = ExecuteCommandErrorType::FailedEdit,
                        .ErrorMessage = GenerateDepthError()};
                }

                if (command.Points.empty()) {
                    return ExecuteCommandResult{.State = m_State};
                }

                // PaintStrokeCommand goes through the async path, so we
                // cannot call BeginPaintChange / PaintCell with a
                // VersionManager reference here. Instead, we record the
                // pre-stroke and post-stroke universe states directly.
                const auto preStroke = m_Grid;
                for (const auto point : command.Points) {
                    if (!InBounds(point))
                        continue;
                    m_Grid.Set(point.X, point.Y, command.Value);
                }
                if (command.BeginStroke) {
                    PushVersionChange(VersionState{.Universe = preStroke});
                }
                PushVersionChange(VersionState{.Universe = m_Grid});

                return ExecuteCommandResult{.State = m_State};
            },
            // Simulation lifecycle commands never reach Execute() —
            // they are handled inline in EditorModel. These arms exist
            // only to satisfy the exhaustive visitor.
            [this](const StartCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const ClearCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const ResetCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const RestartCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const PauseCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const ResumeCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this](const StepCommand&) {
                return ExecuteCommandResult{.State = m_State};
            },
            [this, &context](const UndoCommand&) {
                // Undo/redo handled inline in EditorModel; unreachable here.
                return ExecuteCommandResult{.State = m_State};
            },
            [this, &context](const RedoCommand&) {
                return ExecuteCommandResult{.State = m_State};
            }},
        cmd);

    result.VersionChanges = std::move(m_PendingVersionChanges);
    return result;
}

} // namespace Golde