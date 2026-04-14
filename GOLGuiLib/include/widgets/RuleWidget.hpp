#ifndef RuleWidget_hpp_
#define RuleWidget_hpp_

#include "ErrorWindow.hpp"
#include "Widget.hpp"

namespace gol {
class RuleWidget : public Widget {
  public:
    RuleWidget();

    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);
    void SetShortcutsImpl(const ShortcutMap&) {}

    std::optional<Size2> ResizeComponent(const EditorResult& state);

  private:
    std::string m_InputText = "B3/S23";
    std::string m_LastValid;

    ErrorWindow m_InputError;
};

} // namespace gol

#endif