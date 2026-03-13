#ifndef KeyShortcut_h_
#define KeyShortcut_h_

#include <imgui.h>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace gol {
class KeyShortcut {
  public:
    constexpr static auto MapChordsToVector =
        std::views::transform([](auto chord) { return KeyShortcut{chord}; }) |
        std::ranges::to<std::vector>();
    constexpr static auto RepeatableMapChordsToVector =
        std::views::transform(
            [](auto chord) { return KeyShortcut{chord, true, true}; }) |
        std::ranges::to<std::vector>();

    static std::string
    StringRepresentation(std::span<const KeyShortcut> shortcuts);

  public:
    KeyShortcut(ImGuiKeyChord shortcut, bool onRelease = true,
                bool allowRepeats = false);

    ImGuiKeyChord Shortcut() const { return m_Shortcut; }
    bool Active();

  private:
    ImGuiKeyChord m_Shortcut;
    bool m_OnRelease;
    bool m_AllowRepeats;
    bool m_Down;
};
} // namespace gol

#endif
