#ifndef EditorModel_hpp_
#define EditorModel_hpp_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "RLEEncoder.hpp"
#include "SelectionManager.hpp"
#include "SimulationSettings.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"

namespace gol {

class EditorModel {
  public:
    EditorModel(uint32_t id, const std::filesystem::path& path, Size2 gridSize);

    // Simulation lifecycle
    SimulationState StartSimulation();
    void StopSimulation(bool stealGrid);

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
    std::expected<void, RLEEncoder::DecodeError>
    PasteSelection(std::optional<Vec2> cursorPos);
    void ForcePaste(std::optional<Vec2> cursorPos);

    // Deselect and paste from clipboard at the given position
    void InsertFromClipboard(Vec2 position);

    SimulationState SetSelectionBounds(Rect bounds);

    bool UpdateSelectionAreaTracked(Vec2 gridPos);
    void TryResetSelection();
    void BeginPaintChange();
    void PaintCell(Vec2 pos, bool value);
    void MarkSaved();

    // Read-only accessors
    const GameGrid& Grid() const { return m_Grid; }
    const SelectionManager& Selection() const { return m_SelectionManager; }
    const VersionManager& Versions() const { return m_VersionManager; }
    SimulationWorker& Worker() { return *m_Worker; }

    SimulationState State() const { return m_State; }
    void SetState(SimulationState state) { m_State = state; }

    uint32_t EditorID() const { return m_EditorID; }
    bool IsSaved() const { return m_VersionManager.IsSaved(); }

    const std::filesystem::path& CurrentFilePath() const {
        return m_CurrentFilePath;
    }

    bool operator==(const EditorModel& other) const;

  private:
    SelectionManager m_SelectionManager;

    GameGrid m_Grid;
    GameGrid m_InitialGrid;

    VersionManager m_VersionManager;

    std::unique_ptr<SimulationWorker> m_Worker;

    std::filesystem::path m_CurrentFilePath;

    uint32_t m_EditorID;

    SimulationState m_State = SimulationState::Paint;
    bool m_StopStepCommand = false;
};

} // namespace gol

#endif
