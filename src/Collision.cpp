#include "physics/Collision.hpp"

namespace physics
{
bool circlesOverlap(
    sf::Vector2f centerA,
    float radiusA,
    sf::Vector2f centerB,
    float radiusB
)
{
    const sf::Vector2f difference = centerB - centerA;
    const float distanceSquared =
        difference.x * difference.x + difference.y * difference.y;
    const float combinedRadius = radiusA + radiusB;

    return distanceSquared <= combinedRadius * combinedRadius;
}
}
