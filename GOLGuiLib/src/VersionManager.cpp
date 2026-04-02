#include <optional>
#include <utility>

#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "VersionManager.hpp"

namespace gol {
void VersionManager::BeginPaintChange(Vec2 pos, bool insert,
                                      SimulationState state) {
    if (state != SimulationState::Paint && state != SimulationState::Empty) {
        return;
    }

    if (insert)
        PushChange({.CellsInserted = {pos}});
    else
        PushChange({.CellsDeleted = {pos}});
}

void VersionManager::AddPaintChange(Vec2 pos, SimulationState state) {
    if (m_UndoStack.empty()) {
        return;
    }
    if (state != SimulationState::Paint && state != SimulationState::Empty) {
        return;
    }

    if (m_UndoStack.top().CellsInserted.size() > 0) {
        m_UndoStack.top().CellsInserted.insert(pos);
    } else {
        m_UndoStack.top().CellsDeleted.insert(pos);
    }
}

void VersionManager::PushChange(const VersionChange& change) {
    if (BreakingChange(change)) {
        m_EditHeight++;
    }
    m_UndoStack.push(change);
    ClearRedos();
}

void VersionManager::TryPushChange(const std::optional<VersionChange>& change,
                                   SimulationState state) {
    if (!change) {
        return;
    }
    if (state != SimulationState::Paint && state != SimulationState::Empty) {
        return;
    }
    PushChange(*change);
}

std::optional<VersionChange> VersionManager::Undo() {
    if (m_UndoStack.empty())
        return std::nullopt;

    VersionChange state = std::move(m_UndoStack.top());
    if (BreakingChange(state))
        m_EditHeight--;
    m_UndoStack.pop();
    m_RedoStack.push(state);

    return state;
}

std::optional<VersionChange> VersionManager::Redo() {
    if (m_RedoStack.empty())
        return std::nullopt;

    VersionChange state = std::move(m_RedoStack.top());
    if (BreakingChange(state))
        m_EditHeight++;

    m_RedoStack.pop();
    m_UndoStack.push(state);
    return state;
}

bool VersionManager::BreakingChange(const VersionChange& change) const {
    return !change.Action ||
           (change.Action != ActionVariant{SelectionAction::Select} &&
            change.Action != ActionVariant{SelectionAction::Deselect} &&
            change.Action != ActionVariant{SelectionAction::Copy});
}

void VersionManager::ClearRedos() {
    while (!m_RedoStack.empty())
        m_RedoStack.pop();
}
} // namespace gol
