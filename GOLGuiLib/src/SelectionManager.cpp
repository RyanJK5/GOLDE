#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <imgui.h>
#include <optional>
#include <set>
#include <utility>
#include <variant>

#include "GameEnums.hpp"
#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "RLEEncoder.hpp"
#include "SelectionManager.hpp"
#include "VersionManager.hpp"

namespace gol {
SelectionUpdateResult SelectionManager::UpdateSelectionArea(GameGrid& grid,
                                                            Vec2 gridPos) {
    const bool shiftDown = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                           ImGui::IsKeyDown(ImGuiKey_RightShift);
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && shiftDown) {
        m_SentinelSelection = gridPos;
        if (!m_AnchorSelection)
            m_AnchorSelection = gridPos;
        return {.BeginSelection = true};
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        (m_AnchorSelection != m_SentinelSelection)) {
        if (m_Selected && m_LockSelection)
            return {.BeginSelection = false};
        if (m_Selected && !m_LockSelection)
            return UpdateUnlockedSelection(gridPos);

        auto change = Select(grid);
        return {.Change = std::move(change), .BeginSelection = false};
    }

    auto deselectChange = Deselect(grid);
    m_AnchorSelection = gridPos;
    m_SentinelSelection = gridPos;
    return {.Change = deselectChange, .BeginSelection = false};
}

SelectionUpdateResult SelectionManager::UpdateUnlockedSelection(Vec2 gridPos) {
    if (gridPos == *m_SentinelSelection && m_UnlockedOriginalPosition) {
        auto result = SelectionUpdateResult{
            .Change = VersionChange{.Action = SelectionAction::NudgeDown,
                                    .SelectionBounds = SelectionBounds(),
                                    .NudgeTranslation =
                                        gridPos - *m_UnlockedOriginalPosition},
            .BeginSelection = false};
        m_UnlockedOriginalPosition = std::nullopt;
        return result;
    } else if ((std::abs(ImGui::GetIO().MouseDelta.x) <= 1 &&
                std::abs(ImGui::GetIO().MouseDelta.y) <= 1))
        return {.BeginSelection = false};

    if (!m_UnlockedOriginalPosition)
        m_UnlockedOriginalPosition = m_SentinelSelection;
    auto sentinelOffset = *m_SentinelSelection - *m_AnchorSelection;
    m_SentinelSelection = gridPos;
    m_AnchorSelection = gridPos - sentinelOffset;

    return {.BeginSelection = false};
}

bool SelectionManager::TryResetSelection() {
    if (CanDrawGrid())
        return false;
    m_AnchorSelection = std::nullopt;
    m_SentinelSelection = std::nullopt;
    m_Selected = std::nullopt;
    return true;
}

VersionChange SelectionManager::Select(GameGrid& grid) {
    m_Selected = grid.SubRegion(SelectionBounds());
    auto change =
        VersionChange{.Action = SelectionAction::Select,
                      .SelectionBounds = SelectionBounds(),
                      .CellsInserted = m_Selected->Data(),
                      .CellsDeleted = grid.ReadRegion(SelectionBounds())};
    grid.ClearRegion(SelectionBounds());

    return change;
}

std::optional<VersionChange> SelectionManager::Deselect(GameGrid& grid) {
    const auto retValue = [this, &grid]() {
        if (!m_Selected)
            return std::optional<VersionChange>{};

        auto insertedCells =
            grid.InsertGrid(*m_Selected, SelectionBounds().UpperLeft());
        return std::optional<VersionChange>{
            {.Action = SelectionAction::Deselect,
             .SelectionBounds = SelectionBounds(),
             .CellsInserted = insertedCells,
             .CellsDeleted = m_Selected->Data()}};
    }();

    m_LockSelection = true;
    m_AnchorSelection = std::nullopt;
    m_SentinelSelection = std::nullopt;
    m_Selected = std::nullopt;

    return retValue;
}

std::optional<VersionChange> SelectionManager::SelectAll(GameGrid& grid) {
    const auto bounds = grid.BoundingBox();
    m_AnchorSelection = bounds.UpperLeft();
    m_SentinelSelection = bounds.LowerRight() - Vec2{1, 1};

    auto regionDestroyed = grid.SubRegion(SelectionBounds());
    m_Selected = regionDestroyed;
    grid.ClearRegion(SelectionBounds());

    return VersionChange{.Action = SelectionAction::Select,
                         .SelectionBounds = SelectionBounds(),
                         .CellsInserted = m_Selected->Data(),
                         .CellsDeleted = std::move(regionDestroyed).Data()};
}

std::optional<VersionChange> SelectionManager::Copy(GameGrid& grid) {
    if (!m_Selected)
        return std::nullopt;

    ImGui::SetClipboardText(
        RLEEncoder::EncodeRegion(*m_Selected, {{0, 0}, m_Selected->Size()})
            .c_str());
    return Deselect(grid);
}

