#include "physics/DistanceConstraint.hpp"

#include <algorithm>
#include <cmath>

namespace physics
{
bool solveDistanceConstraint(
    CircleBody& first,
    CircleBody& second,
    float restLength,
    float stiffness
)
{
    const sf::Vector2f difference = second.center() - first.center();
    const float distanceSquared =
        difference.x * difference.x + difference.y * difference.y;
    const float inverseMassTotal = first.inverseMass + second.inverseMass;
    if (distanceSquared <= 0.00000001f || inverseMassTotal <= 0.f)
    {
        return false;
    }

    const float distance = std::sqrt(distanceSquared);
    const sf::Vector2f direction = difference / distance;
    const float clampedStiffness = std::clamp(stiffness, 0.f, 1.f);
    const sf::Vector2f correction = direction *
        ((distance - restLength) * clampedStiffness / inverseMassTotal);

    first.position += correction * first.inverseMass;
    second.position -= correction * second.inverseMass;
    return true;
}
}
