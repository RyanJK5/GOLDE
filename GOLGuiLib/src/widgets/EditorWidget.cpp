#include <font-awesome/IconsFontAwesome7.h>
#include <imgui.h>
#include <optional>

#include "EditorWidget.hpp"
#include "GameEnums.hpp"
#include "WidgetResult.hpp"

namespace gol {

CopyButton::CopyButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::Copy, shortcuts) {}

Size2F CopyButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 4.f,
            ActionButton::DefaultButtonHeight};
}

std::string CopyButton::Label(const EditorResult&) const {
    return ICON_FA_COPY;
}

bool CopyButton::Enabled(const EditorResult& state) const {
    return state.Editing.SelectionActive;
}

CutButton::CutButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::Cut, shortcuts) {}

Size2F CutButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 2.f,
            ActionButton::DefaultButtonHeight};
}

std::string CutButton::Label(const EditorResult&) const {
    return ICON_FA_SCISSORS;
}

bool CutButton::Enabled(const EditorResult& state) const {
    return state.Editing.SelectionActive &&
           Actions::Editable(state.Simulation.State);
}

PasteButton::PasteButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::Paste, shortcuts) {}

Size2F PasteButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 3.f,
            ActionButton::DefaultButtonHeight};
}

std::string PasteButton::Label(const EditorResult&) const {
    return ICON_FA_PASTE;
}

bool PasteButton::Enabled(const EditorResult& state) const {
    return ImGui::GetClipboardText() &&
           Actions::Editable(state.Simulation.State);
}

DeleteButton::DeleteButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::Delete, shortcuts) {}

Size2F DeleteButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x,
            ActionButton::DefaultButtonHeight};
}

std::string DeleteButton::Label(const EditorResult&) const {
    return ICON_FA_DELETE_LEFT;
}

bool DeleteButton::Enabled(const EditorResult& state) const {
    return state.Editing.SelectionActive &&
           Actions::Editable(state.Simulation.State);
}

DeselectButton::DeselectButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::Deselect, shortcuts) {}

Size2F DeselectButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 4.f,
            ActionButton::DefaultButtonHeight};
}

std::string DeselectButton::Label(const EditorResult&) const {
    return ICON_FA_OBJECT_UNGROUP;
}

bool DeselectButton::Enabled(const EditorResult& state) const {
    return state.Editing.SelectionActive;
}

RotateButton::RotateButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::Rotate, shortcuts) {}

Size2F RotateButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 3.f,
            ActionButton::DefaultButtonHeight};
}

std::string RotateButton::Label(const EditorResult&) const {
    return ICON_FA_ROTATE;
}

bool RotateButton::Enabled(const EditorResult& state) const {
    return state.Editing.SelectionActive &&
           Actions::Editable(state.Simulation.State);
}

FlipVerticalButton::FlipVerticalButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::FlipVertically, shortcuts) {}

Size2F FlipVerticalButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 2.f,
            ActionButton::DefaultButtonHeight};
}

std::string FlipVerticalButton::Label(const EditorResult&) const {
    return ICON_FA_ARROWS_UP_DOWN;
}

bool FlipVerticalButton::Enabled(const EditorResult& state) const {
    return state.Editing.SelectionActive &&
           Actions::Editable(state.Simulation.State);
}

FlipHorizontalButton::FlipHorizontalButton(
    std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::FlipHorizontally, shortcuts) {}

Size2F FlipHorizontalButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x,
            ActionButton::DefaultButtonHeight};
}

std::string FlipHorizontalButton::Label(const EditorResult&) const {
    return ICON_FA_ARROWS_LEFT_RIGHT;
}

bool FlipHorizontalButton::Enabled(const EditorResult& state) const {
    return state.Editing.SelectionActive &&
           Actions::Editable(state.Simulation.State);
}

SelectAllButton::SelectAllButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(SelectionAction::SelectAll, shortcuts) {}

Size2F SelectAllButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 4.f,
            ActionButton::DefaultButtonHeight};
}

std::string SelectAllButton::Label(const EditorResult&) const {
    return ICON_FA_SQUARE_CHECK;
}

bool SelectAllButton::Enabled(const EditorResult& state) const {
    return Actions::Editable(state.Simulation.State) &&
           !state.Simulation.OutOfBounds;
}

