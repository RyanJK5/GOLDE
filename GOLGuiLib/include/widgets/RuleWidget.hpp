#ifndef RuleWidget_hpp_
#define RuleWidget_hpp_

#include "Combo.hpp"
#include "ErrorWindow.hpp"
#include "LifeRule.hpp"
#include "Widget.hpp"

namespace gol {
class RuleWidget : public Widget {
  public:
    RuleWidget();

    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);
    void SetShortcutsImpl(const ShortcutMap&) {}

    struct RuleInfoChange {
        std::optional<Size2> NewSize;
        std::optional<TopologyKind> NewTopologyKind;
    };
    RuleInfoChange ResizeComponent(const EditorResult& state);

  private:
    std::string m_InputText = "B3/S23";
    std::string m_LastValid = "B3/S23";

    Combo m_TopologyCombo;
    ErrorWindow m_InputError;
};

} // namespace gol

#endif