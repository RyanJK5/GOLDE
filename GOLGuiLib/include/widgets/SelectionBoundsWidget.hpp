#ifndef SelectionBoundsWidget_hpp_
#define SelectionBoundsWidget_hpp_

#include <imgui.h>
#include <span>

#include "Widget.hpp"

namespace Golde {
class SelectionBoundsWidget : public Widget {
  public:
    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);
    void SetShortcutsImpl(const ShortcutMap&) {}
};
} // namespace Golde

#endif