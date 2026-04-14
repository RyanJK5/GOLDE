#include "CameraPositionWidget.hpp"

#include <array>

namespace gol {
WidgetResult CameraPositionWidget::UpdateImpl(const EditorResult& info) {
    std::array data{m_Position.X, m_Position.Y};

    ImGui::Text("Camera Position");

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x / 3.f * 2.f + 5);
    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.f);

    ImGui::InputInt2("##CameraPositionLabel", data.data());
    m_Position.X = data[0];
    m_Position.Y = data[1];

    ImGui::SameLine();

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);
    auto ret = [&] {
        if (ImGui::Button("Travel", {ImGui::GetContentRegionAvail().x, 0})) {
            return WidgetResult{.Command = CameraPositionCommand{m_Position}};
        }
        return WidgetResult{};
    }();
    ImGui::PopStyleVar();

    ImGui::SetItemTooltip("Move the camera to the specified position.");

    ImGui::Text("Camera Scale");

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);

    const auto inputScale = 20.f / info.Zoom;
    auto scale = inputScale;

    const bool leftDisabled = scale < 1.f;
    const auto leftID = ImGui::GetID("##CameraZoomLabel");
    const auto rightID = ImGui::GetID("##RatioLabel");

    if (leftDisabled && ImGui::GetActiveID() == leftID) {
        ImGui::ClearActiveID();
    }
    if (!leftDisabled && ImGui::GetActiveID() == rightID) {
        ImGui::ClearActiveID();
    }

    const auto separatorWidth = ImGui::CalcTextSize(" : ").x;
    const auto totalSpacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
    const auto availableForInputs =
        ImGui::GetContentRegionAvail().x - separatorWidth - totalSpacing;
    const auto inputWidth = availableForInputs * 0.5f;

    auto one = 1.f;

    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, leftDisabled);
    ImGui::SetNextItemWidth(inputWidth); // Apply the calculated half-width

    auto leftReturn = [&] {
        const bool input =
            ImGui::InputFloat("##CameraZoomLabel", leftDisabled ? &one : &scale,
                              0.f, 0.f, leftDisabled ? "%.0f" : "%.2e",
                              ImGuiInputTextFlags_EnterReturnsTrue);
        if (input && scale != inputScale) {
            return WidgetResult{.Command =
                                    CameraZoomCommand{.Zoom = 20.f / scale}};
        }
        return WidgetResult{};
    }();
    ImGui::PopItemFlag();

    ImGui::SameLine();
    ImGui::Text(" : ");
    ImGui::SameLine();

    auto zoom = info.Zoom / 20.f;
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !leftDisabled);
    ImGui::SetNextItemWidth(inputWidth); // Apply the calculated half-width

    auto rightReturn = [&] {
        const bool input =
            ImGui::InputFloat("##RatioLabel", leftDisabled ? &zoom : &one, 0.f,
                              0.f, leftDisabled ? "%.2e" : "%.0f",
                              ImGuiInputTextFlags_EnterReturnsTrue);
        if (input && zoom != info.Zoom / 20.f) {
            return WidgetResult{.Command =
                                    CameraZoomCommand{.Zoom = zoom * 20.f}};
        }
        return WidgetResult{};
    }();
    ImGui::PopItemFlag();

    ImGui::Separator();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    if (ret.Command) {
        return ret;
    } else if (leftReturn.Command) {
        return leftReturn;
    } else if (rightReturn.Command) {
        return rightReturn;
    } else {
        return WidgetResult{};
    }
}
} // namespace gol