std::optional<VersionChange> SelectionManager::Cut() {
    if (!m_Selected)
        return std::nullopt;

    ImGui::SetClipboardText(
        RLEEncoder::EncodeRegion(
            *m_Selected, {0, 0, m_Selected->Width(), m_Selected->Height()})
            .c_str());
    return Delete();
}

std::expected<VersionChange, std::optional<uint32_t>>
SelectionManager::Paste(std::optional<Vec2> gridPos, uint32_t warnThreshold,
                        bool unlock) {
    if (unlock)
        m_LockSelection = false;

    if (!gridPos)
        gridPos = m_AnchorSelection;
    if (!gridPos)
        gridPos = {0, 0};

    auto decodeResult =
        RLEEncoder::DecodeRegion(ImGui::GetClipboardText(), warnThreshold);
    if (!decodeResult)
        return std::unexpected{decodeResult.error()};

    m_Selected = std::move(decodeResult->Grid);
    m_AnchorSelection = gridPos;
    m_SentinelSelection = {gridPos->X + m_Selected->Width() - 1,
                         gridPos->Y + m_Selected->Height() - 1};

    return VersionChange{.Action = SelectionAction::Paste,
                         .SelectionBounds = SelectionBounds(),
                         .CellsInserted = m_Selected->Data(),
                         .CellsDeleted = {}};
}

std::optional<VersionChange> SelectionManager::Delete() {
    if (!m_Selected)
        return std::nullopt;

    auto retValue = VersionChange{.Action = SelectionAction::Delete,
                                  .SelectionBounds = SelectionBounds(),
                                  .CellsInserted = {},
                                  .CellsDeleted = m_Selected->Data()};

    m_Selected = std::nullopt;
    m_AnchorSelection = std::nullopt;
    m_SentinelSelection = std::nullopt;

    return retValue;
}

std::optional<VersionChange> SelectionManager::Rotate(bool clockwise) {
    if (!m_Selected)
        return std::nullopt;

    auto upperLeft = SelectionBounds().UpperLeft();
    auto width = static_cast<float>(m_Selected->Width());
    auto height = static_cast<float>(m_Selected->Height());

    auto gridCenter = Vec2F{static_cast<float>(upperLeft.X) + width / 2.f,
                            static_cast<float>(upperLeft.Y) + height / 2.f};

    auto oldCorner = Vec2F{clockwise ? SelectionBounds().LowerLeft()
                                     : SelectionBounds().UpperRight()};
    m_AnchorSelection = RotatePoint(gridCenter, oldCorner, clockwise);
    m_SentinelSelection = *m_AnchorSelection + Vec2{m_Selected->Height() - 1,
                                                    m_Selected->Width() - 1};
    m_Selected->RotateGrid(clockwise);

    return VersionChange{
        .Action = SelectionAction::Rotate,
        .SelectionBounds = SelectionBounds(),
    };
}

std::optional<VersionChange> SelectionManager::Flip(SelectionAction direction) {
    if (!m_Selected)
        return std::nullopt;

    m_Selected->FlipGrid(direction == SelectionAction::FlipVertically);

    return VersionChange{.Action = direction};
}

std::optional<VersionChange> SelectionManager::Nudge(Vec2 translation) {
    if (!m_AnchorSelection || (m_AnchorSelection == m_SentinelSelection))
        return std::nullopt;

    *m_AnchorSelection += translation;
    *m_SentinelSelection += translation;

    auto action = [translation]() {
        if (translation.X < 0)
            return SelectionAction::NudgeLeft;
        else if (translation.X > 0)
            return SelectionAction::NudgeRight;
        else if (translation.Y < 0)
            return SelectionAction::NudgeUp;
        return SelectionAction::NudgeDown;
    }();

    return VersionChange{.Action = action,
                         .SelectionBounds = SelectionBounds(),
                         .NudgeTranslation = translation};
}

std::optional<VersionChange> SelectionManager::InsertNoise(Rect selectionBounds,
                                                           float density) {
    m_Selected = GameGrid::GenerateNoise(selectionBounds, density);
    m_SentinelSelection = selectionBounds.Pos();
    m_AnchorSelection = selectionBounds.LowerRight() - Vec2{1, 1};

    return VersionChange{.Action = SelectionAction::Paste,
                         .SelectionBounds = SelectionBounds(),
                         .CellsInserted = m_Selected->Data()};
}

