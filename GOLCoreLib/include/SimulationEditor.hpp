#ifndef SimulationEditor_h_
#define SimulationEditor_h_

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>

#include "EditorResult.hpp"
#include "ErrorWindow.hpp"
#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "PresetSelectionResult.hpp"
#include "SelectionManager.hpp"
#include "SimulationControlResult.hpp"
#include "SimulationWorker.hpp"
#include "VersionManager.hpp"
#include "WarnWindow.hpp"

namespace gol {
class SimulationEditor {
  public:
    static constexpr float DefaultCellWidth = 20.f;
    static constexpr float DefaultCellHeight = 20.f;

  public:
    SimulationEditor(uint32_t id, const std::filesystem::path &path,
                     Size2 windowSize, Size2 gridSize);

    Rect WindowBounds() const;
    Rect ViewportBounds() const;
    const std::filesystem::path &CurrentFilePath() const {
        return m_CurrentFilePath;
    }

    EditorResult Update(std::optional<bool> activeOverride,
                        const SimulationControlResult &controlArgs,
                        const PresetSelectionResult &presetArgs);

    uint32_t EditorID() const { return m_EditorID; }
    bool IsSaved() const;
    bool operator==(const SimulationEditor &other) const;

  private:
    struct DisplayResult {
        bool Visible = false;
        bool Selected = false;
        bool Closing = false;
    };

  private:
    SimulationState SimulationUpdate(const GraphicsHandlerArgs &args);
    SimulationState PaintUpdate(const GraphicsHandlerArgs &args);
    SimulationState PauseUpdate(const GraphicsHandlerArgs &args);

    void DrawHashLifeData(const HashQuadtree &quadtree,
                          const GraphicsHandlerArgs &args);

    SimulationState StartSimulation();
    void StopSimulation(bool stealGrid);

    void UpdateVersion(const SimulationControlResult &args);

    DisplayResult DisplaySimulation(bool grabFocus);

    SimulationState UpdateState(const SimulationControlResult &action);

    void PasteSelection();

    void LoadFile(const SimulationControlResult &result);

    void SaveToFile(const SimulationControlResult &result);

    SimulationState ResizeGrid(const SimulationControlResult &result);

    void UpdateViewport();

    std::optional<Vec2> CursorGridPos();

    std::optional<Vec2> ConvertToGridPos(Vec2F screenPos);

    void UpdateMouseState(Vec2 gridPos);
    void FillCells();
    void UpdateDragState();

    void PasteWarnUpdated(PopupWindowState state);

  private:
    static constexpr double DefaultTickDelayMs = 0.;

  private:
    SelectionManager m_SelectionManager;

    GameGrid m_Grid;
    GameGrid m_InitialGrid;

    VersionManager m_VersionManager;

    GraphicsHandler m_Graphics;

    ErrorWindow m_FileErrorWindow;
    WarnWindow m_PasteWarning;
    WarnWindow m_SaveWarning;

    std::filesystem::path m_CurrentFilePath;

    RectF m_WindowBounds;

    std::unique_ptr<SimulationWorker> m_Worker;

    Vec2F m_LeftDeltaLast;
    Vec2F m_RightDeltaLast;

    uint32_t m_EditorID;

    EditorMode m_EditorMode = EditorMode::None;
    SimulationState m_State = SimulationState::Paint;

    bool m_StopStepCommand = false;
    bool m_TakeKeyboardInput = false;
    bool m_TakeMouseInput = false;
};
} // namespace gol

#endif