#include <algorithm>
#include <imgui/imgui.h>

#include "DelayWidget.hpp"
#include "GameEnums.hpp"
#include "WidgetResult.hpp"

namespace gol {

WidgetResult DelayWidget::UpdateImpl(const EditorResult&) {

    ImGui::Text("Simulation Delay (ms)");
    ImGui::SetItemTooltip(
        "The delay between each step while the simulation is running.");

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);
    ImGui::SliderInt("##label test", &m_TickDelayMs, 0, 1000);
    ImGui::SetItemTooltip("Ctrl + Click to input value");
    ImGui::PopStyleVar();

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);
    ImGui::Checkbox("Show Grid Lines", &m_GridLines);

    ImGui::Separator();
    ImGui::PopStyleVar();

    m_TickDelayMs = std::max(m_TickDelayMs, 0);
    return {};
}

} // namespace gol
