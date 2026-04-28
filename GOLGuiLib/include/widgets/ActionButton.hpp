#ifndef GameActionButton_hpp_
#define GameActionButton_hpp_

#include <cstdint>
#include <imgui.h>
#include <imgui_internal.h>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "DisabledScope.hpp"
#include "EditorResult.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "KeyShortcut.hpp"

namespace gol {
template <ActionType ActType>
struct ActionButtonResult {
    std::optional<ActType> Action;
    bool FromShortcut = false;
};

using ShortcutMap =
    std::unordered_map<ActionVariant, std::vector<ImGuiKeyChord>>;

template <ActionType ActType, bool LineBreak>
class MultiActionButton {
  public:
    static float DefaultButtonHeight() { return ImGui::GetFontSize() * 4.f /3.f; }

    MultiActionButton(
        const std::unordered_map<ActType, std::vector<KeyShortcut>>& shortcuts)
        : m_Shortcuts(shortcuts) {}

    ActionButtonResult<ActType> Update(const EditorResult& state) {
        const bool enabled = Enabled(state);
        auto result = [&] {
            DisabledScope disableIf{!enabled};

            if (!m_LineBreak)
                ImGui::SameLine();

            return [this, state] {
                bool active = false;
                if (Enabled(state))
                    for (auto& shortcut : m_Shortcuts.at(Action(state))) {
                        active = shortcut.Active() || active;
                    }

                if (ImGui::Button(Label(state).c_str(), Dimensions()) || active)
                    return ActionButtonResult<ActType>{Action(state), active};
                return ActionButtonResult<ActType>{};
            }();
        }();

        if (enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            auto tooltip = Actions::ToString(Action(state));
            const auto& shortcuts = m_Shortcuts.at(Action(state));
            if (!shortcuts.empty())
                tooltip += ": " + KeyShortcut::StringRepresentation(
                                      m_Shortcuts.at(Action(state)));
            ImGui::SetTooltip("%s", tooltip.c_str());
        }

        return result;
    }

    void
    SetShortcuts(const std::unordered_map<ActType, std::vector<KeyShortcut>>&
                     shortcuts) {
        m_Shortcuts = shortcuts;
    }

  protected:
    virtual ActType Action(const EditorResult& state) const = 0;

    virtual Size2F Dimensions() const = 0;
    virtual std::string Label(const EditorResult& state) const = 0;
    virtual bool Enabled(const EditorResult& state) const = 0;

  private:
    bool m_LineBreak = LineBreak;

    std::unordered_map<ActType, std::vector<KeyShortcut>> m_Shortcuts;
};

template <ActionType ActType, bool LineBreak>
class ActionButton : public MultiActionButton<ActType, LineBreak> {
  public:
    ActionButton(ActType action, std::span<const ImGuiKeyChord> shortcuts,
                 bool allowRepeats = false)
        : MultiActionButton<ActType, LineBreak>(
              {{action,
                allowRepeats
                    ? shortcuts | KeyShortcut::RepeatableMapChordsToVector
                    : shortcuts | KeyShortcut::MapChordsToVector}}),
          m_Action(action), m_AllowRepeats(allowRepeats) {}

    void SetShortcuts(std::span<const ImGuiKeyChord> shortcuts) {
        MultiActionButton<ActType, LineBreak>::SetShortcuts(
            std::unordered_map<ActType, std::vector<KeyShortcut>>{
                {m_Action,
                 m_AllowRepeats
                     ? shortcuts | KeyShortcut::RepeatableMapChordsToVector
                     : shortcuts | KeyShortcut::MapChordsToVector}});
    }

  protected:
    virtual ActType Action(const EditorResult&) const override final {
        return m_Action;
    }

  private:
    ActType m_Action;
    bool m_AllowRepeats;
};
} // namespace gol

#endif
