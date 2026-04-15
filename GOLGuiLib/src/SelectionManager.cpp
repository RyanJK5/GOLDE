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
            return UpdateUnlockedSelection(grid, gridPos);

        auto change = Select(grid);
        return {.Change = std::move(change), .BeginSelection = false};
    }

    auto deselectChange = Deselect(grid);
    m_AnchorSelection = gridPos;
    m_SentinelSelection = gridPos;
    return {.Change = deselectChange, .BeginSelection = false};
}

SelectionUpdateResult SelectionManager::UpdateUnlockedSelection(GameGrid& grid,
                                                                Vec2 gridPos) {
    if (gridPos == *m_SentinelSelection && m_UnlockedOriginalPosition) {
        auto result = SelectionUpdateResult{.Change = CaptureState(grid),
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

std::optional<VersionState> SelectionManager::Select(GameGrid& grid) {
    m_Selected = grid.SubRegion(SelectionBounds());
    grid.ClearRegion(SelectionBounds());
    return CaptureState(grid);
}

std::optional<VersionState> SelectionManager::Deselect(GameGrid& grid) {
    if (!m_Selected) {
        return std::nullopt;
    }

    grid.InsertGrid(*m_Selected, SelectionBounds().UpperLeft());

    m_LockSelection = true;
    m_AnchorSelection = std::nullopt;
    m_SentinelSelection = std::nullopt;
    m_Selected = std::nullopt;

    return CaptureState(grid);
}

std::optional<VersionState> SelectionManager::SelectAll(GameGrid& grid) {
    const auto bounds = grid.BoundingBox();
    m_AnchorSelection = bounds.UpperLeft();
    m_SentinelSelection = bounds.LowerRight() - Vec2{1, 1};

    m_Selected = grid.SubRegion(SelectionBounds());
    grid.ClearRegion(SelectionBounds());

    return CaptureState(grid);
}

std::optional<VersionState> SelectionManager::Copy(GameGrid& grid) {
    if (!m_Selected || !m_Selected->ShouldValidateCache()) {
        return std::nullopt;
    }

    ImGui::SetClipboardText(
        FileEncoder::EncodeRegion(*m_Selected, {{0, 0}, m_Selected->Size()})
            .c_str());
    return Deselect(grid);
}

std::optional<VersionState> SelectionManager::Cut(const GameGrid& grid) {
    if (!m_Selected || !m_Selected->ShouldValidateCache())
        return std::nullopt;

    ImGui::SetClipboardText(
        FileEncoder::EncodeRegion(
            *m_Selected, {0, 0, m_Selected->Width(), m_Selected->Height()})
            .c_str());
    return Delete(grid);
}

std::expected<VersionState, FileEncoder::DecodeError>
SelectionManager::Paste(const GameGrid& grid, std::optional<Vec2> gridPos,
                        uint32_t warnThreshold, bool unlock) {
    if (unlock)
        m_LockSelection = false;

    if (!gridPos)
        gridPos = m_AnchorSelection;

    auto decodeResult =
        FileEncoder::DecodeRegion(ImGui::GetClipboardText(), warnThreshold);
    if (!decodeResult) {
        return std::unexpected<FileEncoder::DecodeError>{decodeResult.error()};
    }

    if (!gridPos) {
        gridPos = decodeResult->Offset;
    }

    m_Selected = std::move(decodeResult->Grid);
    m_AnchorSelection = gridPos;
    m_SentinelSelection = {gridPos->X + m_Selected->Width() - 1,
                           gridPos->Y + m_Selected->Height() - 1};

    return CaptureState(grid);
}

std::optional<VersionState> SelectionManager::Delete(const GameGrid& grid) {
    if (!m_Selected)
        return std::nullopt;

    m_Selected = std::nullopt;
    m_AnchorSelection = std::nullopt;
    m_SentinelSelection = std::nullopt;

    return CaptureState(grid);
}

std::optional<VersionState> SelectionManager::Rotate(bool clockwise,
                                                     const GameGrid& grid) {
    if (!m_Selected || !m_Selected->ShouldValidateCache())
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

    return CaptureState(grid);
}

std::optional<VersionState> SelectionManager::Flip(SelectionAction direction,
                                                   const GameGrid& grid) {
    if (!m_Selected || !m_Selected->ShouldValidateCache())
        return std::nullopt;

    m_Selected->FlipGrid(direction == SelectionAction::FlipVertically);

    return CaptureState(grid);
}

std::optional<VersionState> SelectionManager::Nudge(Vec2 translation,
                                                    const GameGrid& grid) {
    if (!m_AnchorSelection || (m_AnchorSelection == m_SentinelSelection))
        return std::nullopt;

    *m_AnchorSelection += translation;
    *m_SentinelSelection += translation;

    return CaptureState(grid);
}

std::optional<VersionState>
SelectionManager::InsertNoise(const GameGrid& grid, Rect selectionBounds,
                              uint32_t warnThreshold, float density) {
    auto result =
        GameGrid::GenerateNoise(selectionBounds, density, warnThreshold);
    if (result) {
        m_Selected = std::move(*result);
    } else {
        return std::nullopt;
    }

    m_SentinelSelection = selectionBounds.Pos();
    m_AnchorSelection = selectionBounds.LowerRight() - Vec2{1, 1};

    return CaptureState(grid);
}

std::expected<VersionState, FileEncoder::DecodeError>
SelectionManager::Load(const GameGrid& grid,
                       const std::filesystem::path& filePath) {
    auto result = FileEncoder::ReadRegion(filePath);
    if (!result)
        return std::unexpected{std::move(result.error())};

    m_Selected = std::move(result->Grid);
    m_AnchorSelection = result->Offset;
    m_SentinelSelection = result->Offset + Vec2{m_Selected->Width() - 1,
                                                m_Selected->Height() - 1};

    return CaptureState(grid);
}

bool SelectionManager::Save(const GameGrid& grid,
                            const std::filesystem::path& filePath) const {
    if (!m_Selected) {
        auto boundingBox = grid.BoundingBox();
        return FileEncoder::WriteRegion(grid, boundingBox, filePath,
                                        boundingBox.Pos());
    }
    return FileEncoder::WriteRegion(*m_Selected,
                                    Rect{{0, 0}, m_Selected->Size()}, filePath);
}

std::optional<VersionState>
SelectionManager::HandleAction(SelectionAction action, GameGrid& grid,
                               int32_t nudgeSize) {
    switch (action) {
        using enum SelectionAction;
    case Copy:
        return this->Copy(grid);
    case Cut:
        return this->Cut(grid);
    case Delete:
        return this->Delete(grid);
    case Deselect:
        return this->Deselect(grid);
    case SelectAll:
        return this->SelectAll(grid);
    case NudgeLeft:
        return Nudge({-nudgeSize, 0}, grid);
    case NudgeRight:
        return Nudge({nudgeSize, 0}, grid);
    case NudgeUp:
        return Nudge({0, -nudgeSize}, grid);
    case NudgeDown:
        return Nudge({0, nudgeSize}, grid);
    case Rotate:
        return this->Rotate(true, grid);
    case FlipVertically:
        [[fallthrough]];
    case FlipHorizontally:
        return this->Flip(action, grid);
    default:
        assert(false && "Invalid action passed to HandleAction");
    }
    return std::nullopt;
}

void SelectionManager::HandleVersionChange(EditorAction undoRedo,
                                           GameGrid& grid,
                                           const VersionState& state) {
    RestoreGridVersion(undoRedo, grid, state);
}

void SelectionManager::RestoreGridVersion(EditorAction, GameGrid& grid,
                                          const VersionState& state) {
    m_UnlockedOriginalPosition = std::nullopt;
    grid = state.Universe;

    if (state.SelectionBounds.has_value()) {
        SetSelectionBounds(*state.SelectionBounds);
        m_Selected = state.SelectionUniverse;
    } else {
        m_AnchorSelection = std::nullopt;
        m_SentinelSelection = std::nullopt;
        m_Selected = std::nullopt;
        m_LockSelection = true;
    }
}

Rect SelectionManager::SelectionBounds() const {
    return {std::min(m_AnchorSelection->X, m_SentinelSelection->X),
            std::min(m_AnchorSelection->Y, m_SentinelSelection->Y),
            std::abs(m_SentinelSelection->X - m_AnchorSelection->X) + 1,
            std::abs(m_SentinelSelection->Y - m_AnchorSelection->Y) + 1};
}

std::pair<std::optional<VersionState>, std::optional<VersionState>>
SelectionManager::ModifySelectionBounds(GameGrid& grid, Rect bounds) {
    if (CanDrawGrid() && bounds == SelectionBounds()) {
        return {std::nullopt, std::nullopt};
    }

    const bool previouslyValidBounds = CanDrawGrid();
    const bool notSameBounds =
        previouslyValidBounds && bounds.Size() != SelectionBounds().Size();
    const auto ogPos =
        previouslyValidBounds ? SelectionBounds().Pos() : std::optional<Vec2>{};

    auto change1 = [&] -> std::optional<VersionState> {
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

    auto change2 = [&] -> std::optional<VersionState> {
        if (!previouslyValidBounds || notSameBounds) {
            return Select(grid);
        }
        if (previouslyValidBounds) {
            return CaptureState(grid);
        }
        return {};
    }();

    return {std::move(change1), std::move(change2)};
}

bool SelectionManager::GridAlive() const {
    return m_Selected && !m_Selected->Population().is_zero();
}

const HashQuadtree& SelectionManager::GridData() const {
    return m_Selected->Data();
}

VersionState SelectionManager::CaptureState(const GameGrid& grid) const {
    VersionState state{};
    state.Universe = grid;

    if (CanDrawGrid()) {
        state.SelectionBounds = SelectionBounds();
        state.SelectionUniverse = *m_Selected;
    }

    return state;
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
