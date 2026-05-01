#include "LoadingSpinner.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numbers>

namespace Golde {
void LoadingSpinner(const char* label, float radius, float thickness,
                    ImU32 color) {
    auto* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    const auto& style = GImGui->Style;
    const auto id = window->GetID(label);

    const auto pos = window->DC.CursorPos;
    const ImVec2 size{radius * 2 + thickness, radius * 2 + thickness};

    const ImRect bounds{pos, ImVec2{pos.x + size.x, pos.y + size.y}};
    ImGui::ItemSize(bounds, style.FramePadding.y);
    if (!ImGui::ItemAdd(bounds, id))
        return;

    window->DrawList->PathClear();

    constexpr static auto numSegments = 30;
    const auto time = static_cast<float>(GImGui->Time);
    const auto start = static_cast<int32_t>(
        std::abs(std::sinf(time * 1.8f) * (numSegments - 5)));

    constexpr static auto pi = std::numbers::pi_v<float>;
    const auto a_min = pi * 2.f * start / numSegments;
    const auto a_max = pi * 2.f * (numSegments - 3.f) / numSegments;

    const ImVec2 centre{pos.x + radius, pos.y + radius + style.FramePadding.y};

    for (auto i = 0; i < numSegments; i++) {
        const auto a =
            a_min + (i / static_cast<float>(numSegments)) * (a_max - a_min);
        window->DrawList->PathLineTo(
            ImVec2{centre.x + std::cosf(a + time * 8) * radius,
                   centre.y + std::sinf(a + time * 8) * radius});
    }

    window->DrawList->PathStroke(color, false, thickness);
}
} // namespace Golde
