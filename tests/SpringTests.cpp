#include "physics/Spring.hpp"

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

physics::CircleBody makeBody(float x, float velocityX = 0.f)
{
    return physics::CircleBody(
        1.f, {x, 0.f}, {velocityX, 0.f}, {0.f, 0.f}, 0.5f
    );
}
}

int main()
{
    auto stretchedA = makeBody(0.f);
    auto stretchedB = makeBody(12.f);
    expect(physics::applySpringForce(
               stretchedA, stretchedB, 8.f, 100.f, 0.f),
           "separated bodies define a spring direction");
    expect(stretchedA.acceleration.x > 0.f &&
               stretchedB.acceleration.x < 0.f,
           "a stretched spring pulls its endpoints together");

    auto compressedA = makeBody(0.f);
    auto compressedB = makeBody(4.f);
    physics::applySpringForce(compressedA, compressedB, 8.f, 100.f, 0.f);
    expect(compressedA.acceleration.x < 0.f &&
               compressedB.acceleration.x > 0.f,
           "a compressed spring pushes its endpoints apart");

    auto separatingA = makeBody(0.f, -5.f);
    auto separatingB = makeBody(8.f, 5.f);
    physics::applySpringForce(
        separatingA, separatingB, 8.f, 0.f, 20.f
    );
    expect(separatingA.acceleration.x > 0.f &&
               separatingB.acceleration.x < 0.f,
           "spring damping opposes endpoint separation");

    auto coincidentA = makeBody(0.f);
    auto coincidentB = makeBody(0.f);
    expect(!physics::applySpringForce(
               coincidentA, coincidentB, 8.f, 100.f, 20.f),
           "coincident endpoints avoid an undefined direction");

    if (failures == 0)
    {
        std::cout << "All spring tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
