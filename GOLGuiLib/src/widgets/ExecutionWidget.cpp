#include <font-awesome/IconsFontAwesome7.h>
#include <imgui.h>
#include <optional>
#include <string>
#include <utility>

#include "ActionButton.hpp"
#include "ExecutionWidget.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "WidgetResult.hpp"

namespace gol {
Size2F StartButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 4.f,
            MultiActionButton::DefaultButtonHeight()};
}
std::string StartButton::Label(const EditorResult& state) const {
    switch (state.Simulation.State) {
        using enum SimulationState;
    case Simulation:
        return ICON_FA_PAUSE;
    default:
        return ICON_FA_PLAY;
    }
}
GameAction StartButton::Action(const EditorResult& state) const {
    switch (state.Simulation.State) {
        using enum SimulationState;
    case Paint:
        return GameAction::Start;
    case Paused:
        return GameAction::Resume;
    case Simulation:
        return GameAction::Pause;
    default:
        assert(false && "Illegal simulation state passed to StartButton");
    }
    std::unreachable();
}
bool StartButton::Enabled(const EditorResult& state) const {
    return state.Simulation.State == SimulationState::Paint ||
           state.Simulation.State == SimulationState::Paused ||
           state.Simulation.State == SimulationState::Simulation;
}

Size2F ResetButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 3.f,
            ActionButton::DefaultButtonHeight()};
}
std::string ResetButton::Label(const EditorResult&) const {
    return ICON_FA_STOP;
}
bool ResetButton::Enabled(const EditorResult& state) const {
    return state.Simulation.State == SimulationState::Simulation ||
           state.Simulation.State == SimulationState::Paused ||
           state.Simulation.State == SimulationState::Stepping;
}

Size2F RestartButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 2.f,
            ActionButton::DefaultButtonHeight()};
}
std::string RestartButton::Label(const EditorResult&) const {
    return ICON_FA_REPEAT;
}
bool RestartButton::Enabled(const EditorResult& state) const {
    return state.Simulation.State == SimulationState::Simulation ||
           state.Simulation.State == SimulationState::Paused;
}

Size2F ClearButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x,
            ActionButton::DefaultButtonHeight()};
}
std::string ClearButton::Label(const EditorResult&) const {
    return ICON_FA_TRASH;
}
bool ClearButton::Enabled(const EditorResult& state) const {
    return state.Simulation.State == SimulationState::Paint ||
           state.Simulation.State == SimulationState::Paused ||
           state.Simulation.State == SimulationState::Simulation;
}

WidgetResult ExecutionWidget::UpdateImpl(const EditorResult& state) {
    auto result = WidgetResult{};
    UpdateResult(result, m_StartButton.Update(state));
    UpdateResult(result, m_ResetButton.Update(state));
    UpdateResult(result, m_RestartButton.Update(state));

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, ImGui::GetFontSize());
    UpdateResult(result, m_ClearButton.Update(state));
    ImGui::Separator();
    ImGui::PopStyleVar();

    return result;
}

void ExecutionWidget::SetShortcutsImpl(const ShortcutMap& shortcuts) {
    m_StartButton.SetShortcuts(
        {{GameAction::Start,
          shortcuts.at(GameAction::Start) | KeyShortcut::MapChordsToVector},
         {GameAction::Pause,
          shortcuts.at(GameAction::Pause) | KeyShortcut::MapChordsToVector},
         {GameAction::Resume,
          shortcuts.at(GameAction::Resume) | KeyShortcut::MapChordsToVector}});
    m_ClearButton.SetShortcuts(shortcuts.at(GameAction::Clear));
    m_ResetButton.SetShortcuts(shortcuts.at(GameAction::Reset));
    m_RestartButton.SetShortcuts(shortcuts.at(GameAction::Restart));
}
} // namespace gol
