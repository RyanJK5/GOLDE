
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
#include "Plane.hpp"
#include "Torus.hpp"

namespace Golde {
std::expected<GameGrid, std::string>
GameGrid::GenerateNoise(HashLifeCache& cache, Rect bounds, float density,
                        uint32_t warnThreshold) {
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

        GameGrid ret{cache, bounds.Width, bounds.Height};
        ret.m_HashLifeData = HashQuadtree{cache, cells};
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

    GameGrid ret{cache, bounds.Width, bounds.Height};
    ret.m_HashLifeData = HashQuadtree{cache, cells};
    return ret;
}

GameGrid::GameGrid(HashLifeCache& cache, int32_t width, int32_t height)
    : m_HashLifeData(cache),
      m_Algorithm(std::make_unique<HashLife>(cache.GetRule())), m_Width(width),
      m_Height(height) {
    m_Algorithm->SetTopology(
        std::make_unique<Plane>(Rect{0, 0, width, height}));
}

GameGrid::GameGrid(HashLifeCache& cache, Size2 size)
    : GameGrid(cache, size.Width, size.Height) {}

GameGrid::GameGrid(const GameGrid& other, Size2 size)
    : m_HashLifeData(other.m_HashLifeData.Extract({{0, 0}, size})),
      m_Width(size.Width), m_Height(size.Height) {
    m_RuleString = other.m_RuleString;
    m_Algorithm = other.m_Algorithm->Clone();

    // When resizing a bounded universe, keep topology consistent with the
    // rule string if it encodes explicit bounds (e.g. :T600,136). Otherwise,
    // fall back to a bounded plane.
    const Rect newBounds{0, 0, size.Width, size.Height};
    const auto kind = LifeRule::ExtractTopologyKind(m_RuleString);
    const auto dims = LifeRule::ExtractDimensions(m_RuleString);
    const bool matchesExplicitDims = dims && dims->Width == size.Width &&
                                     dims->Height == size.Height &&
                                     (size.Width > 0 || size.Height > 0);

    if (kind && matchesExplicitDims) {
        switch (*kind) {
        case TopologyKind::Plane:
            m_Algorithm->SetTopology(std::make_unique<Plane>(newBounds));
            break;
        case TopologyKind::Torus:
            m_Algorithm->SetTopology(std::make_unique<Torus>(newBounds));
            break;
        }
    } else {
        m_Algorithm->SetTopology(std::make_unique<Plane>(newBounds));
    }
}

GameGrid::GameGrid(const GameGrid& other)
    : m_HashLifeData(other.m_HashLifeData), m_RuleString(other.m_RuleString),
      m_Algorithm(other.m_Algorithm->Clone()), m_Width(other.m_Width),
      m_Height(other.m_Height), m_Generation(other.m_Generation) {}

GameGrid& GameGrid::operator=(const GameGrid& other) {
    if (this == &other) {
        return *this;
    }

    m_HashLifeData = other.m_HashLifeData;
    m_RuleString = other.m_RuleString;
    m_Width = other.m_Width;
    m_Height = other.m_Height;
    m_Algorithm = other.m_Algorithm->Clone();
    m_Generation = other.m_Generation;

    return *this;
}

GameGrid::GameGrid(const HashQuadtree& data, Size2 size)
    : m_HashLifeData(data),
      m_Algorithm(std::make_unique<HashLife>(data.Cache().GetRule())),
      m_Width(size.Width), m_Height(size.Height) {
    m_Algorithm->SetTopology(
        std::make_unique<Plane>(Rect{0, 0, size.Width, size.Height}));
}

void GameGrid::SetAlgorithm(std::unique_ptr<LifeAlgorithm> algo) {
    m_Algorithm = std::move(algo);
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

HashLifeCache& GameGrid::Cache() const { return m_HashLifeData.Cache(); }
const HashQuadtree& GameGrid::Data() const { return m_HashLifeData; }

void GameGrid::SetRule(const LifeRule& rule) {
    Cache().SetRule(rule);
    m_Algorithm->SetRule(Cache().GetRule());
}

void GameGrid::SetRule(const LifeRule& rule, std::string_view ruleString) {
    SetRule(rule);
    m_RuleString = ruleString;
}

std::string_view GameGrid::GetRuleString() const { return m_RuleString; }

BigInt GameGrid::Update(const BigInt& numSteps, std::stop_token stopToken) {
    m_SortedCacheInvalidated = true;

    const auto generations =
        m_Algorithm->Step(m_HashLifeData, numSteps, stopToken);
    m_Generation += generations;
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

    return true;
}

GameGrid GameGrid::SubRegion(Rect region) const {
    auto subRegion = GameGrid{m_HashLifeData.Extract(region), region.Size()};
    subRegion.SetRule(Cache().GetRule(), m_RuleString);
    return subRegion;
}

void GameGrid::ClearRegion(Rect region) {
    m_HashLifeData.Clear(region);
    m_SortedCacheInvalidated = true;
}

void GameGrid::InsertGrid(const GameGrid& region, Vec2 pos) {
    m_HashLifeData.Insert(region.Data(), pos);
    m_SortedCacheInvalidated = true;
}

void GameGrid::RotateGrid(bool clockwise) {
    // Probably more easily read as ((width - 1) / 2, (height - 1) / 2)
    const Vec2F center{((m_Width / 2.f) - 0.5f), ((m_Height / 2.f) - 0.5F)};
    std::vector<Vec2> newSet{};
    newSet.reserve(Population().convert_to<size_t>());

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

    m_HashLifeData = HashQuadtree{Cache(), newSet};
}

void GameGrid::FlipGrid(bool vertical) {
    std::vector<Vec2> newData{};
    newData.reserve(Population().convert_to<size_t>());
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

    m_HashLifeData = HashQuadtree{Cache(), newData};
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

bool GameGrid::ShouldAllowUniverseEdits() const {
    return UniverseDepth() < 4096;
}

int32_t GameGrid::UniverseDepth() const {
    return m_HashLifeData.CalculateDepth();
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
} // namespace Golde
