#include "physics/Collision.hpp"

#include <cmath>
#include <iostream>
#include <string_view>

namespace
{
int failures = 0;

bool nearlyEqual(float first, float second, float tolerance = 0.001f)
{
    return std::abs(first - second) <= tolerance;
}

void expect(bool condition, std::string_view testName)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << testName << '\n';
        ++failures;
    }
}
}

int main()
{
    expect(
        !physics::circlesOverlap({0.f, 0.f}, 1.f, {3.f, 0.f}, 1.f),
        "separated circles do not overlap"
    );
    expect(
        physics::circlesOverlap({0.f, 0.f}, 1.f, {2.f, 0.f}, 1.f),
        "touching circles count as a collision"
    );
    expect(
        physics::circlesOverlap({0.f, 0.f}, 2.f, {2.f, 0.f}, 1.f),
        "intersecting circles overlap"
    );
    expect(
        physics::circlesOverlap({4.f, -2.f}, 1.f, {4.f, -2.f}, 1.f),
        "circles with the same centre overlap"
    );
    expect(
        !physics::circlesOverlap({0.f, 0.f}, 1.f, {1.5f, 1.5f}, 1.f),
        "diagonally separated circles do not overlap"
    );

    {
        sf::Vector2f positionA(0.f, 0.f);
        sf::Vector2f positionB(1.5f, 0.f);
        sf::Vector2f velocityA(1.f, 0.f);
        sf::Vector2f velocityB(-1.f, 0.f);

        const bool collided = physics::resolveCircleCollision(
            positionA, velocityA, 1.f, 1.f, 1.f,
            positionB, velocityB, 1.f, 1.f, 1.f
        );

        expect(collided, "overlapping circles are resolved");
        expect(nearlyEqual(positionB.x - positionA.x, 2.f),
               "penetration correction separates equal circles");
        expect(nearlyEqual(velocityA.x, -1.f) && nearlyEqual(velocityB.x, 1.f),
               "equal elastic circles exchange head-on velocities");
    }

    {
        sf::Vector2f positionA(0.f, 0.f);
        sf::Vector2f positionB(2.f, 0.f);
        sf::Vector2f velocityA(-1.f, 0.f);
        sf::Vector2f velocityB(1.f, 0.f);

        physics::resolveCircleCollision(
            positionA, velocityA, 1.f, 1.f, 0.5f,
            positionB, velocityB, 1.f, 1.f, 0.5f
        );

        expect(nearlyEqual(velocityA.x, -1.f) && nearlyEqual(velocityB.x, 1.f),
               "separating circles receive no extra impulse");
    }

    {
        sf::Vector2f positionA(0.f, 0.f);
        sf::Vector2f positionB(4.f, 0.f);
        sf::Vector2f velocityA(1.f, 0.f);
        sf::Vector2f velocityB(-1.f, 0.f);

        expect(!physics::resolveCircleCollision(
                   positionA, velocityA, 1.f, 1.f, 1.f,
                   positionB, velocityB, 1.f, 1.f, 1.f
               ),
               "separated circles are not resolved");
    }

    if (failures == 0)
    {
        std::cout << "All collision tests passed.\n";
    }

    return failures == 0 ? 0 : 1;
}
