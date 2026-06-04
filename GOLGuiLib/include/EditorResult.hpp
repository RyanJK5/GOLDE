#ifndef EditorResult_hpp_
#define EditorResult_hpp_

#include <filesystem>
#include <optional>
#include <string>

#include "GameEnums.hpp"
#include "Graphics2D.hpp"

namespace Golde {
struct SimulationStatus {
    SimulationState State = SimulationState::Paint;
    bool OutOfBounds = false;
    std::string_view RuleString = "B3/S23";
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
} // namespace Golde

#endif
