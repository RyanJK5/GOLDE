#include <string_view>

#include <imgui.h>

#include "PopupWindow.hpp"

namespace Golde {
PopupWindow::PopupWindow(std::string_view title,
                         std::function<void(PopupWindowState)> onUpdate)
    : Message(""), m_UpdateCallback(onUpdate), m_Title(title) {}

void PopupWindow::Update() {
    if (!Active)
        return;

    ImGui::OpenPopup(m_Title.c_str());
    ImGui::BeginPopupModal(m_Title.c_str(), nullptr,
                           ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoResize);

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, ImGui::GetFontSize());
    ImGui::Text("%s", Message.c_str());
    ImGui::PopStyleVar();

    const auto result = ShowButtons();
    if (result) {
        m_UpdateCallback(*result);
        Active = false;
    }

    ImGui::EndPopup();
}

void PopupWindow::SetCallback(std::function<void(PopupWindowState)> onUpdate) {
    m_UpdateCallback = onUpdate;
}

} // namespace Golde
