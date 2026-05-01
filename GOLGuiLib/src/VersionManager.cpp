#include <optional>
#include <string_view>
#include <utility>

#include "GameEnums.hpp"
#include "Graphics2D.hpp"
#include "VersionManager.hpp"

namespace Golde {
void VersionManager::BeginPaintChange(const GameGrid& universe,
                                      SimulationState state) {
    if (state != SimulationState::Paint && state != SimulationState::Empty) {
        return;
    }

    PushChange({.Universe = universe});
}

void VersionManager::AddPaintChange(const GameGrid& universe,
                                    SimulationState state) {
    if (state != SimulationState::Paint && state != SimulationState::Empty) {
        return;
    }

    if (m_UndoStack.empty()) {
        PushChange({.Universe = universe});
        return;
    }

    m_UndoStack.top().Universe = universe;
}

void VersionManager::PushChange(const VersionState& change) {
    m_EditHeight++;
    m_UndoStack.push(change);
    ClearRedos();
}

void VersionManager::TryPushChange(const std::optional<VersionState>& change,
                                   SimulationState state) {
    if (!change) {
        return;
    }
    if (state != SimulationState::Paint && state != SimulationState::Empty) {
        return;
    }

    PushChange(*change);
}

std::optional<VersionState> VersionManager::Undo() {
    if (m_UndoStack.empty())
        return std::nullopt;

    VersionState current = std::move(m_UndoStack.top());
    m_EditHeight--;
    m_UndoStack.pop();
    m_RedoStack.push(std::move(current));

    if (m_UndoStack.empty()) {
        return std::nullopt;
    }

    return m_UndoStack.top();
}

std::optional<VersionState> VersionManager::Redo() {
    if (m_RedoStack.empty())
        return std::nullopt;

    VersionState state = std::move(m_RedoStack.top());
    m_EditHeight++;

    m_RedoStack.pop();
    m_UndoStack.push(state);
    return state;
}

void VersionManager::ClearRedos() {
    while (!m_RedoStack.empty())
        m_RedoStack.pop();
}
} // namespace Golde
