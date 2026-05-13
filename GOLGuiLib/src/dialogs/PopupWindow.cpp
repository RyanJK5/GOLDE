#include <string_view>

#include <imgui.h>

#include "PopupWindow.hpp"

namespace Golde {
void PopupWindow::Update() {
    if (!Active)
        return;

    ImGui::OpenPopup(Title.c_str());
    ImGui::BeginPopupModal(Title.c_str(), nullptr,
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

void PopupWindow::Activate(std::string_view title, std::string_view message) {
    Title = title;
    Message = message;
    Active = true;
}

void PopupWindow::SetCallback(std::function<void(PopupWindowState)> onUpdate) {
    m_UpdateCallback = onUpdate;
}

} // namespace Golde
