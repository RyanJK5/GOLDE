#ifndef VersionManager_hpp_
#define VersionManager_hpp_

#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"

namespace Golde {
struct VersionState {
    GameGrid Universe{};
    GameGrid SelectionUniverse{};
    std::optional<Rect> SelectionBounds{};
};

class VersionManager {
  public:
    void BeginPaintChange(const GameGrid& universe, SimulationState state);
    void AddPaintChange(const GameGrid& universe, SimulationState state);

    void PushChange(const VersionState& change);
    void TryPushChange(const std::optional<VersionState>& change,
                       SimulationState state);

    std::optional<VersionState> Undo();
    std::optional<VersionState> Redo();

    void Save() { m_LastSavedHeight = m_EditHeight; }
    bool IsSaved() const { return m_EditHeight == m_LastSavedHeight; }

    bool UndosAvailable() const { return m_UndoStack.size() > 1; }
    bool RedosAvailable() const { return !m_RedoStack.empty(); }

  private:
    void ClearRedos();

  private:
    size_t m_EditHeight = 0;
    size_t m_LastSavedHeight = 0;

    std::stack<VersionState> m_UndoStack;
    std::stack<VersionState> m_RedoStack;
};
} // namespace Golde

#endif
