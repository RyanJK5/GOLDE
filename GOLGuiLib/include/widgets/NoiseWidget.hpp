#ifndef NoiseWidget_hpp_
#define NoiseWidget_hpp_

#include <cstdint>
#include <imgui.h>
#include <span>
#include <string>

#include "ActionButton.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace Golde {
class GenerateNoiseButton : public ActionButton<EditorAction, true> {
  public:
    GenerateNoiseButton(std::span<const ImGuiKeyChord> shortcuts = {})
        : ActionButton(EditorAction::GenerateNoise, shortcuts) {}

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult&) const override final;
    virtual bool Enabled(const EditorResult& state) const override final;
};

class NoiseWidget : public Widget {
  public:
    static constexpr int32_t BigStep = 10;

  public:
    NoiseWidget(std::span<const ImGuiKeyChord> shortcuts = {})
        : m_Button(shortcuts) {}

    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);

    void SetShortcutsImpl(const ShortcutMap& shortcuts);

  private:
    float m_Density = 0.5f;

    GenerateNoiseButton m_Button;
};
} // namespace Golde

#endif
