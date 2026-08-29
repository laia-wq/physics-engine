#include "physics/BroadPhase.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace physics
{
namespace
{
std::uint64_t cellKey(int x, int y)
{
    return static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U |
        static_cast<std::uint32_t>(y);
}

std::uint64_t pairKey(std::size_t first, std::size_t second)
{
    return static_cast<std::uint64_t>(first) << 32U |
        static_cast<std::uint32_t>(second);
}
}

std::vector<BodyPair> buildUniformGridPairs(
    const std::vector<CircleBounds>& bodies,
    float cellSize
)
{
    std::vector<BodyPair> pairs;
    if (cellSize <= 0.f)
    {
        return pairs;
    }

    std::unordered_map<std::uint64_t, std::vector<std::size_t>> cells;

    for (std::size_t index = 0; index < bodies.size(); ++index)
    {
        const auto& body = bodies[index];
        const int minimumX =
            static_cast<int>(std::floor(body.position.x / cellSize));
        const int minimumY =
            static_cast<int>(std::floor(body.position.y / cellSize));
        const int maximumX = static_cast<int>(std::floor(
            (body.position.x + body.radius * 2.f) / cellSize
        ));
        const int maximumY = static_cast<int>(std::floor(
            (body.position.y + body.radius * 2.f) / cellSize
        ));

        for (int x = minimumX; x <= maximumX; ++x)
        {
            for (int y = minimumY; y <= maximumY; ++y)
            {
                cells[cellKey(x, y)].push_back(index);
            }
        }
    }

    std::unordered_set<std::uint64_t> uniquePairs;
    for (const auto& cell : cells)
    {
        const auto& indices = cell.second;
        for (std::size_t first = 0; first < indices.size(); ++first)
        {
            for (std::size_t second = first + 1; second < indices.size(); ++second)
            {
                const std::size_t lower = std::min(indices[first], indices[second]);
                const std::size_t upper = std::max(indices[first], indices[second]);
                if (uniquePairs.insert(pairKey(lower, upper)).second)
                {
                    pairs.emplace_back(lower, upper);
                }
            }
        }
    }

    return pairs;
}
}
