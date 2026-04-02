
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
#include <set>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
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
        GameGrid ret{bounds.Width, bounds.Height};
        for (auto x = 0; x < bounds.Width; x++) {
            for (auto y = 0; y < bounds.Height; y++) {
                ret.m_Data.insert({x, y});
            }
        }
        ret.m_Population = BigInt{ret.m_Data.size()};
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

    // Fill the container
    GameGrid ret{bounds.Width, bounds.Height};
    ret.m_Data.reserve(finalCount);

    std::ranges::generate_n(
        std::inserter(ret.m_Data, ret.m_Data.end()), finalCount,
        [&] { return Vec2{distX(generator), distY(generator)}; });

    ret.m_Population = BigInt{ret.m_Data.size()};
    return ret;
}

GameGrid::GameGrid(int32_t width, int32_t height)
    : m_Algorithm(LifeAlgorithm::HashLife), m_Width(width), m_Height(height) {}

GameGrid::GameGrid(Size2 size) : GameGrid(size.Width, size.Height) {}

GameGrid::GameGrid(const GameGrid& other, Size2 size) : GameGrid(size) {
    m_Population = other.m_Population;
    m_Data = other.Data() |
             std::views::filter([this](Vec2 pos) { return InBounds(pos); }) |
             std::ranges::to<LifeHashSet>();
}

GameGrid::GameGrid(const HashQuadtree& data, Size2 size)
    : m_Width(size.Width), m_Height(size.Height), m_HashLifeData(data),
      m_Population(data.Population()), m_Algorithm(LifeAlgorithm::HashLife) {}

void GameGrid::SetAlgorithm(LifeAlgorithm algorithm) {
    if (algorithm == LifeAlgorithm::SparseLife && m_HashLifeData) {
        m_Data = *m_HashLifeData | std::ranges::to<LifeHashSet>();
        m_HashLifeData.reset();
    }

    m_Algorithm = algorithm;
}

bool GameGrid::Dead() const { return m_Data.empty(); }

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

    if (m_HashLifeData) {
        return findBox(*m_HashLifeData);
    }
    return findBox(m_Data);
}

std::span<Vec2> GameGrid::SortedData() const {
    ValidateSortedCache();
    return std::span{m_SortedData};
}

const LifeHashSet& GameGrid::Data() const {
    ValidateUnsortedCache();
    return m_Data;
}

std::variant<std::reference_wrapper<const LifeHashSet>,
             std::reference_wrapper<const HashQuadtree>>
GameGrid::IterableData() const {
    if (m_Algorithm == LifeAlgorithm::HashLife && m_HashLifeData) {
        return std::ref(*m_HashLifeData);
    }
    return std::ref(m_Data);
}

BigInt GameGrid::Update(const BigInt& numSteps, std::stop_token stopToken) {
    m_SortedCacheInvalidated = true;

    switch (m_Algorithm) {
    case LifeAlgorithm::SparseLife:
        for (auto i = 0; i < numSteps; i++) {
            if (stopToken.stop_requested()) {
                break;
            }
            m_Data = SparseLife(m_Data, {0, 0, m_Width, m_Height}, stopToken);
            m_Generation++;
        }
        if (m_Data.size() != m_Population) {
            m_Population = BigInt{m_Data.size()};
        }
        return numSteps;
    case LifeAlgorithm::HashLife:
        if (!m_HashLifeData) {
            m_HashLifeData = HashQuadtree{m_Data, {0, 0}};
            m_Data.clear();
            m_SortedData.clear();
        }
        m_UnsortedCacheInvalidated = true;
        const auto generations = HashLife(*m_HashLifeData, numSteps, stopToken);
        m_Generation += generations;
        m_Population = m_HashLifeData->Population();
        return generations;
    }

    return 0;
}

bool GameGrid::Toggle(int32_t x, int32_t y) {
    if (!InBounds(x, y)) {
        return false;
    }

    return Set(x, y, m_Data.contains({x, y}));
}

bool GameGrid::Set(int32_t x, int32_t y, bool active) {
    if (!InBounds(x, y)) {
        return false;
    }
    m_SortedCacheInvalidated = true;

    if (m_HashLifeData) {
        m_UnsortedCacheInvalidated = true;
        m_HashLifeData->Set({x, y}, active);
    } else {
        auto itr = m_Data.find({x, y});
        if (itr == m_Data.end() && active) {
            m_Population++;
            m_Data.insert({x, y});
        } else if (itr != m_Data.end() && !active) {
            m_Population--;
            m_Data.erase(itr);
        }
    }

    m_Generation = 0;
    return true;
}

GameGrid GameGrid::SubRegion(Rect region) const {
    if (m_HashLifeData) {
        GameGrid result{m_HashLifeData->Extract(region), region.Size()};
        return result;
    }

    GameGrid grid{region.Width, region.Height};
    for (const auto pos : m_Data) {
        if (!region.InBounds(pos)) {
            continue;
        }
        grid.m_Population++;
        grid.m_Data.insert(pos - region.Pos());
    }
    return grid;
}

