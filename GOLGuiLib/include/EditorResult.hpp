#ifndef EditorResult_h_
#define EditorResult_h_

#include <filesystem>

#include "GameEnums.hpp"

namespace gol {
struct SimulationStatus {
    SimulationState State = SimulationState::Paint;
};

struct EditingStatus {
    bool SelectionActive = false;
    bool UndosAvailable = false;
    bool RedosAvailable = false;
};

struct FileStatus {
    std::filesystem::path CurrentFilePath{};
    bool HasUnsavedChanges = false;
};

struct EditorResult {
    SimulationStatus Simulation{};
    EditingStatus Editing{};
    FileStatus File{};
    bool Active = true;
    bool Closing = false;
};
} // namespace gol

#endif