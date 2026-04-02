#ifndef GameEnums_hpp_
#define GameEnums_hpp_

#include <concepts>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace gol {
enum class SimulationState { None, Paint, Simulation, Paused, Stepping, Empty };

enum class GameAction { Start, Pause, Resume, Restart, Reset, Clear, Step };

enum class EditorAction {
    Resize,
    GenerateNoise,
    Undo,
    Redo,
    SaveAsNew,
    NewFile,
    Save,
    Load,
    Close,
};

enum class SelectionAction {
    Rotate,
    Select,
    Deselect,
    Delete,
    Copy,
    Cut,
    Paste,
    SelectAll,

    NudgeLeft,
    NudgeRight,
    NudgeUp,
    NudgeDown,

    FlipVertically,
    FlipHorizontally
};

using ActionVariant = std::variant<GameAction, EditorAction, SelectionAction>;

namespace Actions {
inline const std::unordered_map<std::string_view, GameAction>
    GameActionDefinitions = {
        {"start", GameAction::Start},   {"pause", GameAction::Pause},
        {"resume", GameAction::Resume}, {"restart", GameAction::Restart},
        {"reset", GameAction::Reset},   {"clear", GameAction::Clear},
        {"step", GameAction::Step}};
inline const std::unordered_map<std::string_view, EditorAction>
    EditorActionDefinitions = {{"resize", EditorAction::Resize},
                               {"undo", EditorAction::Undo},
                               {"redo", EditorAction::Redo},
                               {"save_as_new", EditorAction::SaveAsNew},
                               {"new", EditorAction::NewFile},
                               {"save", EditorAction::Save},
                               {"load", EditorAction::Load},
                               {"close", EditorAction::Close},
                               {"generate_noise", EditorAction::GenerateNoise}};
inline const std::unordered_map<std::string_view, SelectionAction>
    SelectionActionDefinitions = {
        {"rotate", SelectionAction::Rotate},
        {"deselect", SelectionAction::Deselect},
        {"delete", SelectionAction::Delete},
        {"copy", SelectionAction::Copy},
        {"cut", SelectionAction::Cut},
        {"paste", SelectionAction::Paste},
        {"nudge_left", SelectionAction::NudgeLeft},
        {"nudge_right", SelectionAction::NudgeRight},
        {"nudge_up", SelectionAction::NudgeUp},
        {"nudge_down", SelectionAction::NudgeDown},
        {"flip_horizontal", SelectionAction::FlipHorizontally},
        {"flip_vertical", SelectionAction::FlipVertically},
        {"select_all", SelectionAction::SelectAll}};

std::string ToString(ActionVariant action);

inline bool Editable(SimulationState state) {
    switch (state) {
        using enum SimulationState;
    case None:
    case Simulation:
    case Stepping:
        return false;
    case Paint:
    case Paused:
    case Empty:
        return true;
    default:
        return false;
    }
}
}; // namespace Actions

template <typename T>
concept ActionType =
    std::same_as<T, GameAction> || std::same_as<T, EditorAction> ||
    std::same_as<T, SelectionAction>;

enum class EditorMode { None, Insert, Delete, Select };
} // namespace gol

#endif
