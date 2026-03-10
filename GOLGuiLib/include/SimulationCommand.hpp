#ifndef SimulationCommand_h_
#define SimulationCommand_h_

#include <cstdint>
#include <filesystem>
#include <variant>

#include "GameEnums.hpp"
#include "Graphics2D.hpp"

namespace gol {

// Helper for std::visit with multiple lambdas
template <class... Ts> struct overloaded : Ts... {
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

// Selection command — wraps the existing SelectionAction enum
struct SelectionCommand {
    SelectionAction Action;
    int32_t NudgeSize = 1;
};

using SimulationCommand =
    std::variant<StartCommand, PauseCommand, ResumeCommand, RestartCommand,
                 ResetCommand, ClearCommand, StepCommand, ResizeCommand,
                 GenerateNoiseCommand, UndoCommand, RedoCommand, SaveCommand,
                 SaveAsNewCommand, NewFileCommand, LoadCommand, CloseCommand,
                 SelectionCommand>;

// Convert individual action enum values to SimulationCommand.
// Used by Widget::UpdateResult for simple (no-payload) buttons.
SimulationCommand ToCommand(GameAction action);
SimulationCommand ToCommand(EditorAction action);
SimulationCommand ToCommand(SelectionAction action);

} // namespace gol

#endif
