#include "RuleWidget.hpp"
#include "DisabledScope.hpp"
#include "LifeRule.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace gol {
RuleWidget::RuleWidget()
    : m_TopologyCombo("##TopologyLabel", "Plane", "Torus"),
      m_InputError("Invalid Rule",
                   [this](auto) { m_InputText = m_LastValid; }) {}

RuleWidget::RuleInfoChange RuleWidget::ResizeComponent(const EditorResult&) {
    const auto totalWidth = ImGui::GetContentRegionAvail().x / 2.f;

    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding,
                         Widget::DefaultInputPadding());
    ImGui::SetNextItemWidth(totalWidth);

    const auto dimensions =
        LifeRule::ExtractDimensions(m_InputText).value_or(Size2{});
    std::array wrapper{dimensions.Width, dimensions.Height};
    ImGui::InputInt2("##BoundsLabel", wrapper.data());
    ImGui::SetItemTooltip("If a dimension is set to 0, the universe will "
                          "unbounded along that dimension.");

    ImGui::SameLine();

    const auto newActiveTopology = [&] -> std::optional<TopologyKind> {
        const auto dimensions = LifeRule::ExtractDimensions(m_InputText);
        DisabledScope disableIf{
            !dimensions || (dimensions->Width == 0 && dimensions->Height == 0)};

        const auto oldActiveIndex = m_TopologyCombo.ActiveIndex;
        m_TopologyCombo.ActiveIndex =
            static_cast<int32_t>(LifeRule::ExtractTopologyKind(m_InputText)
                                     .value_or(TopologyKind::Plane));
        m_TopologyCombo.Update();

        switch (static_cast<TopologyKind>(m_TopologyCombo.ActiveIndex)) {
        case TopologyKind::Plane:
            ImGui::SetItemTooltip("The standard topology for both unbounded "
                                  "and bounded universes.");
            break;
        case TopologyKind::Torus:
            ImGui::SetItemTooltip("Creates a looping universe where cells "
                                  "re-enter on the opposite side they exited.");
        }

        if (oldActiveIndex != m_TopologyCombo.ActiveIndex) {
            return static_cast<TopologyKind>(m_TopologyCombo.ActiveIndex);
        }
        return std::nullopt;
    }();

    const auto newSize = [&] -> std::optional<Size2> {
        if (wrapper[0] != dimensions.Width || wrapper[1] != dimensions.Height) {
            return Size2{wrapper[0], wrapper[1]};
        }
        return std::nullopt;
    }();

    ImGui::PopStyleVar();
    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, ImGui::GetFontSize());

    ImGui::SetCursorPosX(ImGui::GetStyle().FramePadding.x * 3);

    ImGui::Text("Width");
    ImGui::SameLine();
    ImGui::SetCursorPosX(totalWidth / 2.f +
                         ImGui::GetStyle().FramePadding.x * 3);
    ImGui::Text("Height");
    ImGui::SameLine();
    ImGui::SetCursorPosX(totalWidth + ImGui::GetStyle().FramePadding.x * 4);
    ImGui::Text("Topology");

    ImGui::Separator();
    ImGui::PopStyleVar();

    return {.NewSize = newSize, .NewTopologyKind = newActiveTopology};
}

WidgetResult RuleWidget::UpdateImpl(const EditorResult& state) {
    if (state.Simulation.RuleString != m_LastValid) {
        m_LastValid = state.Simulation.RuleString;
        m_InputText = m_LastValid;
    }

    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding,
                         Widget::DefaultInputPadding());
    ImGui::TextUnformatted(HasPendingRuleChange() ? "Rule*" : "Rule");

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
    if (changed.NewSize) {
        auto suffix = [&] -> std::string {
            if (changed.NewSize->Width == 0 && changed.NewSize->Height == 0) {
                return "";
            }

            auto topologyString = [&] -> std::string_view {
                const auto colonIndex = m_InputText.find(':');
                if (colonIndex == std::string::npos) {
                    return ":P";
                }
                return std::string_view{m_InputText.begin() + colonIndex,
                                        m_InputText.begin() + colonIndex + 2};
            }();

            return std::format("{}{},{}", topologyString,
                               changed.NewSize->Width, changed.NewSize->Height);
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
    if (changed.NewTopologyKind) {
        const auto colonIndex = m_InputText.find(':');
        if (colonIndex != std::string::npos) {
            m_InputText[colonIndex + 1] = [&] {
                switch (*changed.NewTopologyKind) {
                case TopologyKind::Plane:
                    return 'P';
                case TopologyKind::Torus:
                    return 'T';
                default:
                    return '?';
                }
            }();
        }
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

    m_InputText = *LifeRule::Canonicalize(m_InputText);
    m_LastValid = m_InputText;
    return WidgetResult{.Command = RuleCommand{m_InputText}};
}
} // namespace gol