#ifndef SelectionManager_hpp_
#define SelectionManager_hpp_

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
    std::optional<VersionState> Change{};
    bool BeginSelection = false;
};

class SelectionManager {
  public:
    SelectionUpdateResult UpdateSelectionArea(GameGrid& grid, Vec2 gridPos);

    bool TryResetSelection();

    std::optional<VersionState> Deselect(GameGrid& grid);

    std::optional<VersionState> SelectAll(GameGrid& grid);

    std::optional<VersionState> Copy(GameGrid& grid);

    std::optional<VersionState> Cut(const GameGrid& grid);

    std::expected<VersionState, FileEncoder::DecodeError>
    Paste(const GameGrid& grid, std::optional<Vec2> gridPos,
          uint32_t warnThreshold, bool unlock = false);

    std::optional<VersionState> Delete(const GameGrid& grid);

    std::optional<VersionState> Rotate(bool clockwise, const GameGrid& grid);

    std::optional<VersionState> Flip(SelectionAction direction,
                                     const GameGrid& grid);

    std::optional<VersionState> Nudge(Vec2 translation, const GameGrid& grid);

    std::optional<VersionState> InsertNoise(const GameGrid& grid,
                                            Rect selectionBounds,
                                            uint32_t warnThreshold,
                                            float density);

    std::pair<std::optional<VersionState>, std::optional<VersionState>>
    ModifySelectionBounds(GameGrid& grid, Rect bounds);

    std::expected<VersionState, FileEncoder::DecodeError>
    Load(const GameGrid& grid, const std::filesystem::path& filePath);

    bool Save(const GameGrid& grid,
              const std::filesystem::path& filePath) const;

    std::optional<VersionState> HandleAction(SelectionAction action,
                                             GameGrid& grid, int32_t nudgeSize);
    void HandleVersionChange(EditorAction undoRedo, GameGrid& grid,
                             const VersionState& state);

    Rect SelectionBounds() const;

    bool GridAlive() const;
    const HashQuadtree& GridData() const;
    const BigInt& SelectedPopulation() const;

    bool CanDrawSelection() const;
    bool CanDrawLargeSelection() const;
    bool CanDrawGrid() const;

  private:
    std::optional<VersionState> Select(GameGrid& grid);

    SelectionUpdateResult UpdateUnlockedSelection(GameGrid& grid, Vec2 gridPos);

    void RestoreGridVersion(EditorAction undoRedo, GameGrid& grid,
                            const VersionState& state);

    VersionState CaptureState(const GameGrid& grid) const;

    void SetSelectionBounds(Rect bounds);

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
