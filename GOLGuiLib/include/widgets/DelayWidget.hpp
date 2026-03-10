#ifndef DelayWidget_h_
#define DelayWidget_h_

#include <cstdint>
#include <imgui/imgui.h>
#include <span>

#include "GameEnums.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace gol {
class DelayWidget : public Widget {
  public:
    DelayWidget(std::span<const ImGuiKeyChord> = {}) {}
    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult &state);
    void SetShortcutsImpl(const ShortcutMap &) { }

  public:
    int32_t TickDelayMs() const { return m_TickDelayMs; }
    bool ShowGridLines() const { return m_GridLines; }

  private:
    int32_t m_TickDelayMs = 1;
    bool m_GridLines = false;
};
} // namespace gol

#endif