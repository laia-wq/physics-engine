#include "physics/CircleBody.hpp"

#include <cmath>

namespace physics
{
CircleBody::CircleBody(
    float bodyRadius,
    sf::Vector2f startPosition,
    sf::Vector2f startVelocity,
    sf::Vector2f bodyAcceleration,
    float bodyRestitution
)
    : position(startPosition),
      velocity(startVelocity),
      acceleration(bodyAcceleration),
      radius(bodyRadius),
      inverseMass(bodyRadius > 0.f ? 1.f / (bodyRadius * bodyRadius) : 0.f),
      restitution(bodyRestitution)
{
}

void CircleBody::integrate(float timeStep)
{
    velocity += acceleration * timeStep;
    position += velocity * timeStep;
}

void CircleBody::resolveBounds(
    float width,
    float height,
    float floorFriction,
    float minimumBounceSpeed
)
{
    if (position.x < 0.f)
    {
        position.x = 0.f;
        velocity.x = -velocity.x * restitution;
    }

    if (position.x + radius * 2.f > width)
    {
        position.x = width - radius * 2.f;
        velocity.x = -velocity.x * restitution;
    }

    if (position.y < 0.f)
    {
        position.y = 0.f;
        velocity.y = -velocity.y * restitution;
    }

    if (position.y + radius * 2.f > height)
    {
        position.y = height - radius * 2.f;
        velocity.y = -velocity.y * restitution;
        velocity.x *= floorFriction;

        if (std::abs(velocity.y) < minimumBounceSpeed)
        {
            velocity.y = 0.f;
        }
    }
}

sf::Vector2f CircleBody::center() const
{
    return position + sf::Vector2f(radius, radius);
}
}
