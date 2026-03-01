#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "GameGrid.h"
#include "Graphics2D.h"
#include "LifeAlgorithm.h"
#include <HashQuadtree.h>
#include <LifeHashSet.h>
#include <functional>
#include <iterator>
#include <ranges>
#include <stop_token>
#include <type_traits>
#include <unordered_dense.h>
#include <variant>

namespace gol {
GameGrid GameGrid::GenerateNoise(Rect bounds, float density) {
    static std::random_device random{};
    static std::mt19937 generator{random()};

    // Since our density is not totally accurate because it can produce overlapping
    // cells, we create the illusion that it is fully accurate when density == 1.
    if (density == 1.f) {
        GameGrid ret{bounds.Width, bounds.Height};
        for (auto x = 0; x < bounds.Width; x++)
            for (auto y = 0; y < bounds.Height; y++)
                ret.m_Data.insert({x, y});
        ret.m_Population = ret.m_Data.size();
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

    ret.m_Population = ret.m_Data.size();
    return ret;
}

GameGrid::GameGrid(int32_t width, int32_t height)
    : m_Algorithm(LifeAlgorithm::HashLife), m_Data(), m_Width(width),
      m_Height(height) {}

GameGrid::GameGrid(Size2 size) : GameGrid(size.Width, size.Height) {}

GameGrid::GameGrid(const GameGrid &other, Size2 size) : GameGrid(size) {
    m_Population = other.m_Population;
    m_Data = other.Data() |
             std::views::filter([this](Vec2 pos) { return InBounds(pos); }) |
             std::ranges::to<LifeHashSet>();
}

void GameGrid::SetAlgorithm(LifeAlgorithm algorithm) {
    if (algorithm == LifeAlgorithm::SparseLife && m_HashLifeData) {
        m_Data = *m_HashLifeData | std::ranges::to<LifeHashSet>();
        m_HashLifeData.reset();
    }

    m_Algorithm = algorithm;
}

void GameGrid::PrepareCopyBetweenThreads() {
    if (m_HashLifeData) {
        m_HashLifeData->PrepareCopyBetweenThreads();
    }
}

bool GameGrid::Dead() const { return m_Data.size() == 0; }

Rect GameGrid::BoundingBox() const {
    if (Bounded()) {
        return {0, 0, m_Width, m_Height};
    }

    const auto findBox = [](std::ranges::input_range auto &&data) {
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

    if (m_CacheInvalidated && m_HashLifeData) {
        return findBox(*m_HashLifeData);
    } else {
        return findBox(m_Data);
    }
}

const std::set<Vec2> &GameGrid::SortedData() const {
    if (m_CacheInvalidated)
        ValidateCache(true);
    return m_SortedData;
}

const LifeHashSet &GameGrid::Data() const {
    if (m_CacheInvalidated && m_HashLifeData)
        ValidateCache(false);
    m_CacheInvalidated = false;
    return m_Data;
}

std::variant<std::reference_wrapper<const LifeHashSet>,
             std::reference_wrapper<const HashQuadtree>>
GameGrid::IterableData() const {
    if (m_Algorithm == LifeAlgorithm::HashLife && m_HashLifeData)
        return std::ref(*m_HashLifeData);
    else
        return std::ref(m_Data);
}

int64_t GameGrid::Update(int64_t numSteps, std::stop_token stopToken) {
    m_CacheInvalidated = true;

    switch (m_Algorithm) {
    case LifeAlgorithm::SparseLife:
        for (auto i = 0; i < numSteps; i++) {
            if (stopToken.stop_requested())
                break;
            m_Data = SparseLife(m_Data, {0, 0, m_Width, m_Height}, stopToken);
            m_Generation++;
        }
        m_Population = m_Data.size();
        return numSteps;
    case LifeAlgorithm::HashLife:
        if (!m_HashLifeData) {
            m_HashLifeData = HashQuadtree{m_Data, {0, 0}};
            m_Data.clear();
            m_SortedData.clear();
        }
        const auto generations = HashLife(*m_HashLifeData, numSteps, stopToken);

        if (std::numeric_limits<int64_t>::max() - generations < m_Generation)
            m_Generation = std::numeric_limits<int64_t>::max();
        else
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

    return Set(x, y, m_Data.find({x, y}) != m_Data.end());
}

bool GameGrid::Set(int32_t x, int32_t y, bool active) {
    if (!InBounds(x, y)) {
        return false;
    }

    auto itr = m_Data.find({x, y});
    if (itr == m_Data.end() && active) {
        m_Population++;
        m_Data.insert({x, y});
        m_SortedData.insert({x, y});
    } else if (itr != m_Data.end() && !active) {
        m_Population--;
        m_Data.erase(itr);
        m_SortedData.erase({x, y});
    }

    m_Generation = 0;
    return true;
}

void GameGrid::TranslateRegion(Rect region, Vec2 translation) {
    std::vector<Vec2> newCells{};
    auto it = m_Data.begin();
    while (it != m_Data.end()) {
        if (region.InBounds(*it)) {
            newCells.push_back(*it + translation);
            m_Data.erase(it++);
            continue;
        }
        ++it;
    }
    for (const auto pos : newCells) {
        m_Data.insert(pos);
    }
}

GameGrid GameGrid::SubRegion(Rect region) const {
    // TODO: Clunky lambda, abstract into static helper function
    const auto fillGrid =
        // We only have to check bounds when copying from m_Data, so we can control
        // at compile time whether to check or not
        [region]<bool CheckBounds>(GameGrid &grid,
                                   std::ranges::input_range auto &&range) {
            for (const auto pos : range) {
                if constexpr (CheckBounds) {
                    if (!region.InBounds(pos))
                        continue;
                }
                grid.m_Population++;
                grid.m_Data.insert(pos - region.Pos());
            }
            return grid;
        };

    if (m_CacheInvalidated && m_HashLifeData) {
        GameGrid result{region.Width, region.Height};
        // Because we're using the bounded iterator here, we can disable bounds check
        return fillGrid.operator()<false>(
            result, std::ranges::subrange(m_HashLifeData->begin(region),
                                          m_HashLifeData->end()));
    }
    auto result = GameGrid{region.Width, region.Height};
    return fillGrid.operator()<true>(result, m_Data);
}

// TODO: Unsafe if running hash life & cache is invalidated
LifeHashSet GameGrid::ReadRegion(Rect region) const {
    LifeHashSet result{};
    for (const auto pos : m_Data) {
        if (region.InBounds(pos))
            result.insert(pos);
    }
    return result;
}

void GameGrid::ClearRegion(Rect region) {
    m_Population -= std::erase_if(
        m_Data, [region](Vec2 pos) { return region.InBounds(pos); });
    m_CacheInvalidated = true;
}

void GameGrid::ClearData(const std::vector<Vec2> &data, Vec2 offset) {
    for (const auto vec : data) {
        m_Population -= m_Data.erase({vec.X + offset.X, vec.Y + offset.Y});
    }
    m_CacheInvalidated = true;
}

LifeHashSet GameGrid::InsertGrid(const GameGrid &region, Vec2 pos) {
    ValidateCache(true); // HashQuadtree does not support insertion
    m_HashLifeData.reset();
    
    LifeHashSet result{};
    for (const auto cell : region.m_Data) {
        const Vec2 offsetPos{pos.X + cell.X, pos.Y + cell.Y};
        if (m_Data.find(offsetPos) != m_Data.end())
            continue;
        m_Data.insert(offsetPos);
        m_SortedData.insert(offsetPos);
        result.insert(offsetPos);
        m_Population++;
    }
    return result;
}

void GameGrid::RotateGrid(bool clockwise) {
    // Probably more easily read as ((width - 1) / 2, (height - 1) / 2)
    const Vec2F center{static_cast<float>(m_Width / 2.f - 0.5f),
                        static_cast<float>(m_Height / 2.f - 0.5f)};
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
    m_CacheInvalidated = true;
    m_Data = std::move(newSet);
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
    m_CacheInvalidated = true;
    m_Data = std::move(newData);
}

std::optional<bool> GameGrid::Get(int32_t x, int32_t y) const {
    return Get({x, y});
}

std::optional<bool> GameGrid::Get(Vec2 pos) const {
    if (!InBounds(pos))
        return std::nullopt;
    return m_Data.find(pos) != m_Data.end();
}

void GameGrid::ValidateCache(bool validateSorted) const {
    if (m_HashLifeData)
        m_Data = *m_HashLifeData | std::ranges::to<LifeHashSet>();
    if (validateSorted)
        m_SortedData = m_Data | std::ranges::to<std::set<Vec2>>();
}
} // namespace gol
