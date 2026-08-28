#pragma once

#include <SFML/System/Vector2.hpp>

namespace physics
{
struct CircleBody
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Vector2f acceleration;

    float radius;
    float inverseMass;
    float restitution;

    CircleBody(
        float radius,
        sf::Vector2f position,
        sf::Vector2f velocity,
        sf::Vector2f acceleration,
        float restitution
    );

    void integrate(float timeStep);
    void resolveBounds(
        float width,
        float height,
        float floorFriction,
        float minimumBounceSpeed
    );
    sf::Vector2f center() const;
};
}
