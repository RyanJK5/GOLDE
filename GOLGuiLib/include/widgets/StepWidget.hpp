#ifndef StepWidget_hpp_
#define StepWidget_hpp_

#include <cstdint>
#include <imgui.h>
#include <span>
#include <string>

#include "ActionButton.hpp"
#include "BigInt.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "LifeAlgorithm.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace gol {
class StepButton : public ActionButton<GameAction, false> {
  public:
    StepButton(std::span<const ImGuiKeyChord> shortcuts = {})
        : ActionButton(GameAction::Step, shortcuts, true) {}

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult&) const override final;
    virtual bool Enabled(const EditorResult& state) const override final;
};

class StepWidget : public Widget {
  public:
    static constexpr int32_t BigStep = 10;

  public:
    StepWidget(std::span<const ImGuiKeyChord> shortcuts = {})
        : m_InputText("1"), m_Button(shortcuts) {}

    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult& state);

    void SetShortcutsImpl(const ShortcutMap& shortcuts);

    void SetStepCount(const BigInt& stepCount);

    void ShowInputText();

  public:
    BigInt EffectiveStepCount() const {
        return m_HyperSpeed ? BigZero : m_StepCount;
    }
    std::string_view CurrentAlgorithmIdentifier() const {
        return m_AlgorithmIdentifier;
    }
    bool IsHyperSpeed() const { return m_HyperSpeed; }

  private:
    std::string m_InputText;

    std::string_view m_AlgorithmIdentifier = "HashLife";
    BigInt m_StepCount = 1;
    bool m_HyperSpeed = false;

    StepButton m_Button;
};
} // namespace gol

#endif
