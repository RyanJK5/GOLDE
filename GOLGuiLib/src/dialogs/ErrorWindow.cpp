#include <cstdint>
#include <imgui.h>

#include "ActionButton.hpp"
#include "ErrorWindow.hpp"
#include "GameEnums.hpp"
#include "PopupWindow.hpp"

namespace Golde {

std::optional<PopupWindowState> ErrorWindow::ShowButtons() const {
    const float height =
        ActionButton<EditorAction, false>::DefaultButtonHeight();
    const bool ok =
        ImGui::Button("Ok", {ImGui::GetContentRegionAvail().x, height});

    if (ok)
        return PopupWindowState::Success;
    return {};
}

} // namespace Golde
