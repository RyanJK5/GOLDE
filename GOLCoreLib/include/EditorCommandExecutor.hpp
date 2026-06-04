#ifndef EditorCommandExecutor_hpp_
#define EditorCommandExecutor_hpp_

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "FileFormatHandler.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "SelectionManager.hpp"
#include "SimulationCommand.hpp"
#include "VersionManager.hpp"

namespace Golde {

struct ExecuteCommandContext;
struct ExecuteCommandResult;

class EditorCommandExecutor {
  public:
    EditorCommandExecutor(HashLifeCache& cache, uint32_t editorID,
                          const std::filesystem::path& path, Size2 gridSize);

    // Execute a command and return the result. All version changes are
    // accumulated in result.VersionChanges rather than written to a
    // VersionManager — the caller applies them after publish.
    ExecuteCommandResult Execute(const SimulationCommand& cmd,
                                 const ExecuteCommandContext& context);

    // Apply a version change coming from VersionManager::Undo/Redo.
    // Called by EditorModel on the main thread only.
    void ApplyVersionChange(const VersionState& change);

    // Read facades — safe to call from the main thread when no async
    // command is in flight.
    Size2 GridSize() const;
    int32_t GridWidth() const;
    int32_t GridHeight() const;
    const HashQuadtree& GridData() const;
    bool GridDead() const;
    bool InBounds(Vec2 pos) const;
    std::optional<bool> CellAt(Vec2 pos) const;

    bool SelectionActive() const;
    bool CanDrawSelection() const;
    bool CanDrawLargeSelection() const;
    bool SelectionGridAlive() const;
    const HashQuadtree& SelectionGridData() const;
    const BigInt& SelectedPopulation() const;
    std::optional<Rect> SelectionBoundsOpt() const;

    BigInt GridPopulation() const;
    BigInt GridGeneration() const;
    std::string_view CurrentRuleString() const;

    SimulationState State() const;
    void SetState(SimulationState state);

    uint32_t EditorID() const;
    bool IsSaved() const;
    const std::filesystem::path& CurrentFilePath() const;

    bool IsSimulationOutOfBounds() const;

    // Direct-access methods called by EditorModel outside command dispatch.
    bool UpdateSelectionAreaTracked(Vec2 gridPos,
                                    std::vector<VersionState>& outChanges);
    void TryResetSelection();
    void BeginPaintChange(VersionManager& versionManager);
    void PaintCell(Vec2 pos, bool value, VersionManager& versionManager);
    void MarkSaved(VersionManager& versionManager);

    // Expose grid reference so EditorModel can pass it to the worker.
    const GameGrid& Grid() const;
    GameGrid& Grid();

    // Expose initial grid so EditorModel can manage reset/restart.
    const GameGrid& InitialGrid() const;
    void SetInitialGrid(const GameGrid& grid);

  private:
    // Command handlers — each operates on the executor's own members.
    SimulationState HandleRuleChange(std::string_view ruleStr);
    bool HandleGenerateNoise(float density, uint32_t warnThreshold);
    std::expected<void, std::string>
    HandleSelectionAction(SelectionAction action, int32_t nudgeSize);

    std::optional<std::string> LoadFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path, bool markAsSaved);

    std::expected<void, FileEncoder::DecodeError>
    PasteSelection(std::optional<Vec2> cursorPos, std::string_view clipboardText,
                   bool unlock = false);
    void ForcePaste(std::optional<Vec2> cursorPos,
                    std::string_view clipboardText);
    void InsertFromClipboard(Vec2 position, std::string_view clipboardText);

    SimulationState SetSelectionBounds(Rect bounds);

    std::optional<ExecuteCommandResult>
    HandleIncomingRule(std::optional<std::string_view> incomingRule,
                       bool hadExistingUniverseData);

    std::string GenerateDepthError() const;

    // Version change accumulator — flushed into ExecuteCommandResult.
    void PushVersionChange(const std::optional<VersionState>& change);
    void PushVersionChange(const VersionState& change);

  private:
    std::reference_wrapper<HashLifeCache> m_LifeCache;

    GameGrid m_Grid;
    GameGrid m_InitialGrid;
    SelectionManager m_SelectionManager;
    SimulationState m_State = SimulationState::Paint;

    std::filesystem::path m_CurrentFilePath;
    uint32_t m_EditorID;

    // Accumulated during Execute(), moved into ExecuteCommandResult.
    std::vector<VersionState> m_PendingVersionChanges;
};

} // namespace Golde

#endif