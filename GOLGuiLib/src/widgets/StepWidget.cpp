#include <algorithm>
#include <charconv>
#include <font-awesome/IconsFontAwesome7.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <string>
#include <utility>

#include "DisabledScope.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "SimulationCommand.hpp"
#include "StepWidget.hpp"
#include "WidgetResult.hpp"

namespace gol {
Size2F StepButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x,
            ActionButton::DefaultButtonHeight};
}
std::string StepButton::Label(const EditorResult&) const {
    return ICON_FA_FORWARD_STEP;
}
bool StepButton::Enabled(const EditorResult& state) const {
    return state.Simulation.State == SimulationState::Paint ||
           state.Simulation.State == SimulationState::Paused;
}

void StepWidget::SetStepCount(const BigInt& stepCount) {
    m_StepCount = std::max(BigOne, stepCount);
    m_InputText = std::format("{}", stepCount);
}

void StepWidget::ShowInputText() {
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2.f + 5);

    if (ImGui::InputText("##expr", &m_InputText)) {
        const auto stepCountBefore = m_StepCount;
        try {
            constexpr static auto validChar = [](char c) {
                return std::isdigit(c) || std::isspace(c);
            };

            std::string_view str{m_InputText};

            using std::ranges::all_of;
            str = str.substr(str.find_first_not_of(' '));
            if (const auto index = str.find('^'); index != std::string::npos) {
                const auto baseStr = str.substr(0, index);
                const auto expStr = str.substr(index + 1);

                if (!all_of(baseStr, validChar) || !all_of(expStr, validChar)) {
                    throw std::invalid_argument{""};
                }

                int64_t base{};
                const auto [baseEnd, baseError] = std::from_chars(
                    baseStr.data(), baseStr.data() + baseStr.size(), base);

                int32_t exponent{};
                [[maybe_unused]] const auto [expEnd, expError] =
                    std::from_chars(expStr.data(),
                                    expStr.data() + expStr.size(), exponent);

                if (baseError != std::errc{} || expError != std::errc{}) {
                    SetStepCount(stepCountBefore);
                } else {
                    const BigInt input{boost::multiprecision::pow(
                        BigInt{base}, static_cast<unsigned int>(exponent))};
                    m_StepCount = std::max(BigOne, input);
                }

            } else {
                if (!all_of(str, validChar)) {
                    throw std::invalid_argument{""};
                }

                const static auto isSpace = [](char c) {
                    return std::isspace(c);
                };
                m_StepCount.assign(
                    std::string{std::ranges::find_if_not(str, isSpace),
                                std::ranges::find_if_not(
                                    std::ranges::reverse_view(str), isSpace)
                                    .base()});
            }

            m_InputText = std::format("{}", m_StepCount);
        } catch (const std::exception&) {
            SetStepCount(stepCountBefore);
        }
    }
    ImGui::SetItemTooltip("Enter the number of steps to advance. You can also "
                          "enter an expression in the\n"
                          "form of 'x^y' to compute large step counts quickly. "
                          "For example, '2^10' will\n"
                          "set the step count to 1024.");
    ImGui::PopItemWidth();
}

WidgetResult StepWidget::UpdateImpl(const EditorResult& state) {
    ImGui::Text("Step Count");

    {
        DisabledScope disableIf{m_HyperSpeed};

        ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.f);

        ShowInputText();

        const auto buttonSize = ImGui::GetFrameHeight();

        ImGui::SameLine();
        if (ImGui::Button("-##minus", {buttonSize, buttonSize}))
            SetStepCount(m_StepCount - 1);

        ImGui::SameLine();
        if (ImGui::Button("+##plus", {buttonSize, buttonSize}))
            SetStepCount(m_StepCount + 1);

        ImGui::PopStyleVar();
    }

    if (m_StepCount < 1)
        m_StepCount = 1;

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);
    auto result = m_Button.Update(state);

    auto retValue = WidgetResult{
        .Command = result.Action ? std::make_optional(ToCommand(*result.Action))
                                 : std::nullopt,
        .FromShortcut = result.FromShortcut};

    retValue.FromShortcut = retValue.FromShortcut || result.FromShortcut;

    {
        const bool hideHyperSpeedOption =
            state.Simulation.State == SimulationState::Simulation;
        DisabledScope disableIf(hideHyperSpeedOption);

        ImGui::Checkbox("Enable Hyper Speed", &m_HyperSpeed);
        ImGui::SetItemTooltip(
            "Hyper Speed enables the HashLife algorithm to "
            "progress as fast as possible. However, this means\n"
            "it may skip several generations at a time, and step "
            "count will be ignored. Expect the simulation\n"
            "to run slowly for the first few jumps, but speed up "
            "significantly afterwards.");
    }

    ImGui::Separator();
    ImGui::PopStyleVar();

    return retValue;
}
} // namespace gol
