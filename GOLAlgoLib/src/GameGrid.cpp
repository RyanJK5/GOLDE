
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
#include "HashLife.hpp"
#include "HashQuadtree.hpp"
#include "LifeAlgorithm.hpp"
#include "LifeHashSet.hpp"

namespace gol {
std::expected<GameGrid, std::string>
GameGrid::GenerateNoise(Rect bounds, float density, uint32_t warnThreshold) {
    static std::random_device random{};
    static std::mt19937 generator{random()};

    // Since our density is not totally accurate because it can produce
    // overlapping cells, we create the illusion that it is fully accurate when
    // density == 1.
    if (bounds.Height > (std::numeric_limits<int32_t>::max() / bounds.Width) ||
        bounds.Width * bounds.Height > static_cast<int32_t>(warnThreshold)) {
        return std::unexpected{"Too many cells to generate."};
    }
    if (density == 1.f) {
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
    : m_Algorithm(std::make_unique<HashLife>()), m_Width(width),
      m_Height(height) {}

GameGrid::GameGrid(Size2 size) : GameGrid(size.Width, size.Height) {}

GameGrid::GameGrid(const GameGrid& other, Size2 size) : GameGrid(size) {
    auto cropped =
        other.Data() |
        std::views::filter([this](Vec2 pos) { return InBounds(pos); }) |
        std::ranges::to<std::vector<Vec2>>();
    m_HashLifeData = HashQuadtree{cropped};
    m_Population = m_HashLifeData.Population();
}

GameGrid::GameGrid(const GameGrid& other)
    : m_HashLifeData(other.m_HashLifeData), m_Width(other.m_Width),
      m_Height(other.m_Height), m_Population(other.m_Population) {}

GameGrid& GameGrid::operator=(const GameGrid& other) {
    if (this == &other) {
        return *this;
    }

    m_HashLifeData = other.m_HashLifeData;
    m_Width = other.m_Width;
    m_Height = other.m_Height;
    m_Population = other.m_Population;

    return *this;
}

GameGrid::GameGrid(const HashQuadtree& data, Size2 size)
    : m_Width(size.Width), m_Height(size.Height), m_HashLifeData(data),
      m_Population(data.Population()),
      m_Algorithm(std::make_unique<HashLife>()) {}

void GameGrid::SetAlgorithm(std::string_view algorithmIdentifier) {
    if (algorithmIdentifier == m_Algorithm->GetIdentifier()) {
        return;
    }
    m_Algorithm = LifeAlgorithm::MakeAlgorithm(algorithmIdentifier);
}

bool GameGrid::Dead() const { return m_HashLifeData.empty(); }

Rect GameGrid::BoundingBox() const {
    if (Bounded()) {
        return {0, 0, m_Width, m_Height};
    }

    return m_HashLifeData.FindBoundingBox();
}

std::span<Vec2> GameGrid::SortedData() const {
    ValidateSortedCache();
    return std::span{m_SortedData};
}

const HashQuadtree& GameGrid::Data() const { return m_HashLifeData; }

void GameGrid::SetRule(const LifeRule& rule) { m_Algorithm->SetRule(rule); }

BigInt GameGrid::Update(const BigInt& numSteps, std::stop_token stopToken) {
    m_SortedCacheInvalidated = true;

    const auto generations =
        m_Algorithm->Step(m_HashLifeData, numSteps, stopToken);
    m_Generation += generations;
    m_Population = m_HashLifeData.Population();
    m_SortedCacheInvalidated = true;

    return generations;
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

void GameGrid::ClearRegion(Rect region) {
    m_HashLifeData.Clear(region);
    m_Population = m_HashLifeData.Population();
    m_SortedCacheInvalidated = true;
}

void GameGrid::InsertGrid(const GameGrid& region, Vec2 pos) {
    m_HashLifeData.Insert(region.Data(), pos);
    m_Population = m_HashLifeData.Population();
    m_SortedCacheInvalidated = true;
}

void GameGrid::RotateGrid(bool clockwise) {
    // Probably more easily read as ((width - 1) / 2, (height - 1) / 2)
    const Vec2F center{((m_Width / 2.f) - 0.5f), ((m_Height / 2.f) - 0.5F)};
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
