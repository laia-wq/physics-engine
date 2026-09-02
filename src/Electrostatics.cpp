#include "physics/Electrostatics.hpp"

#include <cmath>

namespace physics
{
bool applyElectrostaticPair(
    CircleBody& first,
    CircleBody& second,
    float strength,
    float softening
)
{
    if (first.charge == 0.f || second.charge == 0.f)
    {
        return false;
    }

    const sf::Vector2f difference = second.center() - first.center();
    const float softenedDistanceSquared =
        difference.x * difference.x + difference.y * difference.y +
        softening * softening;
    const sf::Vector2f direction =
        difference / std::sqrt(softenedDistanceSquared);
    const float forceMagnitude =
        -strength * first.charge * second.charge / softenedDistanceSquared;
    const sf::Vector2f forceOnFirst = direction * forceMagnitude;

    first.acceleration += forceOnFirst * first.inverseMass;
    second.acceleration -= forceOnFirst * second.inverseMass;
    return true;
}
}
