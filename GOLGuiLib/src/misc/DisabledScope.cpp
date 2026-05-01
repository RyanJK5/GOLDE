#include <imgui.h>

#include "DisabledScope.hpp"

namespace Golde {
DisabledScope::DisabledScope(bool disabled) : m_Disabled(disabled) {
    if (m_Disabled) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
    }
}

DisabledScope::~DisabledScope() {
    if (m_Disabled) {
        ImGui::PopStyleVar();
        ImGui::PopItemFlag();
    }
}
} // namespace Golde