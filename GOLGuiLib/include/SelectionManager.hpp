#ifndef SelectionManager_h_
#define SelectionManager_h_

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <set>
#include <string>

#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "VersionManager.hpp"

namespace gol {
struct SelectionUpdateResult {
    std::optional<VersionChange> Change{};
    bool BeginSelection = false;
};

class SelectionManager {
  public:
    SelectionUpdateResult UpdateSelectionArea(GameGrid &grid, Vec2 gridPos);

    bool TryResetSelection();

    std::optional<VersionChange> Deselect(GameGrid &grid);

    std::optional<VersionChange> SelectAll(GameGrid &grid);

    std::optional<VersionChange> Copy(GameGrid &grid);

    std::optional<VersionChange> Cut();

    std::expected<VersionChange, std::optional<uint32_t>>
    Paste(std::optional<Vec2> gridPos, uint32_t warnThreshold,
          bool unlock = false);

    std::optional<VersionChange> Delete();

    std::optional<VersionChange> Rotate(bool clockwise);

    std::optional<VersionChange> Flip(SelectionAction direction);

    std::optional<VersionChange> Nudge(Vec2 translation);

    std::optional<VersionChange> InsertNoise(Rect selectionBounds,
                                             float density);

    std::expected<VersionChange, std::string>
    Load(const std::filesystem::path &filePath);

    bool Save(const GameGrid &grid,
              const std::filesystem::path &filePath) const;

    std::optional<VersionChange>
    HandleAction(SelectionAction action, GameGrid &grid, int32_t nudgeSize);
    void HandleVersionChange(EditorAction undoRedo, GameGrid &grid,
                             const VersionChange &change);

    Rect SelectionBounds() const;

    bool GridAlive() const;
    const LifeHashSet &GridData() const;
    int64_t SelectedPopulation() const;

    bool CanDrawSelection() const;
    bool CanDrawLargeSelection() const;
    bool CanDrawGrid() const;

  private:
    SelectionUpdateResult UpdateUnlockedSelection(Vec2 &gridPos);

    void RestoreGridVersion(EditorAction undoRedo, GameGrid &grid,
                            const VersionChange &change);

    void SetSelectionBounds(const Rect &bounds);

    Vec2 RotatePoint(Vec2F center, Vec2F point, bool clockwise);

  private:
    bool m_RotationParity = false;

    bool m_LockSelection = true;
    std::optional<Vec2> m_UnlockedOriginalPosition;

    std::optional<Vec2> m_AnchorSelection;
    std::optional<Vec2> m_SentinelSelection;
    std::optional<GameGrid> m_Selected;
};
} // namespace gol

#endif
