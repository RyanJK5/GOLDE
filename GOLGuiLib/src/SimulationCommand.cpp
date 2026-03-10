#include "SimulationCommand.hpp"

namespace gol {

SimulationCommand ToCommand(GameAction action) {
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

SimulationCommand ToCommand(EditorAction action) {
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

SimulationCommand ToCommand(SelectionAction action) {
    return SelectionCommand{action};
}

} // namespace gol