std::expected<VersionChange, std::string>
SelectionManager::Load(const std::filesystem::path& filePath) {
    auto result = RLEEncoder::ReadRegion(filePath);
    if (!result)
        return std::unexpected{std::move(result.error())};

    m_Selected = std::move(result->Grid);
    m_AnchorSelection = result->Offset;
    m_SentinelSelection = result->Offset + Vec2{m_Selected->Width() - 1,
                                                m_Selected->Height() - 1};

    return VersionChange{.Action = SelectionAction::Paste,
                         .SelectionBounds = SelectionBounds(),
                         .CellsInserted = m_Selected->Data(),
                         .CellsDeleted = {}};
}

bool SelectionManager::Save(const GameGrid& grid,
                            const std::filesystem::path& filePath) const {
    if (!m_Selected) {
        auto boundingBox = grid.BoundingBox();
        return RLEEncoder::WriteRegion(grid, boundingBox, filePath,
                                       boundingBox.Pos());
    }
    return RLEEncoder::WriteRegion(*m_Selected,
                                   Rect{{0, 0}, m_Selected->Size()}, filePath);
}

std::optional<VersionChange>
SelectionManager::HandleAction(SelectionAction action, GameGrid& grid,
                               int32_t nudgeSize) {
    switch (action) {
        using enum SelectionAction;
    case Copy:
        return this->Copy(grid);
    case Cut:
        return this->Cut();
    case Delete:
        return this->Delete();
    case Deselect:
        return this->Deselect(grid);
    case SelectAll:
        return this->SelectAll(grid);
    case NudgeLeft:
        return Nudge({-nudgeSize, 0});
    case NudgeRight:
        return Nudge({nudgeSize, 0});
    case NudgeUp:
        return Nudge({0, -nudgeSize});
    case NudgeDown:
        return Nudge({0, nudgeSize});
    case Rotate:
        return this->Rotate(true);
    case FlipVertically:
        [[fallthrough]];
    case FlipHorizontally:
        return this->Flip(action);
    default:
        assert(false && "Invalid action passed to HandleAction");
    }
    return std::nullopt;
}

void SelectionManager::HandleVersionChange(EditorAction undoRedo,
                                           GameGrid& grid,
                                           const VersionChange& change) {
    if (!change.Action) {
        RestoreGridVersion(undoRedo, grid, change);
        return;
    }

    m_UnlockedOriginalPosition = std::nullopt;

    if (auto* editorAction = std::get_if<EditorAction>(&*change.Action)) {
        if (!change.GridResize)
            return;

        switch (*editorAction) {
        case EditorAction::Resize:
            if (undoRedo == EditorAction::Undo)
                grid = change.GridResize->first;
            else
                grid = GameGrid(grid, change.GridResize->second);
            return;
        default:
            assert(false &&
                   "Invalid EditorAction passed to HandleVersionChange");
        }
    }

    if (auto* gameAction = std::get_if<GameAction>(&*change.Action)) {
        switch (*gameAction) {
        case GameAction::Clear:
            if (undoRedo == EditorAction::Undo) {
                for (auto pos : change.CellsDeleted)
                    grid.Set(pos.X, pos.Y, true);
            } else {
                for (auto pos : change.CellsDeleted)
                    grid.Set(pos.X, pos.Y, false);
            }
            return;
        default:
            assert(false && "Invalid GameAction passed to HandleVersionChange");
        }
    }

    auto* action = std::get_if<SelectionAction>(&*change.Action);
    if (!action)
        return;

    switch (*action) {
        using enum SelectionAction;
    case NudgeLeft:
        [[fallthrough]];
    case NudgeRight:
        [[fallthrough]];
    case NudgeUp:
        [[fallthrough]];
    case NudgeDown:
        if (undoRedo == EditorAction::Undo)
            Nudge(-change.NudgeTranslation);
        else
            Nudge(change.NudgeTranslation);
        return;
    case FlipHorizontally:
        [[fallthrough]];
    case FlipVertically:
        Flip(*action);
        return;
    case Rotate:
        this->Rotate(undoRedo == EditorAction::Redo);
        return;
    case Paste:
        [[fallthrough]];
    case Delete:
        SetSelectionBounds(*change.SelectionBounds);
        m_Selected = GameGrid{change.SelectionBounds->Size()};
        RestoreGridVersion(undoRedo, *m_Selected, change);

        if ((undoRedo == EditorAction::Redo) == (*action == Delete))
            this->Deselect(grid);
        return;
    case Select:
        if (undoRedo == EditorAction::Undo) {
            this->Deselect(grid);
            return;
        }

        SetSelectionBounds(*change.SelectionBounds);
        m_Selected = GameGrid{change.SelectionBounds->Size()};

        for (auto pos : change.CellsInserted)
            m_Selected->Set(pos.X, pos.Y, true);
        for (auto pos : change.CellsDeleted)
            grid.Set(pos.X, pos.Y, false);
        return;
    case Deselect:
        if (undoRedo == EditorAction::Undo) {
            SetSelectionBounds(*change.SelectionBounds);
            m_Selected = GameGrid{change.SelectionBounds->Size()};
            for (auto pos : change.CellsDeleted)
                m_Selected->Set(pos.X, pos.Y, true);
            for (auto pos : change.CellsInserted)
                grid.Set(pos.X, pos.Y, false);
            return;
        }

        this->Deselect(grid);
        return;
    default:
        assert(false && "Invalid action passed to HandleVersionChange");
    }
}

