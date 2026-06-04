#ifndef EditorModel_hpp_
#define EditorModel_hpp_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "EditorCommandExecutor.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "SimulationCommand.hpp"
#include "SimulationSettings.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace Golde {

enum class EditWorkState { Idle, Working, PublishPending, Failed };

enum class EditRejectReason { Busy, SimulationRunning, InvalidState };

struct EditDispatchResult {
    bool Accepted = false;
    std::optional<EditRejectReason> RejectedReason{};
};

struct ExecuteCommandContext {
    std::optional<Vec2> CursorPos{};
    bool PrimaryMouseDown = false;
    bool ForcePasteSelection = false;
    bool UnlockPasteSelection = false;
    bool ConfirmSaveAsWarning = false;
};

struct SaveAsWarningRequest {
    std::filesystem::path FilePath;
    BigInt Population{};
};

struct LoadRuleWarningRequest {
    std::string OriginalRuleString;
    std::string LoadedRuleString;
};

enum class ExecuteCommandErrorType {
    None,
    File,
    FailedEdit,
    Noise,
    Paste,
    PasteTooManyCells
};

struct ExecuteCommandResult {
    SimulationState State = SimulationState::Paint;
    std::optional<Vec2> CameraPositionCell{};
    std::optional<float> CameraZoom{};
    bool RecenterCameraToGridCenter = false;
    ExecuteCommandErrorType ErrorType = ExecuteCommandErrorType::None;
    std::optional<std::string> ErrorMessage{};
    std::optional<SaveAsWarningRequest> SaveAsWarning{};
    std::optional<LoadRuleWarningRequest> LoadRuleWarning{};
    std::optional<std::string> ClipboardText{};
    // Version changes produced by the async executor. Applied by
    // EditorModel::PollCommandResult and never seen by callers above.
    std::vector<VersionState> VersionChanges{};
};

class EditorModel {
  public:
    EditorModel(uint32_t id, const std::filesystem::path& path, Size2 gridSize);

    // Apply per-frame settings from the control panel.
    void ApplySettings(const SimulationSettings& settings);

    // Process and clear the end-of-step flag (call once per frame).
    void CheckStopStep();

    bool TryStartCommand(const SimulationCommand& cmd,
                         const ExecuteCommandContext& context);
    std::optional<ExecuteCommandResult> PollCommandResult();

    // Facade read API for SimulationEditor — delegates to m_Executor.
    Size2 GridSize() const { return m_Executor.GridSize(); }
    int32_t GridWidth() const { return m_Executor.GridWidth(); }
    int32_t GridHeight() const { return m_Executor.GridHeight(); }
    const HashQuadtree& GridData() const { return m_Executor.GridData(); }
    bool GridDead() const { return m_Executor.GridDead(); }
    bool InBounds(Vec2 pos) const { return m_Executor.InBounds(pos); }
    std::optional<bool> CellAt(Vec2 pos) const { return m_Executor.CellAt(pos); }

    std::optional<GameGrid> SimulationSnapshot() const {
        return m_Worker->GetResult();
    }
    std::chrono::duration<float> SimulationLag() const {
        return m_Worker->GetTimeSinceLastUpdate();
    }

    bool SelectionActive() const { return m_Executor.SelectionActive(); }
    bool CanDrawSelection() const { return m_Executor.CanDrawSelection(); }
    bool CanDrawLargeSelection() const { return m_Executor.CanDrawLargeSelection(); }
    bool SelectionGridAlive() const { return m_Executor.SelectionGridAlive(); }
    const HashQuadtree& SelectionGridData() const { return m_Executor.SelectionGridData(); }
    const BigInt& SelectedPopulation() const { return m_Executor.SelectedPopulation(); }
    std::optional<Rect> SelectionBoundsOpt() const { return m_Executor.SelectionBoundsOpt(); }

    BigInt GridPopulation() const { return m_Executor.GridPopulation(); }
    BigInt GridGeneration() const { return m_Executor.GridGeneration(); }
    std::string_view CurrentRuleString() const { return m_Executor.CurrentRuleString(); }

    bool IsSimulationOutOfBounds() const { return m_Executor.IsSimulationOutOfBounds(); }

    SimulationState State() const { return m_Executor.State(); }
    void SetState(SimulationState state) { m_Executor.SetState(state); }
    uint32_t EditorID() const { return m_Executor.EditorID(); }
    bool IsSaved() const { return m_VersionManager.IsSaved(); }
    bool UndosAvailable() const { return m_VersionManager.UndosAvailable(); }
    bool RedosAvailable() const { return m_VersionManager.RedosAvailable(); }
    const std::filesystem::path& CurrentFilePath() const {
        return m_Executor.CurrentFilePath();
    }

    EditWorkState WorkState() const;
    bool IsEditBusy() const;
    EditDispatchResult CanDispatchEdit() const;
    bool IsMutatingCommand(const SimulationCommand& cmd) const;
    bool CanDispatchMutatingCommand(const SimulationCommand& cmd) const;

    // Direct-access methods called by SimulationEditor outside command dispatch.
    bool UpdateSelectionAreaTracked(Vec2 gridPos);
    void TryResetSelection();
    void BeginPaintChange();
    void PaintCell(Vec2 pos, bool value);
    void MarkSaved();

    bool operator==(const EditorModel& other) const;

  private:
    // Result wrapper used internally for the async path.
    struct AsyncCommandResult {
        EditorCommandExecutor UpdatedExecutor;
        ExecuteCommandResult Result;
    };

    // Inline execution path — simulation lifecycle, undo/redo, and other
    // commands that must run synchronously on the main thread.
    ExecuteCommandResult ExecuteInline(const SimulationCommand& cmd,
                                       const ExecuteCommandContext& context);

    // Simulation lifecycle helpers — all main-thread only.
    SimulationState StartSimulation();
    void StopSimulation(bool stealGrid);

    SimulationState HandleStart();
    SimulationState HandleClear();
    SimulationState HandleReset();
    SimulationState HandleRestart();
    SimulationState HandlePause();
    SimulationState HandleResume();
    SimulationState HandleStep();
    SimulationState HandleUndo();
    SimulationState HandleRedo();

    // Applies a completed async result back into m_Executor and m_VersionManager.
    void ApplyAsyncResult(AsyncCommandResult&& asyncResult);

  private:
    HashLifeCache m_LifeCache;
    EditorCommandExecutor m_Executor;
    VersionManager m_VersionManager;
    std::unique_ptr<SimulationWorker> m_Worker;

    std::mutex m_CommandMutex;
    std::optional<ExecuteCommandResult> m_InlineCommandResult;
    std::optional<std::future<AsyncCommandResult>> m_InFlightCommand;
    std::atomic<bool> m_EditBusy = false;
    std::atomic<bool> m_StopStepCommand = false;
};

} // namespace Golde

#endif