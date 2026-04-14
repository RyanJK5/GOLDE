#include "RuleWidget.hpp"
#include "DisabledScope.hpp"
#include "LifeRule.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace gol {
RuleWidget::RuleWidget()
    : m_InputError("Invalid Rule",
                   [this](auto) { m_InputText = m_LastValid; }) {}

std::optional<Size2> RuleWidget::ResizeComponent(const EditorResult& state) {
    const auto totalWidth = ImGui::GetContentRegionAvail().x / 2.f;

    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.f);
    ImGui::SetNextItemWidth(totalWidth);

    const auto dimensions =
        LifeRule::ExtractDimensions(m_InputText).value_or(Size2{});
    std::array wrapper{dimensions.Width, dimensions.Height};
    ImGui::InputInt2("##label", wrapper.data());
    ImGui::SetItemTooltip("If either width or height is set to zero, the "
                          "universe will be unbounded.");

    const auto ret = [&] -> std::optional<Size2> {
        if (wrapper[0] != dimensions.Width || wrapper[1] != dimensions.Height) {
            return Size2{wrapper[0], wrapper[1]};
        }
        return std::nullopt;
    }();

    ImGui::PopStyleVar();
    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);

    ImGui::SetCursorPosX(ImGui::GetStyle().FramePadding.x * 3);

    ImGui::Text("Width");
    ImGui::SameLine();
    ImGui::SetCursorPosX(totalWidth / 2.f +
                         ImGui::GetStyle().FramePadding.x * 3);
    ImGui::Text("Height");

    ImGui::Separator();
    ImGui::PopStyleVar();

    return ret;
}

WidgetResult RuleWidget::UpdateImpl(const EditorResult& state) {
    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.f);
    ImGui::Text("Rule");

    const bool pressedEnter = ImGui::InputTextWithHint(
        "##RuleWidgetInput", "Enter Rule...", &m_InputText,
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SetItemTooltip(
        "Enter any outer-totalistic rule, which is defined as "
        "\"B1...8/S0...8\". The digits after 'B'\n"
        "specify the number of neighbors a cell can have to be born, and the "
        "digits after 'S'\n"
        "specify the number of neighbors a cell can have to survive to the "
        "next generation.\n"
        "Conway's Game of Life is therefore defined as B3/S23. Additional "
        "properties"
        " can\nbe modified below.");

    ImGui::SameLine();

    const bool pressedButton = [&] {
        DisabledScope disableIf{!Actions::Editable(state.Simulation.State)};
        return ImGui::Button("Set Rule",
                             ImVec2{ImGui::GetContentRegionAvail().x, 0});
    }();

    ImGui::PopStyleVar();

    m_InputError.Update();

    auto changed = ResizeComponent(state);
    if (changed) {
        auto suffix = [&] -> std::string {
            if (changed->Width == 0 && changed->Height == 0) {
                return "";
            }
            return std::format(":P{},{}", changed->Width, changed->Height);
        }();

        const auto replaceIndex = [&] {
            const auto ret = m_InputText.find(':');
            if (ret != std::string::npos) {
                return ret;
            }
            return m_InputText.size();
        }();
        m_InputText.replace(
            replaceIndex,
            std::max(suffix.size(), m_InputText.size() - replaceIndex),
            std::move(suffix));
    }

    if (!Actions::Editable(state.Simulation.State) ||
        (!pressedEnter && !pressedButton)) {
        return {};
    }

    const auto validRule = LifeRule::IsValidRule(m_InputText);
    if (!validRule) {
        m_InputError.Message = validRule.error();
        m_InputError.Activate();
        return {};
    }

    m_LastValid = m_InputText;
    return WidgetResult{.Command = RuleCommand{m_InputText}};
}
} // namespace gol