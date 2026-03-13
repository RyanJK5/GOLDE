#ifndef ExecutionWidget_h_
#define ExecutionWidget_h_

#include <imgui.h>
#include <span>
#include <unordered_map>
#include <vector>

#include "ActionButton.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "KeyShortcut.hpp"
#include "Widget.hpp"
#include "WidgetResult.hpp"

namespace gol {
class StartButton : public MultiActionButton<GameAction, true> {
  public:
    StartButton(const std::unordered_map<ActionVariant,
                                         std::vector<ImGuiKeyChord>> &shortcuts)
        : MultiActionButton(
              {{GameAction::Start, shortcuts.at(GameAction::Start) |
                                       KeyShortcut::MapChordsToVector},
               {GameAction::Pause, shortcuts.at(GameAction::Pause) |
                                       KeyShortcut::MapChordsToVector},
               {GameAction::Resume, shortcuts.at(GameAction::Resume) |
                                        KeyShortcut::MapChordsToVector}}) {}

  protected:
    virtual Size2F Dimensions() const override final;
    virtual GameAction Action(const EditorResult &state) const override final;
    virtual std::string Label(const EditorResult &state) const override final;
    virtual bool Enabled(const EditorResult &state) const override final;
};

class ClearButton : public ActionButton<GameAction, false> {
  public:
    ClearButton(std::span<const ImGuiKeyChord> shortcuts = {})
        : ActionButton(GameAction::Clear, shortcuts) {}

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult &state) const override final;
    virtual bool Enabled(const EditorResult &state) const override final;
};

class ResetButton : public ActionButton<GameAction, false> {
  public:
    ResetButton(std::span<const ImGuiKeyChord> shortcuts = {})
        : ActionButton(GameAction::Reset, shortcuts) {}

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult &state) const override final;
    virtual bool Enabled(const EditorResult &state) const override final;
};

class RestartButton : public ActionButton<GameAction, false> {
  public:
    RestartButton(std::span<const ImGuiKeyChord> shortcuts = {})
        : ActionButton(GameAction::Restart, shortcuts) {}

  protected:
    virtual Size2F Dimensions() const override final;
    virtual std::string Label(const EditorResult &state) const override final;
    virtual bool Enabled(const EditorResult &state) const override final;
};

class ExecutionWidget : public Widget {
  public:
    ExecutionWidget(
        const ShortcutMap
            &shortcuts)
        : m_StartButton(shortcuts),
          m_ClearButton(shortcuts.at(GameAction::Clear)),
          m_ResetButton(shortcuts.at(GameAction::Reset)),
          m_RestartButton(shortcuts.at(GameAction::Restart)) {}
    friend Widget;

  private:
    WidgetResult UpdateImpl(const EditorResult &state);

    void SetShortcutsImpl(const ShortcutMap &shortcuts);
  private:
    StartButton m_StartButton;
    ClearButton m_ClearButton;
    ResetButton m_ResetButton;
    RestartButton m_RestartButton;
};
} // namespace gol

#endif
