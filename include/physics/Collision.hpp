#pragma once

#include <SFML/System/Vector2.hpp>

namespace physics
{
bool circlesOverlap(
    sf::Vector2f centerA,
    float radiusA,
    sf::Vector2f centerB,
    float radiusB
);
}
