#include <cstdint>
#include <imgui.h>

#include "ActionButton.hpp"
#include "ErrorWindow.hpp"
#include "GameEnums.hpp"
#include "PopupWindow.hpp"

namespace gol {

std::optional<PopupWindowState> ErrorWindow::ShowButtons() const {
    constexpr static int32_t height =
        ActionButton<EditorAction, false>::DefaultButtonHeight;
    const bool ok =
        ImGui::Button("Ok", {ImGui::GetContentRegionAvail().x, height});

    if (ok)
        return PopupWindowState::Success;
    return {};
}

} // namespace gol
