#include "physics/Spring.hpp"

#include <cmath>

namespace physics
{
bool applySpringForce(
    CircleBody& first,
    CircleBody& second,
    float restLength,
    float stiffness,
    float damping
)
{
    const sf::Vector2f difference = second.center() - first.center();
    const float distanceSquared =
        difference.x * difference.x + difference.y * difference.y;
    if (distanceSquared <= 0.00000001f)
    {
        return false;
    }

    const float distance = std::sqrt(distanceSquared);
    const sf::Vector2f direction = difference / distance;
    const sf::Vector2f relativeVelocity = second.velocity - first.velocity;
    const float velocityAlongSpring =
        relativeVelocity.x * direction.x + relativeVelocity.y * direction.y;
    const float forceMagnitude =
        stiffness * (distance - restLength) + damping * velocityAlongSpring;
    const sf::Vector2f forceOnFirst = direction * forceMagnitude;

    first.acceleration += forceOnFirst * first.inverseMass;
    second.acceleration -= forceOnFirst * second.inverseMass;
    return true;
}
}
