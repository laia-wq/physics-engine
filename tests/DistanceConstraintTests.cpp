#include "physics/DistanceConstraint.hpp"

#include <cmath>
#include <iostream>
#include <string_view>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view name)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << name << '\n';
        ++failures;
    }
}

physics::CircleBody makeBody(float x)
{
    return physics::CircleBody(
        1.f, {x, 0.f}, {0.f, 0.f}, {0.f, 0.f}, 0.5f
    );
}

float centerDistance(
    const physics::CircleBody& first,
    const physics::CircleBody& second
)
{
    const sf::Vector2f difference = second.center() - first.center();
    return std::sqrt(
        difference.x * difference.x + difference.y * difference.y
    );
}
}

int main()
{
    auto stretchedA = makeBody(0.f);
    auto stretchedB = makeBody(12.f);
    expect(physics::solveDistanceConstraint(
               stretchedA, stretchedB, 8.f, 1.f),
           "a valid movable pair can be solved");
    expect(std::abs(centerDistance(stretchedA, stretchedB) - 8.f) < 0.001f,
           "full stiffness restores the requested distance");

    auto pinned = makeBody(0.f);
    pinned.inverseMass = 0.f;
    auto moving = makeBody(12.f);
    physics::solveDistanceConstraint(pinned, moving, 8.f, 1.f);
    expect(std::abs(pinned.position.x) < 0.001f,
           "a pinned endpoint does not move");
    expect(std::abs(centerDistance(pinned, moving) - 8.f) < 0.001f,
           "the movable endpoint receives the entire correction");

    auto softA = makeBody(0.f);
    auto softB = makeBody(12.f);
    physics::solveDistanceConstraint(softA, softB, 8.f, 0.5f);
    expect(std::abs(centerDistance(softA, softB) - 10.f) < 0.001f,
           "partial stiffness applies a proportional correction");

    auto coincidentA = makeBody(0.f);
    auto coincidentB = makeBody(0.f);
    expect(!physics::solveDistanceConstraint(
               coincidentA, coincidentB, 8.f, 1.f),
           "coincident endpoints avoid an undefined direction");

    if (failures == 0)
    {
        std::cout << "All distance constraint tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