// TODO: Unsafe if running hash life & cache is invalidated
LifeHashSet GameGrid::ReadRegion(Rect region) const {
    if (m_HashLifeData) {
        LifeHashSet result{};
        for (const auto pos : std::ranges::subrange(
                 m_HashLifeData->begin(region), m_HashLifeData->end())) {
            result.insert(pos);
        }
        return result;
    }
    LifeHashSet result{};
    for (const auto pos : m_Data) {
        if (region.InBounds(pos))
            result.insert(pos);
    }
    return result;
}

void GameGrid::ClearRegion(Rect region) {
    if (m_HashLifeData) {
        m_UnsortedCacheInvalidated = true;
        m_HashLifeData->Clear(region);
        m_Population = m_HashLifeData->Population();
        return;
    }

    m_Population -= std::erase_if(
        m_Data, [region](Vec2 pos) { return region.InBounds(pos); });
    m_SortedCacheInvalidated = true;
}

LifeHashSet GameGrid::InsertGrid(const GameGrid& region, Vec2 pos) {
    if (m_HashLifeData && region.m_HashLifeData) {
        // Merge directly at the node level, no materialization needed
        // For now, iterate the source quadtree and Set each cell
        LifeHashSet inserted{};
        for (const auto cell : *region.m_HashLifeData) {
            const Vec2 offsetPos{pos.X + cell.X, pos.Y + cell.Y};
            if (!m_HashLifeData->Get(offsetPos)) {
                m_HashLifeData->Set(offsetPos, true);
                inserted.insert(offsetPos);
                m_Population++;
            }
        }
        return inserted;
    }

    ValidateUnsortedCache(); // HashQuadtree does not support insertion
    m_HashLifeData.reset();

    LifeHashSet result{};
    for (const auto cell : region.m_Data) {
        const Vec2 offsetPos{pos.X + cell.X, pos.Y + cell.Y};
        if (m_Data.contains(offsetPos)) {
            continue;
        }
        m_Data.insert(offsetPos);
        result.insert(offsetPos);
        m_Population++;
    }
    return result;
}

void GameGrid::RotateGrid(bool clockwise) {
    // Probably more easily read as ((width - 1) / 2, (height - 1) / 2)
    const Vec2F center{((m_Width / 2.F) - 0.5F), ((m_Height / 2.F) - 0.5F)};
    LifeHashSet newSet{};

    for (const auto cellPos : m_Data) {
        const auto offset = Vec2F{static_cast<float>(cellPos.X),
                                  static_cast<float>(cellPos.Y)} -
                            center;
        const auto rotated =
            clockwise ? Vec2F{-offset.Y, offset.X} : Vec2F{offset.Y, -offset.X};
        const auto result = rotated + Vec2F{center.Y, center.X};
        newSet.insert(Vec2{static_cast<int32_t>(result.X),
                           static_cast<int32_t>(result.Y)});
    }
    std::swap(m_Width, m_Height);
    m_SortedCacheInvalidated = true;

    if (m_HashLifeData) {
        m_UnsortedCacheInvalidated = true;
        m_HashLifeData = HashQuadtree{std::move(newSet)};
    } else {
        m_Data = std::move(newSet);
    }
}

void GameGrid::FlipGrid(bool vertical) {
    LifeHashSet newData{};
    if (!Bounded()) {
        for (const auto pos : m_Data) {
            if (vertical) {
                newData.insert({pos.X, -pos.Y});
            } else {
                newData.insert({-pos.X, pos.Y});
            }
        }
    } else {
        for (const auto pos : m_Data) {
            if (vertical) {
                newData.insert({pos.X, m_Height - 1 - pos.Y});
            } else {
                newData.insert({m_Width - 1 - pos.X, pos.Y});
            }
        }
    }
    m_SortedCacheInvalidated = true;

    if (m_HashLifeData) {
        m_HashLifeData = HashQuadtree{std::move(newData)};
        m_UnsortedCacheInvalidated = true;
    } else {
        m_Data = std::move(newData);
    }
}

std::optional<bool> GameGrid::Get(int32_t x, int32_t y) const {
    return Get({x, y});
}

std::optional<bool> GameGrid::Get(Vec2 pos) const {
    if (!InBounds(pos)) {
        return std::nullopt;
    }
    if (m_HashLifeData) {
        return m_HashLifeData->Get(pos);
    }
    return m_Data.contains(pos);
}

void GameGrid::ValidateUnsortedCache() const {
    if (m_HashLifeData) {
        m_Data = *m_HashLifeData | std::ranges::to<LifeHashSet>();
    }
    m_UnsortedCacheInvalidated = false;
}

void GameGrid::ValidateSortedCache() const {
    if (m_HashLifeData) {
        m_SortedData = *m_HashLifeData | std::ranges::to<std::vector<Vec2>>();
    } else {
        m_SortedData = m_Data | std::ranges::to<std::vector<Vec2>>();
    }
    m_SortedCacheInvalidated = false;
    std::ranges::sort(m_SortedData, RowMajorEqual{});
}
} // namespace gol
