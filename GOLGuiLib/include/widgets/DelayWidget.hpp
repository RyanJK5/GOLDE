#ifndef DelayWidget_hpp_
#define DelayWidget_hpp_

#include <cstdint>
#include <imgui.h>
#include <span>

#include "GameEnums.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace Golde {
class DelayWidget : public Widget {
  public:
    DelayWidget(std::span<const ImGuiKeyChord> = {}) {}
    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);
    void SetShortcutsImpl(const ShortcutMap&) {}

  public:
    int32_t TickDelayMs() const { return m_TickDelayMs; }
    bool ShowGridLines() const { return m_GridLines; }

  private:
    int32_t m_TickDelayMs = 1;
    bool m_GridLines = false;
};
} // namespace Golde

#endif
