#include <expected>
#include <filesystem>
#include <font-awesome/IconsFontAwesome7.h>
#include <imgui/imgui.h>
#include <span>
#include <string>

#include "ActionButton.hpp"
#include "FileDialog.hpp"
#include "FileWidget.hpp"
#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "PopupWindow.hpp"
#include "SimulationCommand.hpp"
#include "WidgetResult.hpp"

namespace gol {
NewFileButton::NewFileButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(EditorAction::NewFile, shortcuts) {}
Size2F NewFileButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 4.f,
            ActionButton::DefaultButtonHeight};
}
std::string NewFileButton::Label(const EditorResult&) const {
    return ICON_FA_FILE_CIRCLE_PLUS;
}
bool NewFileButton::Enabled(const EditorResult&) const { return true; }

UpdateFileButton::UpdateFileButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(EditorAction::Save, shortcuts) {}
Size2F UpdateFileButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 3.f,
            ActionButton::DefaultButtonHeight};
}
std::string UpdateFileButton::Label(const EditorResult&) const {
    return ICON_FA_FILE_ARROW_UP;
}
bool UpdateFileButton::Enabled(const EditorResult& state) const {
    return (state.File.CurrentFilePath.empty() &&
            state.Simulation.State == SimulationState::Paused) ||
           (state.File.CurrentFilePath.empty() &&
            state.Simulation.State == SimulationState::Empty) ||
           state.Simulation.State == SimulationState::Paint;
}

SaveButton::SaveButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(EditorAction::SaveAsNew, shortcuts) {}

Size2F SaveButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x / 2.f,
            ActionButton::DefaultButtonHeight};
}

std::string SaveButton::Label(const EditorResult&) const {
    return ICON_FA_FLOPPY_DISK;
}

bool SaveButton::Enabled(const EditorResult& state) const {
    return state.Simulation.State == SimulationState::Paint ||
           state.Simulation.State == SimulationState::Paused;
}

LoadButton::LoadButton(std::span<const ImGuiKeyChord> shortcuts)
    : ActionButton(EditorAction::Load, shortcuts) {}
Size2F LoadButton::Dimensions() const {
    return {ImGui::GetContentRegionAvail().x,
            ActionButton::DefaultButtonHeight};
}

std::string LoadButton::Label(const EditorResult&) const {
    return ICON_FA_FOLDER_OPEN;
}

bool LoadButton::Enabled(const EditorResult&) const { return true; }

FileWidget::FileWidget(const ShortcutMap& shortcutInfo)
    : m_NewFileButton(shortcutInfo.at(EditorAction::NewFile)),
      m_UpdateFileButton(shortcutInfo.at(EditorAction::Save)),
      m_SaveButton(shortcutInfo.at(EditorAction::SaveAsNew)),
      m_LoadButton(shortcutInfo.at(EditorAction::Load)),
      m_FileNotOpened("File Not Opened", [](auto) {}) {}

WidgetResult FileWidget::UpdateImpl(const EditorResult& state) {
    m_FileNotOpened.Update();

    std::optional<EditorAction> action;
    bool fromShortcut = false;

    auto tryButton = [&](auto& button) {
        if (action)
            return;
        auto btnResult = button.Update(state);
        if (btnResult.Action) {
            action = *btnResult.Action;
            fromShortcut = btnResult.FromShortcut;
        }
    };

    tryButton(m_NewFileButton);
    tryButton(m_UpdateFileButton);
    tryButton(m_SaveButton);

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 30.f);
    tryButton(m_LoadButton);
    ImGui::Separator();
    ImGui::PopStyleVar();

    if (!action)
        return {};

    auto filePath =
        [&action,
         &state] -> std::expected<std::filesystem::path, FileDialogFailure> {
        switch (*action) {
            using enum EditorAction;
        case NewFile:
            return std::filesystem::path{};
        case Save:
            if (!state.File.CurrentFilePath.empty())
                return state.File.CurrentFilePath;
            return FileDialog::SaveFileDialog("gol", "");
        case SaveAsNew:
            return FileDialog::SaveFileDialog(
                "gol", state.File.CurrentFilePath.string());
        case Load:
            return FileDialog::OpenFileDialog("gol", "");
        default:
            return std::unexpected{
                FileDialogFailure{FileFailureType::Error, "Unknown action"}};
        }
    }();

    if (!filePath) {
        if (filePath.error().Type == FileFailureType::Error) {
            m_FileNotOpened.Activate();
            m_FileNotOpened.Message = filePath.error().Message;
        }
        return {};
    }

    auto command = [&]() -> SimulationCommand {
        switch (*action) {
            using enum EditorAction;
        case NewFile:
            return NewFileCommand{*filePath};
        case Save:
            return SaveCommand{*filePath};
        case SaveAsNew:
            return SaveAsNewCommand{*filePath};
        case Load:
            return LoadCommand{*filePath};
        default:
            std::unreachable();
        }
    }();

    return {.Command = command, .FromShortcut = fromShortcut};
}

void FileWidget::SetShortcutsImpl(const ShortcutMap& shortcutInfo) {
    m_NewFileButton.SetShortcuts(shortcutInfo.at(EditorAction::NewFile));
    m_UpdateFileButton.SetShortcuts(shortcutInfo.at(EditorAction::Save));
    m_SaveButton.SetShortcuts(shortcutInfo.at(EditorAction::SaveAsNew));
    m_LoadButton.SetShortcuts(shortcutInfo.at(EditorAction::Load));
}
} // namespace gol