#include <imgui.h>
#include <imgui_internal.h>

#include "KeyShortcut.hpp"
#include <span>
#include <string>

namespace Golde {
KeyShortcut::KeyShortcut(ImGuiKeyChord shortcut, bool onRelease,
                         bool allowRepeats)
    : m_Shortcut(shortcut), m_OnRelease(onRelease),
      m_AllowRepeats(allowRepeats), m_Down(false) {}

std::string
KeyShortcut::StringRepresentation(std::span<const KeyShortcut> shortcuts) {
    auto tooltip = std::string{};
    if (!shortcuts.empty()) {
        for (size_t i = 0; i < shortcuts.size(); ++i) {
            tooltip += ImGui::GetKeyChordName(shortcuts[i].Shortcut());
            if (i < shortcuts.size() - 1)
                tooltip += ", ";
        }
    }
    return tooltip;
}

bool KeyShortcut::Active() {
    if (ImGui::GetIO().WantTextInput) {
        m_Down = false;
        return m_Down;
    }

    bool keyCombo = ImGui::IsKeyChordPressed(
        m_Shortcut,
        m_AllowRepeats ? ImGuiInputFlags_Repeat : ImGuiInputFlags_None);
    bool result = (!m_OnRelease && keyCombo) || (m_Down && !keyCombo);

    if (keyCombo)
        m_Down = true;
    else if (m_Down)
        m_Down = false;

    return result;
}
} // namespace Golde
