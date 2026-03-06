#include "NoiseWidget.hpp"

namespace gol {
Size2F GenerateNoiseButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x,
            ActionButton::DefaultButtonHeight};
}

std::string GenerateNoiseButton::Label(const EditorResult &) const {
    return "Generate Noise";
}

bool GenerateNoiseButton::Enabled(const EditorResult &state) const {
    return (state.State == SimulationState::Empty ||
            state.State == SimulationState::Paint) &&
           state.SelectionActive;
}

SimulationControlResult NoiseWidget::UpdateImpl(const EditorResult &state) {
    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.f);

    ImGui::Text("Noise Density");

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

    ImGui::InputFloat("##label", &m_Density, 0.001f, 1.f);
    ImGui::SetItemTooltip(
        "Fill the selection area with randomly generated live cells. The\n"
        "density determines the probability of each cell being alive.");

    m_Density = std::clamp(m_Density, 0.f, 1.f);

    ImGui::PopItemWidth();

    const auto result = m_Button.Update(state);

    ImGui::PopStyleVar();

    return SimulationControlResult{.Action = result.Action,
                                   .NoiseDensity = m_Density,
                                   .FromShortcut = result.FromShortcut};
}

void NoiseWidget::SetShortcutsImpl(const ShortcutMap &shortcuts) {
    m_Button.SetShortcuts(shortcuts.at(EditorAction::GenerateNoise));
}

} // namespace gol