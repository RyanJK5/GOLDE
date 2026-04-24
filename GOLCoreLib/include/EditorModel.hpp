#ifndef EditorModel_hpp_
#define EditorModel_hpp_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>

#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "RLEEncoder.hpp"
#include "SelectionManager.hpp"
#include "SimulationCommand.hpp"
#include "SimulationSettings.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace gol {

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
    bool ConfirmSaveAsWarning = false;
};

struct SaveAsWarningRequest {
    std::filesystem::path FilePath;
    BigInt Population{};
};

enum class ExecuteCommandErrorType {
    None,
    File,
    Copy,
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
};

class EditorModel {
  public:
    EditorModel(uint32_t id, const std::filesystem::path& path, Size2 gridSize);

    // Simulation lifecycle
    SimulationState StartSimulation();
    void StopSimulation(bool stealGrid);

    bool TryStartCommand(const SimulationCommand& cmd,
                         const ExecuteCommandContext& context);
    std::optional<ExecuteCommandResult> PollCommandResult();

    // Process and clear the end-of-step flag (call once per frame)
    void CheckStopStep();

    // Apply per-frame settings from the control panel
    void ApplySettings(const SimulationSettings& settings);

    // Command handlers
    SimulationState HandleStart();
    SimulationState HandleClear();
    SimulationState HandleReset();
    SimulationState HandleRestart();
    SimulationState HandlePause();
    SimulationState HandleResume();
    SimulationState HandleStep();
    SimulationState HandleRuleChange(std::string_view ruleStr);
    SimulationState HandleUndo();
    SimulationState HandleRedo();
    bool HandleSelectionAction(SelectionAction action, int32_t nudgeSize);
    bool HandleGenerateNoise(float density, uint32_t warnThreshold);

    // File operations
    std::optional<std::string> LoadFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path, bool markAsSaved);

    // Paste operations
    std::expected<void, FileEncoder::DecodeError>
    PasteSelection(std::optional<Vec2> cursorPos,
                   std::string_view clipboardText);
    void ForcePaste(std::optional<Vec2> cursorPos,
                    std::string_view clipboardText);

    // Deselect and paste from clipboard at the given position
    void InsertFromClipboard(Vec2 position, std::string_view clipboardText);

    SimulationState SetSelectionBounds(Rect bounds);

    bool UpdateSelectionAreaTracked(Vec2 gridPos);
    void TryResetSelection();
    void BeginPaintChange();
    void PaintCell(Vec2 pos, bool value);
    void MarkSaved();

    // Facade read API for SimulationEditor.
    Size2 GridSize() const { return m_Grid.Size(); }
    int32_t GridWidth() const { return m_Grid.Width(); }
    int32_t GridHeight() const { return m_Grid.Height(); }
    const HashQuadtree& GridData() const { return m_Grid.Data(); }
    bool GridDead() const { return m_Grid.Dead(); }
    bool InBounds(Vec2 pos) const { return m_Grid.InBounds(pos); }
    std::optional<bool> CellAt(Vec2 pos) const {
        return m_Grid.Get(pos.X, pos.Y);
    }
    const GameGrid* SimulationSnapshot() const { return m_Worker->GetResult(); }
    std::chrono::duration<float> SimulationLag() const {
        return m_Worker->GetTimeSinceLastUpdate();
    }

    bool SelectionActive() const { return m_SelectionManager.CanDrawGrid(); }
    bool CanDrawSelection() const {
        return m_SelectionManager.CanDrawSelection();
    }
    bool CanDrawLargeSelection() const {
        return m_SelectionManager.CanDrawLargeSelection();
    }
    bool SelectionGridAlive() const { return m_SelectionManager.GridAlive(); }
    const HashQuadtree& SelectionGridData() const {
        return m_SelectionManager.GridData();
    }
    const BigInt& SelectedPopulation() const {
        return m_SelectionManager.SelectedPopulation();
    }
    std::optional<Rect> SelectionBoundsOpt() const {
        if (!m_SelectionManager.CanDrawSelection())
            return std::nullopt;
        return m_SelectionManager.SelectionBounds();
    }

    bool UndosAvailable() const { return m_VersionManager.UndosAvailable(); }
    bool RedosAvailable() const { return m_VersionManager.RedosAvailable(); }
    BigInt GridPopulation() const { return m_Grid.Population(); }
    BigInt GridGeneration() const { return m_Grid.Generation(); }

    EditWorkState WorkState() const;
    bool IsEditBusy() const;
    EditDispatchResult CanDispatchEdit() const;
    bool IsMutatingCommand(const SimulationCommand& cmd) const;
    bool CanDispatchMutatingCommand(const SimulationCommand& cmd) const;
    ExecuteCommandResult ExecuteCommand(const SimulationCommand& cmd,
                                        const ExecuteCommandContext& context);

    SimulationState State() const { return m_State; }
    void SetState(SimulationState state) { m_State = state; }

    uint32_t EditorID() const { return m_EditorID; }
    bool IsSaved() const { return m_VersionManager.IsSaved(); }

    const std::filesystem::path& CurrentFilePath() const {
        return m_CurrentFilePath;
    }

    bool operator==(const EditorModel& other) const;

    bool IsSimulationOutOfBounds() const;

  private:
    ExecuteCommandResult
    ExecuteCommandImmediate(const SimulationCommand& cmd,
                            const ExecuteCommandContext& context);

    SelectionManager m_SelectionManager;

    GameGrid m_Grid;
    GameGrid m_InitialGrid;

    VersionManager m_VersionManager;

    std::mutex m_CommandMutex;
    std::optional<std::future<ExecuteCommandResult>> m_InFlightCommand;
    std::atomic<bool> m_EditBusy = false;

    std::unique_ptr<SimulationWorker> m_Worker;

    std::filesystem::path m_CurrentFilePath;

    uint32_t m_EditorID;

    SimulationState m_State = SimulationState::Paint;
    std::atomic<bool> m_StopStepCommand = false;
};

} // namespace gol

#endif
