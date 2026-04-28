#include "CameraPositionWidget.hpp"

#include <array>

namespace gol {
WidgetResult CameraPositionWidget::UpdateImpl(const EditorResult& info) {
    constexpr float BasePixelsPerCellAtZoom1 = 20.f;

    std::array data{m_Position.X, m_Position.Y};

    ImGui::Text("Camera Position");

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x / 3.f * 2.f + 5);
    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding,
                         Widget::DefaultInputPadding());

    ImGui::InputInt2("##CameraPositionLabel", data.data());
    m_Position.X = data[0];
    m_Position.Y = data[1];

    ImGui::SameLine();

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, ImGui::GetFontSize());
    auto ret = [&] {
        if (ImGui::Button("Travel", {ImGui::GetContentRegionAvail().x, 0})) {
            return WidgetResult{.Command = CameraPositionCommand{m_Position}};
        }
        return WidgetResult{};
    }();
    ImGui::PopStyleVar();

    ImGui::SetItemTooltip("Move the camera to the specified position.");

    ImGui::Text("Camera Scale");

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, ImGui::GetFontSize());

    const auto inputPixelsPerCell = BasePixelsPerCellAtZoom1 * info.Zoom;
    auto pixelsPerCell = inputPixelsPerCell;

    const bool leftDisabled = pixelsPerCell < 1.f;
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
        const char* enabledFormat = (pixelsPerCell <= 100.f) ? "%.2f" : "%.2e";
        ImGui::InputFloat(
            "##CameraZoomLabel", leftDisabled ? &one : &pixelsPerCell, 0.f, 0.f,
            (leftDisabled || pixelsPerCell == 1.f) ? "%.0f" : enabledFormat);
        if (ImGui::IsItemDeactivatedAfterEdit() &&
            pixelsPerCell != inputPixelsPerCell && pixelsPerCell > 0.f) {
            return WidgetResult{
                .Command = CameraZoomCommand{.Zoom = pixelsPerCell /
                                                     BasePixelsPerCellAtZoom1}};
        }
        return WidgetResult{};
    }();
    ImGui::PopItemFlag();

    ImGui::SameLine();
    ImGui::Text(" : ");
    ImGui::SameLine();

    auto cellsPerPixel = 1.f / inputPixelsPerCell;
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !leftDisabled);
    ImGui::SetNextItemWidth(inputWidth); // Apply the calculated half-width

    auto rightReturn = [&] {
        const char* enabledFormat = (cellsPerPixel <= 100.f) ? "%.2f" : "%.2e";
        ImGui::InputFloat(
            "##RatioLabel", leftDisabled ? &cellsPerPixel : &one, 0.f, 0.f,
            (leftDisabled || cellsPerPixel == 1.f) ? enabledFormat : "%.0f");
        if (ImGui::IsItemDeactivatedAfterEdit() && cellsPerPixel > 0.f &&
            cellsPerPixel != 1.f / inputPixelsPerCell) {
            return WidgetResult{
                .Command = CameraZoomCommand{
                    .Zoom = 1.f / (cellsPerPixel * BasePixelsPerCellAtZoom1)}};
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