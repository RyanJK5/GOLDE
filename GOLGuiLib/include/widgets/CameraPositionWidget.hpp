#ifndef CameraPositionWidget_hpp_
#define CameraPositionWidget_hpp_

#include "Widget.hpp"

namespace Golde {

class CameraPositionWidget : public Widget {
  public:
    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);
    void SetShortcutsImpl(const ShortcutMap&) {}

  private:
    Vec2 m_Position;
};

} // namespace Golde

#endif