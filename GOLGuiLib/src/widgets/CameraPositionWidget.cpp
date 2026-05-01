#include "CameraPositionWidget.hpp"

#include <array>

namespace Golde {
WidgetResult CameraPositionWidget::UpdateImpl(const EditorResult& info) {
    constexpr static auto basePixelsPerCellAtZoom1 = 20.f;

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
    const bool showPixelsPerCell = (inputPixelsPerCell >= 1.f);

    const auto separatorWidth = ImGui::CalcTextSize(" : ").x;
    const auto totalSpacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
    const auto availableForInputs =
        ImGui::GetContentRegionAvail().x - separatorWidth - totalSpacing;
    const auto inputWidth = availableForInputs * 0.5f;

    // Canonical display: either (pixelsPerCell : 1) or (1 : cellsPerPixel)
    auto leftValue = showPixelsPerCell ? inputPixelsPerCell : 1.f;
    auto rightValue = showPixelsPerCell ? 1.f : (1.f / inputPixelsPerCell);

    constexpr static auto formatFor = [](float v) {
        if (v == 1.f)
            return "%.0f";
        if (v <= 100.f)
            return "%.2f";
        return "%.2e";
    };

    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputFloat("##CameraZoomLeft", &leftValue, 0.f, 0.f,
                      formatFor(leftValue));
    ImGui::SetItemTooltip("Zoom is represented as pixels : cells.");
    auto leftReturn = [&]() -> WidgetResult {
        if (ImGui::IsItemDeactivatedAfterEdit() && leftValue > 0.f) {
            const auto newZoom = [&] {
                if (showPixelsPerCell) {
                    // Canonical side: leftValue is the new pixelsPerCell
                    return leftValue / BasePixelsPerCellAtZoom1;
                } else {
                    // "1" side: user entered N, ratio 1:cellsPerPixel scaled to
                    // N:cellsPerPixel, normalized back to 1:(cellsPerPixel/N),
                    // i.e. zoom scales by N
                    return leftValue * inputPixelsPerCell /
                           BasePixelsPerCellAtZoom1;
                }
            }();
            if (newZoom != info.Zoom && newZoom > 0.f)
                return WidgetResult{.Command =
                                        CameraZoomCommand{.Zoom = newZoom}};
        }
        return {};
    }();

    ImGui::SameLine();
    ImGui::Text(" : ");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputFloat("##CameraZoomRight", &rightValue, 0.f, 0.f,
                      formatFor(rightValue));
    ImGui::SetItemTooltip("Zoom is represented as pixels : cells.");
    auto rightReturn = [&]() -> WidgetResult {
        if (ImGui::IsItemDeactivatedAfterEdit() && rightValue > 0.f) {
            float newZoom;
            if (!showPixelsPerCell) {
                // Canonical side: rightValue is the new cellsPerPixel
                newZoom = 1.f / (rightValue * BasePixelsPerCellAtZoom1);
            } else {
                // "1" side: user entered N, ratio pixelsPerCell:N normalized to
                // (pixelsPerCell/N):1
                newZoom = (inputPixelsPerCell / rightValue) /
                          BasePixelsPerCellAtZoom1;
            }
            if (newZoom != info.Zoom && newZoom > 0.f)
                return WidgetResult{.Command =
                                        CameraZoomCommand{.Zoom = newZoom}};
        }
        return {};
    }();

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
} // namespace Golde