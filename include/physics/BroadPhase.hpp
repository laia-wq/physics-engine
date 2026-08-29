#pragma once

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace physics
{
struct CircleBounds
{
    sf::Vector2f position;
    float radius;
};

using BodyPair = std::pair<std::size_t, std::size_t>;

std::vector<BodyPair> buildUniformGridPairs(
    const std::vector<CircleBounds>& bodies,
    float cellSize
);
}
