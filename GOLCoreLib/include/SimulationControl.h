#ifndef SimulationControl_h_
#define SimulationControl_h_

#include <cstdint>
#include <imgui/imgui.h>
#include <unordered_map>
#include <vector>

#include "ConfigLoader.h"
#include "DelayWidget.h"
#include "EditorWidget.h"
#include "ExecutionWidget.h"
#include "FileWidget.h"
#include "GameEnums.h"
#include "KeyShortcut.h"
#include "NoiseWidget.h"
#include "ResizeWidget.h"
#include "SimulationControlResult.h"
#include "StepWidget.h"

namespace gol {
struct ButtonlessShortcuts {
    std::unordered_map<ActionVariant, std::vector<KeyShortcut>> Shortcuts;

    ButtonlessShortcuts(const std::vector<ImGuiKeyChord> &left,
                        const std::vector<ImGuiKeyChord> &right,
                        const std::vector<ImGuiKeyChord> &up,
                        const std::vector<ImGuiKeyChord> &down,
                        const std::vector<ImGuiKeyChord> &close);

    SimulationControlResult Update(const EditorResult &state);
};

class SimulationControl {
  public:
    SimulationControl(const StyleLoader::StyleInfo<ImVec4> &fileInfo);

    SimulationControlResult Update(const EditorResult &state);

  private:
    void FillResults(SimulationControlResult &current,
                     const SimulationControlResult &update) const;

  private:
    static constexpr int32_t BigStep = 100;
    static constexpr int32_t StepWarning = 100;

  private:
    ButtonlessShortcuts m_ButtonlessShortcuts;

    ExecutionWidget m_ExecutionWidget;
    EditorWidget m_EditorWidget;
    FileWidget m_FileWidget;
    ResizeWidget m_ResizeWidget;
    StepWidget m_StepWidget;
    DelayWidget m_DelayWidget;
    NoiseWidget m_NoiseWidget;
};
} // namespace gol

#endif