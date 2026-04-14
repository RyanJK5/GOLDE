#ifndef EditorResult_hpp_
#define EditorResult_hpp_

#include <filesystem>
#include <optional>

#include "GameEnums.hpp"
#include "Graphics2D.hpp"

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
    std::optional<Rect> SelectionBounds{};
    float Zoom = 1.f;
    bool Active = true;
    bool Closing = false;
};
} // namespace gol

#endif
