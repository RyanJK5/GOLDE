
#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <ranges>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <vector>

#include "GameGrid.hpp"
#include "Graphics2D.hpp"
#include "HashQuadtree.hpp"
#include "LifeAlgorithm.hpp"
#include "LifeHashSet.hpp"

namespace gol {
GameGrid GameGrid::GenerateNoise(Rect bounds, float density) {
    static std::random_device random{};
    static std::mt19937 generator{random()};

    // Since our density is not totally accurate because it can produce
    // overlapping cells, we create the illusion that it is fully accurate when
    // density == 1.
    if (density == 1.F) {
        std::vector<Vec2> cells{};
        cells.reserve(static_cast<size_t>(bounds.Width) * bounds.Height);
        for (auto x = 0; x < bounds.Width; x++) {
            for (auto y = 0; y < bounds.Height; y++) {
                cells.emplace_back(x, y);
            }
        }

        GameGrid ret{bounds.Width, bounds.Height};
        ret.m_HashLifeData = HashQuadtree{cells};
        ret.m_Population = ret.m_HashLifeData.Population();
        return ret;
    }

    // Determine the number of points to generate
    const auto area = bounds.Width * bounds.Height;
    const auto mean = density * area;
    std::poisson_distribution<int> countDist{mean};
    const auto finalCount = countDist(generator); // The random "actual" count

    // Prepare distributions for coordinates
    std::uniform_int_distribution<int32_t> distX{0, bounds.Width - 1};
    std::uniform_int_distribution<int32_t> distY{0, bounds.Height - 1};

    std::vector<Vec2> cells{};
    cells.reserve(finalCount);
    std::ranges::generate_n(std::back_inserter(cells), finalCount, [&] {
        return Vec2{distX(generator), distY(generator)};
    });

    GameGrid ret{bounds.Width, bounds.Height};
    ret.m_HashLifeData = HashQuadtree{cells};
    ret.m_Population = ret.m_HashLifeData.Population();
    return ret;
}

GameGrid::GameGrid(int32_t width, int32_t height)
    : m_Algorithm(LifeAlgorithm::HashLife), m_Width(width), m_Height(height) {}

GameGrid::GameGrid(Size2 size) : GameGrid(size.Width, size.Height) {}

GameGrid::GameGrid(const GameGrid& other, Size2 size) : GameGrid(size) {
    auto cropped =
        other.Data() |
        std::views::filter([this](Vec2 pos) { return InBounds(pos); }) |
        std::ranges::to<std::vector<Vec2>>();
    m_HashLifeData = HashQuadtree{cropped};
    m_Population = m_HashLifeData.Population();
}

GameGrid::GameGrid(const HashQuadtree& data, Size2 size)
    : m_Width(size.Width), m_Height(size.Height), m_HashLifeData(data),
      m_Population(data.Population()), m_Algorithm(LifeAlgorithm::HashLife) {}

void GameGrid::SetAlgorithm(LifeAlgorithm algorithm) {
    m_Algorithm = algorithm;
}

bool GameGrid::Dead() const { return m_HashLifeData.empty(); }

Rect GameGrid::BoundingBox() const {
    if (Bounded()) {
        return {0, 0, m_Width, m_Height};
    }

    const auto findBox = [](std::ranges::input_range auto&& data) {
        auto least = Vec2{std::numeric_limits<int32_t>::max(),
                          std::numeric_limits<int32_t>::max()};
        auto most = Vec2{std::numeric_limits<int32_t>::min(),
                         std::numeric_limits<int32_t>::min()};
        for (const auto value : data) {
            least.X = std::min(least.X, value.X);
            least.Y = std::min(least.Y, value.Y);

            most.X = std::max(most.X, value.X);
            most.Y = std::max(most.Y, value.Y);
        }

        return Rect{least.X, least.Y, most.X - least.X + 1,
                    most.Y - least.Y + 1};
    };

    if (m_HashLifeData.empty()) {
        return {0, 0, 0, 0};
    }

    return findBox(m_HashLifeData);
}

std::span<Vec2> GameGrid::SortedData() const {
    ValidateSortedCache();
    return std::span{m_SortedData};
}

const HashQuadtree& GameGrid::Data() const { return m_HashLifeData; }

const HashQuadtree& GameGrid::IterableData() const { return m_HashLifeData; }

BigInt GameGrid::Update(const BigInt& numSteps, std::stop_token stopToken) {
    m_SortedCacheInvalidated = true;

    switch (m_Algorithm) {
    case LifeAlgorithm::SparseLife:
        // SparseLife is currently disabled with the intention of replacing it
        // long-term with a more efficient algorithm.
        return BigInt{};
    case LifeAlgorithm::HashLife:
        const auto generations = HashLife(m_HashLifeData, numSteps, stopToken);
        m_Generation += generations;
        m_Population = m_HashLifeData.Population();
        m_SortedCacheInvalidated = true;
        return generations;
    }

    return 0;
}

bool GameGrid::Toggle(int32_t x, int32_t y) {
    if (!InBounds(x, y)) {
        return false;
    }

    const auto current = m_HashLifeData.Get({x, y});
    return Set(x, y, !current);
}

bool GameGrid::Set(int32_t x, int32_t y, bool active) {
    if (!InBounds(x, y)) {
        return false;
    }
    m_SortedCacheInvalidated = true;

    const auto before = m_HashLifeData.Get({x, y});
    if (before == active) {
        return true;
    }

    m_HashLifeData.Set({x, y}, active);
    m_Population += active ? BigOne : -BigOne;

    return true;
}

GameGrid GameGrid::SubRegion(Rect region) const {
    return GameGrid{m_HashLifeData.Extract(region), region.Size()};
}

// TODO: Unsafe if running hash life & cache is invalidated
LifeHashSet GameGrid::ReadRegion(Rect region) const {
    LifeHashSet result{};
    for (const auto pos : std::ranges::subrange(m_HashLifeData.begin(region),
                                                m_HashLifeData.end())) {
        result.insert(pos);
    }
    return result;
}

void GameGrid::ClearRegion(Rect region) {
    m_HashLifeData.Clear(region);
    m_Population = m_HashLifeData.Population();
    m_SortedCacheInvalidated = true;
}

LifeHashSet GameGrid::InsertGrid(const GameGrid& region, Vec2 pos) {
    LifeHashSet result{};
    for (const auto cell : region.Data()) {
        const Vec2 offsetPos{pos.X + cell.X, pos.Y + cell.Y};
        if (m_HashLifeData.Get(offsetPos)) {
            continue;
        }
        m_HashLifeData.Set(offsetPos, true);
        result.insert(offsetPos);
        m_Population++;
    }
    m_SortedCacheInvalidated = true;
    return result;
}

void GameGrid::RotateGrid(bool clockwise) {
    // Probably more easily read as ((width - 1) / 2, (height - 1) / 2)
    const Vec2F center{((m_Width / 2.F) - 0.5F), ((m_Height / 2.F) - 0.5F)};
    std::vector<Vec2> newSet{};
    newSet.reserve(static_cast<size_t>(m_Population.convert_to<int64_t>()));

    for (const auto cellPos : m_HashLifeData) {
        const auto offset = Vec2F{static_cast<float>(cellPos.X),
                                  static_cast<float>(cellPos.Y)} -
                            center;
        const auto rotated =
            clockwise ? Vec2F{-offset.Y, offset.X} : Vec2F{offset.Y, -offset.X};
        const auto result = rotated + Vec2F{center.Y, center.X};
        newSet.emplace_back(static_cast<int32_t>(result.X),
                            static_cast<int32_t>(result.Y));
    }
    std::swap(m_Width, m_Height);
    m_SortedCacheInvalidated = true;

    m_HashLifeData = HashQuadtree{newSet};
    m_Population = m_HashLifeData.Population();
}

void GameGrid::FlipGrid(bool vertical) {
    std::vector<Vec2> newData{};
    newData.reserve(static_cast<size_t>(m_Population.convert_to<int64_t>()));
    if (!Bounded()) {
        for (const auto pos : m_HashLifeData) {
            if (vertical) {
                newData.emplace_back(pos.X, -pos.Y);
            } else {
                newData.emplace_back(-pos.X, pos.Y);
            }
        }
    } else {
        for (const auto pos : m_HashLifeData) {
            if (vertical) {
                newData.emplace_back(pos.X, m_Height - 1 - pos.Y);
            } else {
                newData.emplace_back(m_Width - 1 - pos.X, pos.Y);
            }
        }
    }
    m_SortedCacheInvalidated = true;

    m_HashLifeData = HashQuadtree{newData};
    m_Population = m_HashLifeData.Population();
}

std::optional<bool> GameGrid::Get(int32_t x, int32_t y) const {
    return Get({x, y});
}

std::optional<bool> GameGrid::Get(Vec2 pos) const {
    if (!InBounds(pos)) {
        return std::nullopt;
    }
    return m_HashLifeData.Get(pos);
}

bool GameGrid::ShouldValidateCache() const {
    return m_HashLifeData.Population() < 100'000'000;
}

void GameGrid::ValidateSortedCache() const {
    if (!m_SortedCacheInvalidated) {
        return;
    }
    if (!ShouldValidateCache()) {
        throw std::runtime_error("Too many cells to validate cache");
    }

    m_SortedData = m_HashLifeData | std::ranges::to<std::vector<Vec2>>();
    m_SortedCacheInvalidated = false;
    std::ranges::sort(m_SortedData, RowMajorEqual{});
}
} // namespace gol
