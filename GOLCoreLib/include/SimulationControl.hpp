#ifndef SimulationControl_h_
#define SimulationControl_h_

#include <cstdint>
#include <imgui.h>
#include <thread>
#include <stop_token>
#include <unordered_map>
#include <vector>

#include "ConfigLoader.hpp"
#include "DelayWidget.hpp"
#include "EditorWidget.hpp"
#include "ExecutionWidget.hpp"
#include "FileWidget.hpp"
#include "GameEnums.hpp"
#include "KeyShortcut.hpp"
#include "NoiseWidget.hpp"
#include "ResizeWidget.hpp"
#include "SimulationControlResult.hpp"
#include "StepWidget.hpp"
#include "WidgetResult.hpp"

namespace gol {
struct ButtonlessShortcuts : public Widget {
    std::unordered_map<ActionVariant, std::vector<KeyShortcut>> Shortcuts;

    ButtonlessShortcuts(const std::vector<ImGuiKeyChord> &left = {},
                        const std::vector<ImGuiKeyChord> &right = {},
                        const std::vector<ImGuiKeyChord> &up = {},
                        const std::vector<ImGuiKeyChord> &down = {},
                        const std::vector<ImGuiKeyChord> &close = {});

    WidgetResult UpdateImpl(const EditorResult &state);

    void SetShortcuts(const ShortcutMap &shortcuts);
};

class SimulationControl {
  public:
    SimulationControl(const ConfigLoader::StyleInfo<ImVec4> &fileInfo);

    SimulationControlResult Update(const EditorResult &state);

  private:
    void TryUpdateShortcuts(std::stop_token stopToken);

    void ForEachWidget(auto &&widgetCall);
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

    std::filesystem::path m_ShortcutConfigPath;
    std::jthread m_ShortcutChecker;
    std::atomic<bool> m_UpdateShortcuts;
};

void SimulationControl::ForEachWidget(auto&& widgetFunc) {
    widgetFunc(m_ButtonlessShortcuts);
    widgetFunc(m_FileWidget);
    widgetFunc(m_ExecutionWidget);
    widgetFunc(m_EditorWidget);
    widgetFunc(m_ResizeWidget);
    widgetFunc(m_StepWidget);
    widgetFunc(m_DelayWidget);
    widgetFunc(m_NoiseWidget);
}
} // namespace gol

#endif
