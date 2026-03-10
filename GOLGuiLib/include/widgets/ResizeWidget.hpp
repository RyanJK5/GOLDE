#ifndef ResizeWidget_h_
#define ResizeWidget_h_

#include <imgui/imgui.h>
#include <span>

#include "ActionButton.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace gol {
class ResizeButton : public ActionButton<EditorAction, false> {
  public:
    ResizeButton(std::span<const ImGuiKeyChord> shortcuts = {})
        : ActionButton(EditorAction::Resize, shortcuts) {}

  protected:
    virtual Size2F Dimensions() const final {
        return {ImGui::GetContentRegionAvail().x,
                ActionButton::DefaultButtonHeight};
    }
    virtual std::string Label(const EditorResult &) const override final {
        return "Apply";
    }
    virtual bool Enabled(const EditorResult &state) const final {
        return state.Simulation.State == SimulationState::Paint ||
               state.Simulation.State == SimulationState::Empty;
    }
};

class ResizeWidget : public Widget {
  public:
    ResizeWidget(std::span<const ImGuiKeyChord> shortcuts = {})
        : m_Button(shortcuts) {}
    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult &state);
    void SetShortcutsImpl(const ShortcutMap &shortcuts);
  private:
    ResizeButton m_Button;
    Size2 m_Dimensions;
};
} // namespace gol

#endif