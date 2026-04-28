#include <cstdint>
#include <imgui.h>
#include <string>

#include "ActionButton.hpp"
#include "GameEnums.hpp"
#include "PopupWindow.hpp"
#include "WarnWindow.hpp"

namespace gol {

std::optional<PopupWindowState> WarnWindow::ShowButtons() const {
    const float height =
        ActionButton<EditorAction, false>::DefaultButtonHeight();
    bool yes = ImGui::Button("Yes", {ImGui::GetContentRegionAvail().x, height});
    bool no = ImGui::Button("No", {ImGui::GetContentRegionAvail().x, height});

    if (yes)
        return PopupWindowState::Success;
    if (no)
        return PopupWindowState::Failure;
    return {};
}

} // namespace gol
