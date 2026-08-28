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
);
}