void SelectionManager::RestoreGridVersion(EditorAction undoRedo, GameGrid& grid,
                                          const VersionChange& change) {
    const auto& insertions = undoRedo == EditorAction::Undo
                                 ? change.CellsDeleted
                                 : change.CellsInserted;
    for (const auto pos : insertions)
        grid.Set(pos.X, pos.Y, true);

    const auto& deletions = undoRedo == EditorAction::Undo
                                ? change.CellsInserted
                                : change.CellsDeleted;
    for (const auto pos : deletions)
        grid.Set(pos.X, pos.Y, false);
}

Rect SelectionManager::SelectionBounds() const {
    return {std::min(m_AnchorSelection->X, m_SentinelSelection->X),
            std::min(m_AnchorSelection->Y, m_SentinelSelection->Y),
            std::abs(m_SentinelSelection->X - m_AnchorSelection->X) + 1,
            std::abs(m_SentinelSelection->Y - m_AnchorSelection->Y) + 1};
}

std::pair<std::optional<VersionChange>, std::optional<VersionChange>>
SelectionManager::ModifySelectionBounds(GameGrid& grid, Rect bounds) {
    if (CanDrawGrid() && bounds == SelectionBounds()) {
        return {std::nullopt, std::nullopt};
    }

    const bool previouslyValidBounds = CanDrawGrid();
    const bool notSameBounds =
        previouslyValidBounds && bounds.Size() != SelectionBounds().Size();
    const auto ogPos =
        previouslyValidBounds ? SelectionBounds().Pos() : std::optional<Vec2>{};

    auto change1 = [&] -> std::optional<VersionChange> {
        if (notSameBounds) {
            return Deselect(grid);
        }
        return {};
    }();

    if ((bounds.Width == 0) != (bounds.Height == 0)) {
        const auto size = std::max(bounds.Width, bounds.Height);
        bounds.Width = size;
        bounds.Height = size;
    } else if (bounds.Width == 0 && bounds.Height == 0) {
        bounds.Width = 2;
        bounds.Height = 2;
    }
    SetSelectionBounds(bounds);

    auto change2 = [&] -> std::optional<VersionChange> {
        if (!previouslyValidBounds || notSameBounds) {
            return Select(grid);
        }
        if (previouslyValidBounds) {
            return VersionChange{.Action = SelectionAction::NudgeDown,
                                 .SelectionBounds = SelectionBounds(),
                                 .NudgeTranslation = bounds.Pos() - *ogPos};
        }
        return {};
    }();

    return {std::move(change1), std::move(change2)};
}

bool SelectionManager::GridAlive() const {
    return m_Selected && !m_Selected->Population().is_zero();
}

const LifeHashSet& SelectionManager::GridData() const {
    return m_Selected->Data();
}

const BigInt& SelectionManager::SelectedPopulation() const {
    return m_Selected ? m_Selected->Population() : BigZero;
}

bool SelectionManager::CanDrawSelection() const {
    return m_AnchorSelection && m_SentinelSelection;
}

bool SelectionManager::CanDrawLargeSelection() const {
    return m_AnchorSelection && m_SentinelSelection &&
           (*m_AnchorSelection != *m_SentinelSelection);
}

bool SelectionManager::CanDrawGrid() const { return m_Selected.has_value(); }

void SelectionManager::SetSelectionBounds(Rect bounds) {
    m_AnchorSelection = bounds.Pos();
    m_SentinelSelection =
        *m_AnchorSelection + Vec2{bounds.Width == 0 ? 0 : bounds.Width - 1,
                                  bounds.Height == 0 ? 0 : bounds.Height - 1};
}

Vec2 SelectionManager::RotatePoint(Vec2F center, Vec2F point, bool clockwise) {
    auto offset =
        Vec2F{static_cast<float>(point.X), static_cast<float>(point.Y)} -
        center;
    auto rotated =
        clockwise ? Vec2F{-offset.Y, offset.X} : Vec2F{offset.Y, -offset.X};
    auto result = rotated + center;

    auto retValue =
        Vec2{static_cast<int32_t>(m_RotationParity ? std::floor(result.X)
                                                   : std::ceil(result.X)),
             static_cast<int32_t>(!m_RotationParity ? std::floor(result.Y)
                                                    : std::ceil(result.Y))};
    m_RotationParity = !m_RotationParity;
    return retValue;
}
} // namespace gol
