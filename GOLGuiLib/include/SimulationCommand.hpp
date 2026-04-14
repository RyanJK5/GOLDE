#ifndef SimulationCommand_hpp_
#define SimulationCommand_hpp_

#include <cstdint>
#include <filesystem>
#include <variant>

#include "GameEnums.hpp"
#include "Graphics2D.hpp"

namespace gol {

// Helper for std::visit with multiple lambdas
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

// Simulation lifecycle commands
struct StartCommand {};
struct PauseCommand {};
struct ResumeCommand {};
struct RestartCommand {};
struct ResetCommand {};
struct ClearCommand {};
struct StepCommand {};

// Editor commands
struct ResizeCommand {
    Size2 NewDimensions;
};
struct GenerateNoiseCommand {
    float Density = 0.f;
};
struct UndoCommand {};
struct RedoCommand {};

// File commands — each bundles its own path
struct SaveCommand {
    std::filesystem::path FilePath;
};
struct SaveAsNewCommand {
    std::filesystem::path FilePath;
};
struct NewFileCommand {
    std::filesystem::path FilePath;
};
struct LoadCommand {
    std::filesystem::path FilePath;
};
struct CloseCommand {};

struct SelectionBoundsCommand {
    Rect Bounds;
};

struct CameraPositionCommand {
    Vec2 Position;
};

struct CameraZoomCommand {
    float Zoom = 1.f;
};

// Selection command — wraps the existing SelectionAction enum
struct SelectionCommand {
    SelectionAction Action;
    int32_t NudgeSize = 1;
};

struct RuleCommand {
    std::string RuleString;
};

using SimulationCommand =
    std::variant<StartCommand, PauseCommand, ResumeCommand, RestartCommand,
                 ResetCommand, ClearCommand, StepCommand, ResizeCommand,
                 GenerateNoiseCommand, UndoCommand, RedoCommand, SaveCommand,
                 SaveAsNewCommand, NewFileCommand, LoadCommand, CloseCommand,
                 SelectionCommand, SelectionBoundsCommand,
                 CameraPositionCommand, CameraZoomCommand, RuleCommand>;

// Convert individual action enum values to SimulationCommand.
// Used by Widget::UpdateResult for simple (no-payload) buttons.
inline SimulationCommand ToCommand(GameAction action) {
    switch (action) {
        using enum GameAction;
    case Start:
        return StartCommand{};
    case Pause:
        return PauseCommand{};
    case Resume:
        return ResumeCommand{};
    case Restart:
        return RestartCommand{};
    case Reset:
        return ResetCommand{};
    case Clear:
        return ClearCommand{};
    case Step:
        return StepCommand{};
    }
    std::unreachable();
}

inline SimulationCommand ToCommand(EditorAction action) {
    switch (action) {
        using enum EditorAction;
    case Resize:
        return ResizeCommand{};
    case GenerateNoise:
        return GenerateNoiseCommand{};
    case Undo:
        return UndoCommand{};
    case Redo:
        return RedoCommand{};
    case Save:
        return SaveCommand{};
    case SaveAsNew:
        return SaveAsNewCommand{};
    case NewFile:
        return NewFileCommand{};
    case Load:
        return LoadCommand{};
    case Close:
        return CloseCommand{};
    }
    std::unreachable();
}

inline SimulationCommand ToCommand(SelectionAction action) {
    return SelectionCommand{action};
}

} // namespace gol

#endif
