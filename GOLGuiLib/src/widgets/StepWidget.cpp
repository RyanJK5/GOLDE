#include <algorithm>
#include <font-awesome/IconsFontAwesome7.h>
#include <imgui/imgui.h>
#include <string>
#include <utility>

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

template <std::integral T> constexpr static T IntPow(T base, T exponent) {
    T res = 1;
    while (exponent > 0) {
        if (exponent % 2 == 1)
            res *= base;
        base *= base;
        exponent /= 2;
    }
    return res;
}

void StepWidget::SetStepCount(int64_t stepCount) {
    m_StepCount = std::max(static_cast<int64_t>(1), stepCount);
    auto [end, error] = std::to_chars(m_InputText.Data,
                                      m_InputText.Data + m_InputText.Length + 1,
                                      static_cast<int64_t>(m_StepCount));
    assert(error == std::errc{});
    *end = '\0';
}

void StepWidget::ShowInputText() {
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2.f + 5);

    if (ImGui::InputText("##expr", m_InputText.Data, m_InputText.Length + 1)) {
        const auto stepCountBefore = m_StepCount;
        try {
            using std::ranges::all_of;
            constexpr static auto validChar = [](char c) {
                return std::isdigit(c) || std::isspace(c);
            };

            std::string str{m_InputText.Data};
            str = str.substr(str.find_first_not_of(' '));
            if (const auto index = str.find('^'); index != std::string::npos) {
                const auto baseStr = str.substr(0, index);
                const auto exponentStr = str.substr(index + 1);

                if (!all_of(baseStr, validChar) ||
                    !all_of(exponentStr, validChar))
                    throw std::invalid_argument{""};

                const auto base = std::stoll(baseStr);
                const auto exponent = std::stoll(exponentStr);

                m_StepCount = std::max(1LL, IntPow(base, exponent));
            } else {
                if (!all_of(str, validChar))
                    throw std::invalid_argument{""};
                m_StepCount = std::max(1LL, std::stoll(str));
            }

            auto [end, error] = std::to_chars(
                m_InputText.Data, m_InputText.Data + m_InputText.Length + 1,
                static_cast<int64_t>(m_StepCount));
            assert(error == std::errc{});
            *end = '\0';
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
    constexpr static auto beginGreyOutIf = [](bool condition) {
        if (condition) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                                ImGui::GetStyle().Alpha * 0.5f);
        }
    };

    constexpr static auto endGreyOutIf = [](bool condition) {
        if (condition) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
    };

    ImGui::Text("Step Count");

    beginGreyOutIf(m_HyperSpeed);
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
    endGreyOutIf(m_HyperSpeed);

    if (m_StepCount < 1)
        m_StepCount = 1;

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);
    auto result = m_Button.Update(state);

    auto retValue = WidgetResult{
        .Command = result.Action ? std::make_optional(ToCommand(*result.Action))
                                 : std::nullopt,
        .FromShortcut = result.FromShortcut};

    const bool showAlgoDropbox =
        state.Simulation.State != SimulationState::Empty &&
        state.Simulation.State != SimulationState::Paint &&
        state.Simulation.State != SimulationState::Paused;
    beginGreyOutIf(showAlgoDropbox);

    auto algoID = std::to_underlying(m_Algorithm);
    ImGui::Combo("Algorithm", &algoID, "HashLife\0SparseLife\0");
    const auto algo = static_cast<LifeAlgorithm>(algoID);

    endGreyOutIf(showAlgoDropbox);

    if (algo == LifeAlgorithm::HashLife)
        ImGui::SetItemTooltip(
            "HashLife is an optimized algorithm that can quickly compute "
            "generations for large\n"
            "patterns. It may be slower for small or chaotic patterns.");
    else if (algo == LifeAlgorithm::SparseLife)
        ImGui::SetItemTooltip(
            "SparseLife is a straightforward algorithm that "
            "computes each generation iteratively.\n"
            "It may be faster for small or chaotic patterns, but "
            "slower for larger ones.");

    retValue.FromShortcut = retValue.FromShortcut || result.FromShortcut;
    m_Algorithm = algo;

    const bool showHyperSpeedOption =
        state.Simulation.State == SimulationState::Simulation;
    beginGreyOutIf(showHyperSpeedOption);

    ImGui::Checkbox("Enable Hyper Speed", &m_HyperSpeed);
    ImGui::SetItemTooltip("Hyper Speed enables the HashLife algorithm to "
                          "progress as fast as possible. However, this means\n"
                          "it may skip several generations at a time, and step "
                          "count will be ignored. Expect the simulation\n"
                          "to run slowly for the first few jumps, but speed up "
                          "significantly afterwards.");

    endGreyOutIf(showHyperSpeedOption);

    ImGui::Separator();
    ImGui::PopStyleVar();

    return retValue;
}
} // namespace gol