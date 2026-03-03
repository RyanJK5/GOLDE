#include <algorithm>

#include "Graphics2D.hpp"
#include "LifeAlgorithm.hpp"
#include "LifeHashSet.hpp"

namespace gol {
LifeHashSet SparseLife(std::span<const Vec2> data, Rect bounds,
                       std::stop_token stopToken) {
    // This algorithm is a significant improvement over a "naive" Game of Life
    // algorithm. We loop once and count how many neighbors each cell has
    // through a hash set. Then, we apply the Game of Life rules to determine if
    // that cell should be inserted into the result set.

    constexpr static std::array dx{-1, -1, -1, 0, 0, 1, 1, 1};
    constexpr static std::array dy{-1, 0, 1, -1, 1, -1, 0, 1};

    ankerl::unordered_dense::map<Vec2, uint8_t> neighborCount;
    neighborCount.reserve(data.size() * 8);
    for (const auto pos : data) {
        if (stopToken.stop_requested())
            return {};

        for (auto i = 0; i < 8; ++i) {
            const auto x = pos.X + dx[i];
            const auto y = pos.Y + dy[i];

            if (bounds.Width > 0 && bounds.Height > 0 && !bounds.InBounds(x, y))
                continue;

            ++neighborCount[{x, y}];
        }
    }

    LifeHashSet newSet{};
    newSet.reserve(neighborCount.size());
    const LifeHashSet current{data.begin(), data.end()};
    for (auto [pos, neighbors] : neighborCount) {
        if (stopToken.stop_requested()) // The caller is expected to use their
                                        // old data
            return {};

        if (neighbors == 3 || (neighbors == 2 && current.contains(pos)))
            newSet.insert(pos);
    }

    return newSet;
}

} // namespace gol