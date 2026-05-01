#ifndef SimulationEditor_hpp_
#define SimulationEditor_hpp_

#include <cstdint>
#include <future>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "EditorModel.hpp"
#include "EditorResult.hpp"
#include "ErrorWindow.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "GraphicsHandler.hpp"
#include "PresetSelectionResult.hpp"
#include "SimulationCommand.hpp"
#include "SimulationControlResult.hpp"
#include "WarnWindow.hpp"

namespace Golde {
class SimulationEditor {
  public:
    static constexpr float DefaultCellWidth = 20.f;
    static constexpr float DefaultCellHeight = 20.f;

  public:
    SimulationEditor(uint32_t id, const std::filesystem::path& path,
                     Size2 windowSize, Size2 gridSize);

    Rect WindowBounds() const;
    Rect ViewportBounds() const;
    const std::filesystem::path& CurrentFilePath() const {
        return m_Model.CurrentFilePath();
    }

    EditorResult Update(std::optional<bool> activeOverride,
                        const SimulationControlResult& controlArgs,
                        const PresetSelectionResult& presetArgs);

    uint32_t EditorID() const { return m_Model.EditorID(); }
    bool IsSaved() const { return m_Model.IsSaved(); }
    bool operator==(const SimulationEditor& other) const;

  private:
    struct DisplayResult {
        bool Visible = false;
        bool Selected = false;
        bool Closing = false;
    };

  private:
    SimulationState SimulationUpdate(const GraphicsHandlerArgs& args);
    SimulationState PaintUpdate(const GraphicsHandlerArgs& args);
    SimulationState PauseUpdate(const GraphicsHandlerArgs& args);

    DisplayResult DisplaySimulation(bool grabFocus);

    SimulationState UpdateState(const SimulationControlResult& action);
    SimulationState ExecuteEditorCommand(const SimulationCommand& command,
                                         const ExecuteCommandContext& context);
    void ApplyCommandResult(const ExecuteCommandResult& result);
    void PollPendingCommandResult();

    void UpdateViewport();

    std::optional<Vec2> CursorGridPos();

    std::optional<Vec2> ConvertToGridPos(Vec2F screenPos);

    void UpdateMouseState(Vec2 gridPos);
    void FillCells(Vec2 gridPos, bool canDispatch);
    void UpdateDragState();

    void PasteWarnUpdated(PopupWindowState state);
    void LoadRuleWarnUpdated(PopupWindowState state);

  private:
    static constexpr double DefaultTickDelayMs = 0.;

  private:
    EditorModel m_Model;

    GraphicsHandler m_Graphics;

    ErrorWindow m_FileErrorWindow;
    ErrorWindow m_CopyErrorWindow;
    ErrorWindow m_GenerateNoiseError;
    WarnWindow m_PasteWarning;
    WarnWindow m_LoadRuleWarning;
    WarnWindow m_SaveWarning;

    std::optional<LoadRuleWarningRequest> m_PendingLoadRuleWarning;

    RectF m_WindowBounds;

    Vec2F m_LeftDeltaLast;
    Vec2F m_RightDeltaLast;
    bool m_BeginPaintStroke = false;
    std::optional<Vec2> m_LastPaintGridPos;
    std::vector<Vec2> m_BufferedPaintPoints;
    std::optional<std::future<ExecuteCommandResult>> m_PendingCommandResult;

    EditorMode m_EditorMode = EditorMode::None;

    bool m_TakeKeyboardInput = false;
    bool m_TakeMouseInput = false;

#ifdef _DEBUG
    double lastTime = 0;
    int32_t fps = 0;
    int32_t frameCounter = 0;
#endif
};
} // namespace Golde

#endif
