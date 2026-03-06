#include <chrono>
#include <imgui/imgui.h>
#include <thread>
#include <vector>

#include "ConfigLoader.hpp"
#include "GameEnums.hpp"
#include "KeyShortcut.hpp"
#include "SimulationControl.hpp"
#include "SimulationControlResult.hpp"
using namespace std::chrono_literals;

namespace gol {
ButtonlessShortcuts::ButtonlessShortcuts(
    const std::vector<ImGuiKeyChord> &left,
    const std::vector<ImGuiKeyChord> &right,
    const std::vector<ImGuiKeyChord> &up,
    const std::vector<ImGuiKeyChord> &down,
    const std::vector<ImGuiKeyChord> &close)
    : Shortcuts(
          {{SelectionAction::NudgeLeft,
            left | KeyShortcut::RepeatableMapChordsToVector},
           {SelectionAction::NudgeRight,
            right | KeyShortcut::RepeatableMapChordsToVector},
           {SelectionAction::NudgeUp,
            up | KeyShortcut::RepeatableMapChordsToVector},
           {SelectionAction::NudgeDown,
            down | KeyShortcut::RepeatableMapChordsToVector},
           {EditorAction::Close, close | KeyShortcut::MapChordsToVector}}) {}

SimulationControlResult
ButtonlessShortcuts::UpdateImpl(const EditorResult &state) {
    SimulationControlResult result{.NudgeSize = 1};
    for (auto &&[action, actionShortcuts] : Shortcuts) {
        auto *selectionAction = std::get_if<SelectionAction>(&action);
        if (selectionAction && *selectionAction != SelectionAction::Deselect &&
            state.State != SimulationState::Paint)
            continue;

        bool resultActive = false;
        for (auto &shortcut : actionShortcuts) {
            bool shortcutActive = shortcut.Active();
            if (shortcutActive && ((shortcut.Shortcut() & ImGuiMod_Shift) != 0))
                result.NudgeSize = 10;
            resultActive = shortcutActive || resultActive;
        }
        if (resultActive && !result.Action)
            result.Action = action;
    }
    if (result.Action)
        result.FromShortcut = true;
    return result;
}

void ButtonlessShortcuts::SetShortcuts(const ShortcutMap &shortcuts) {
    Shortcuts = {{{SelectionAction::NudgeLeft,
                   shortcuts.at(SelectionAction::NudgeLeft) |
                       KeyShortcut::RepeatableMapChordsToVector},
                  {SelectionAction::NudgeRight,
                   shortcuts.at(SelectionAction::NudgeRight) |
                       KeyShortcut::RepeatableMapChordsToVector},
                  {SelectionAction::NudgeUp,
                   shortcuts.at(SelectionAction::NudgeUp) |
                       KeyShortcut::RepeatableMapChordsToVector},
                  {SelectionAction::NudgeDown,
                   shortcuts.at(SelectionAction::NudgeDown) |
                       KeyShortcut::RepeatableMapChordsToVector},
                  {EditorAction::Close, shortcuts.at(EditorAction::Close) |
                                            KeyShortcut::MapChordsToVector}}};
}

SimulationControl::SimulationControl(
    const ConfigLoader::StyleInfo<ImVec4> &fileInfo)

    : m_ButtonlessShortcuts(fileInfo.Shortcuts.at(SelectionAction::NudgeLeft),
                            fileInfo.Shortcuts.at(SelectionAction::NudgeRight),
                            fileInfo.Shortcuts.at(SelectionAction::NudgeUp),
                            fileInfo.Shortcuts.at(SelectionAction::NudgeDown),
                            fileInfo.Shortcuts.at(EditorAction::Close)),
      m_ExecutionWidget(fileInfo.Shortcuts), m_EditorWidget(fileInfo.Shortcuts),
      m_FileWidget(fileInfo.Shortcuts),
      m_ResizeWidget(fileInfo.Shortcuts.at(EditorAction::Resize)),
      m_StepWidget(fileInfo.Shortcuts.at(GameAction::Step)),
      m_NoiseWidget(fileInfo.Shortcuts.at(EditorAction::GenerateNoise)),
      m_ShortcutConfigPath(fileInfo.OriginPath),
      m_ShortcutChecker(
          std::bind_front(&SimulationControl::TryUpdateShortcuts, this)) {}

void SimulationControl::FillResults(
    SimulationControlResult &current,
    const SimulationControlResult &update) const {
    if (!current.Algorithm)
        current.Algorithm = update.Algorithm;
    if (!current.Action)
        current.Action = update.Action;
    if (!current.NewDimensions)
        current.NewDimensions = update.NewDimensions;
    if (!current.StepCount)
        current.StepCount = update.StepCount;
    if (!current.TickDelayMs)
        current.TickDelayMs = update.TickDelayMs;
    if (current.NudgeSize == 0)
        current.NudgeSize = update.NudgeSize;
    if (!current.FilePath)
        current.FilePath = update.FilePath;
    if (!current.GridLines)
        current.GridLines = update.GridLines;
    if (!current.FromShortcut)
        current.FromShortcut = update.FromShortcut;
    if (!current.NoiseDensity)
        current.NoiseDensity = update.NoiseDensity;
}

void SimulationControl::TryUpdateShortcuts(std::stop_token stopToken) {
    auto lastKnownWriteTime =
        std::filesystem::last_write_time(m_ShortcutConfigPath);
    while (!stopToken.stop_requested()) {
        if (const auto lastWriteTime =
                std::filesystem::last_write_time(m_ShortcutConfigPath);
            lastWriteTime != lastKnownWriteTime) {
            lastKnownWriteTime = lastWriteTime;

            m_UpdateShortcuts.store(true, std::memory_order_relaxed);
        }
        std::this_thread::sleep_for(5ms);
    }
}

SimulationControlResult SimulationControl::Update(const EditorResult &state) {
    ImGui::Begin("Simulation Control", nullptr, ImGuiWindowFlags_NoNav);

    SimulationControlResult result{.State = state.State};

    if (m_UpdateShortcuts.exchange(false, std::memory_order_relaxed)) {
        const auto fileInfo =
            ConfigLoader::LoadYAML<ImVec4>(m_ShortcutConfigPath);

        ForEachWidget([&fileInfo](auto &&widget) {
            widget.SetShortcuts(fileInfo.Shortcuts);
        });
    }

    ForEachWidget([&, this](auto &&widget) {
        FillResults(result, widget.Update(state));
    });

    ImGui::End();
    return result;
}
} // namespace gol