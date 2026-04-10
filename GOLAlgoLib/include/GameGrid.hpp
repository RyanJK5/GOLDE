#ifndef GameGrid_hpp_
#define GameGrid_hpp_

#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

#include "Graphics2D.hpp"
#include "HashLife.hpp"
#include "HashQuadtree.hpp"
#include "LifeAlgorithm.hpp"
#include "LifeHashSet.hpp"

namespace gol {

// The class responsible for the Game of Life universe.
class GameGrid {
  public:
    // Returns a GameGrid with randomly generated cells according to the
    // provided density.
    static std::expected<GameGrid, std::string>
    GenerateNoise(Rect bounds, float density, uint32_t warnThreshold);

    // Calling with `width` or `height` set to zero creates an unbounded
    // universe.
    GameGrid(int32_t width = 0, int32_t height = 0);
    GameGrid(Size2 size);

    GameGrid(const GameGrid& other, Size2 size);

    GameGrid(const HashQuadtree& data, Size2 size);

    GameGrid(const GameGrid& other);

    GameGrid& operator=(const GameGrid& other);

    GameGrid(GameGrid&& other) = default;

    GameGrid& operator=(GameGrid&& other) = default;

    ~GameGrid() = default;

    const LifeAlgorithm& GetAlgorithm() const { return *m_Algorithm; }

    void SetAlgorithm(std::string_view algorithm);

    // Advances the universe `numSteps` generations. A stop token can optionally
    // be provided if the thread may terminate during advance.
    BigInt Update(const BigInt& numSteps, std::stop_token stopToken = {});

    int32_t Width() const { return m_Width; }
    int32_t Height() const { return m_Height; }

    Size2 Size() const { return {m_Width, m_Height}; }

    bool Bounded() const { return m_Width > 0 && m_Height > 0; }

    // If bounded, returns the universe's bounds; otherwise, returns the
    // smallest `Rect` that encompasses all live cells.
    Rect BoundingBox() const;

    bool InBounds(int32_t x, int32_t y) const { return InBounds({x, y}); }
    bool InBounds(Vec2 pos) const {
        return !Bounded() || Rect{0, 0, m_Width, m_Height}.InBounds(pos);
    }

    const BigInt& Generation() const { return m_Generation; }
    const BigInt& Population() const { return m_Population; }

    // Indicates if the universe contains any live cells
    bool Dead() const;

    bool Set(int32_t x, int32_t y, bool active);
    bool Toggle(int32_t x, int32_t y);

    // Copies provided region to a new GameGrid.
    GameGrid SubRegion(Rect region) const;

    // Kills all cells in `region`.
    void ClearRegion(Rect region);

    // Adds all cells from `grid` to this object, with each cell
    // offset by `offset`. Returns the set of all sells that were
    // not already present in this object.
    void InsertGrid(const GameGrid& grid, Vec2 offset);

    // Performs a 90 degree rotation.
    void RotateGrid(bool clockwise = true);

    // If `vertical` is true, flips the grid along its centered
    // vertical axis. Otherwise, flips the grid along its centered
    // horizontal axis.
    void FlipGrid(bool vertical);

    // Returns the state of the cell at (`x`, `y`) if it is within
    // bounds.
    std::optional<bool> Get(int32_t x, int32_t y) const;
    // Returns the state of the cell at `pos` if it is within
    // bounds.
    std::optional<bool> Get(Vec2 pos) const;

    // Returns a sorted set of the universe's data.
    std::span<Vec2> SortedData() const;
    // Returns an unordered set of the universe's data.
    const HashQuadtree& Data() const;

    void SetRule(const LifeRule& rule);

    bool ShouldValidateCache() const;

  private:
    void ValidateSortedCache() const;

  private:
    HashQuadtree m_HashLifeData;
    std::unique_ptr<LifeAlgorithm> m_Algorithm = std::make_unique<HashLife>();

    mutable std::vector<Vec2>
        m_SortedData; // Declared mutable due to hidden cache validation

    mutable bool m_SortedCacheInvalidated = true;

    int32_t m_Width;
    int32_t m_Height;

    BigInt m_Population{};
    BigInt m_Generation{};
};
} // namespace gol

#endif
