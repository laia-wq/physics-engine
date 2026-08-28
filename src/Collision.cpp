#include "physics/Collision.hpp"

#include <algorithm>
#include <cmath>

namespace physics
{
namespace
{
constexpr float MINIMUM_DISTANCE = 0.0001f;

float dot(sf::Vector2f first, sf::Vector2f second)
{
    return first.x * second.x + first.y * second.y;
}
}

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

bool resolveCircleCollision(
    sf::Vector2f& positionA,
    sf::Vector2f& velocityA,
    float radiusA,
    float inverseMassA,
    float restitutionA,
    sf::Vector2f& positionB,
    sf::Vector2f& velocityB,
    float radiusB,
    float inverseMassB,
    float restitutionB
)
{
    const sf::Vector2f centerA = positionA + sf::Vector2f(radiusA, radiusA);
    const sf::Vector2f centerB = positionB + sf::Vector2f(radiusB, radiusB);
    const sf::Vector2f difference = centerB - centerA;
    const float distanceSquared = dot(difference, difference);
    const float combinedRadius = radiusA + radiusB;

    if (distanceSquared > combinedRadius * combinedRadius)
    {
        return false;
    }

    const float distance = std::sqrt(distanceSquared);
    const sf::Vector2f normal = distance > MINIMUM_DISTANCE
        ? difference / distance
        : sf::Vector2f(1.f, 0.f);

    const float inverseMassSum = inverseMassA + inverseMassB;
    if (inverseMassSum <= 0.f)
    {
        return true;
    }

    const float penetration = combinedRadius - distance;
    const sf::Vector2f correction = normal * (penetration / inverseMassSum);
    positionA -= correction * inverseMassA;
    positionB += correction * inverseMassB;

    const sf::Vector2f relativeVelocity = velocityB - velocityA;
    const float velocityAlongNormal = dot(relativeVelocity, normal);

    if (velocityAlongNormal >= 0.f)
    {
        return true;
    }

    const float restitution = std::min(restitutionA, restitutionB);
    const float impulseMagnitude =
        -(1.f + restitution) * velocityAlongNormal / inverseMassSum;
    const sf::Vector2f impulse = normal * impulseMagnitude;

    velocityA -= impulse * inverseMassA;
    velocityB += impulse * inverseMassB;

    return true;
}
}