UndoButton::UndoButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(EditorAction::Undo, shortcuts, true) {}

Size2F UndoButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 3.f,
            ActionButton::DefaultButtonHeight};
}

std::string UndoButton::Label(const EditorResult&) const {
    return ICON_FA_ARROW_ROTATE_LEFT;
}

bool UndoButton::Enabled(const EditorResult& state) const {
    return state.Editing.UndosAvailable &&
           (state.Simulation.State == SimulationState::Paint ||
            state.Simulation.State == SimulationState::Empty);
}

RedoButton::RedoButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(EditorAction::Redo, shortcuts, true) {}

Size2F RedoButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 2.f,
            ActionButton::DefaultButtonHeight};
}

std::string RedoButton::Label(const EditorResult&) const {
    return ICON_FA_ARROW_ROTATE_RIGHT;
}

bool RedoButton::Enabled(const EditorResult& state) const {
    return state.Editing.RedosAvailable &&
           (state.Simulation.State == SimulationState::Paint ||
            state.Simulation.State == SimulationState::Empty);
}

EditorWidget::EditorWidget(const ShortcutMap& shortcuts)
    : m_CopyButton(shortcuts.at(SelectionAction::Copy)),
      m_CutButton(shortcuts.at(SelectionAction::Cut)),
      m_PasteButton(shortcuts.at(SelectionAction::Paste)),
      m_DeleteButton(shortcuts.at(SelectionAction::Delete)),
      m_DeselectButton(shortcuts.at(SelectionAction::Deselect)),
      m_RotateButton(shortcuts.at(SelectionAction::Rotate)),
      m_FlipHorizontalButton(shortcuts.at(SelectionAction::FlipHorizontally)),
      m_FlipVerticalButton(shortcuts.at(SelectionAction::FlipVertically)),
      m_SelectAllButton(shortcuts.at(SelectionAction::SelectAll)),
      m_UndoButton(shortcuts.at(EditorAction::Undo)),
      m_RedoButton(shortcuts.at(EditorAction::Redo)) {}

WidgetResult EditorWidget::UpdateImpl(const EditorResult& state) {
    auto result = WidgetResult{};

    UpdateResult(result, m_CopyButton.Update(state));
    UpdateResult(result, m_PasteButton.Update(state));
    UpdateResult(result, m_CutButton.Update(state));
    UpdateResult(result, m_DeleteButton.Update(state));

    UpdateResult(result, m_DeselectButton.Update(state));
    UpdateResult(result, m_RotateButton.Update(state));
    UpdateResult(result, m_FlipVerticalButton.Update(state));
    UpdateResult(result, m_FlipHorizontalButton.Update(state));
    UpdateResult(result, m_SelectAllButton.Update(state));

    UpdateResult(result, m_UndoButton.Update(state));

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);
    UpdateResult(result, m_RedoButton.Update(state));
    ImGui::PopStyleVar();

    if (result.Command) {
        if (auto* cmd = std::get_if<SelectionCommand>(&*result.Command)) {
            cmd->ClipboardText = ImGui::GetClipboardText();
        }
    }

    return result;
}

void EditorWidget::SetShortcutsImpl(const ShortcutMap& shortcuts) {
    m_CopyButton.SetShortcuts(shortcuts.at(SelectionAction::Copy));
    m_CutButton.SetShortcuts(shortcuts.at(SelectionAction::Cut));
    m_PasteButton.SetShortcuts(shortcuts.at(SelectionAction::Paste));
    m_DeleteButton.SetShortcuts(shortcuts.at(SelectionAction::Delete));
    m_DeselectButton.SetShortcuts(shortcuts.at(SelectionAction::Deselect));
    m_RotateButton.SetShortcuts(shortcuts.at(SelectionAction::Rotate));
    m_FlipHorizontalButton.SetShortcuts(
        shortcuts.at(SelectionAction::FlipHorizontally));
    m_FlipVerticalButton.SetShortcuts(
        shortcuts.at(SelectionAction::FlipVertically));
    m_SelectAllButton.SetShortcuts(shortcuts.at(SelectionAction::SelectAll));
    m_UndoButton.SetShortcuts(shortcuts.at(EditorAction::Undo));
    m_RedoButton.SetShortcuts(shortcuts.at(EditorAction::Redo));
}

} // namespace gol
