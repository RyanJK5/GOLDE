#include <cstdint>
#include <imgui.h>

#include "GameEnums.hpp"
#include "ResizeWidget.hpp"
#include "SimulationCommand.hpp"
#include "WidgetResult.hpp"

namespace gol {
WidgetResult ResizeWidget::UpdateImpl(const EditorResult& state) {
    const float totalWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetStyle().FramePadding.x * 3);
    ImGui::Text("Width");
    ImGui::SameLine();
    ImGui::SetCursorPosX(totalWidth / 3.f +
                         ImGui::GetStyle().FramePadding.x * 3);
    ImGui::Text("Height");

    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x / 3.f * 2.f + 5);

    int32_t wrapper[2] = {m_Dimensions.Width, m_Dimensions.Height};
    ImGui::InputInt2("##label", wrapper);
    ImGui::SetItemTooltip("If either width or height is set to zero, the "
                          "universe will be unbounded.");
    m_Dimensions = {std::clamp(wrapper[0], 0, 1000000),
                    std::clamp(wrapper[1], 0, 1000000)};

    ImGui::PopStyleVar();

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);

    auto result = m_Button.Update(state);
    if (result.Action == EditorAction::Resize &&
        (m_Dimensions.Width == 0 || m_Dimensions.Height == 0)) {
        m_Dimensions.Width = 0;
        m_Dimensions.Height = 0;
    }

    ImGui::Separator();
    ImGui::PopStyleVar();

    if (result.Action)
        return {.Command = ResizeCommand{m_Dimensions},
                .FromShortcut = result.FromShortcut};
    return {};
}

void ResizeWidget::SetShortcutsImpl(const ShortcutMap& shortcuts) {
    m_Button.SetShortcuts(shortcuts.at(EditorAction::Resize));
}
} // namespace gol
