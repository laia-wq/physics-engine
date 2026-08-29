#include "physics/CircleBody.hpp"

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
    physics::CircleBody movingBody(
        2.f, {0.f, 0.f}, {3.f, 4.f}, {0.f, 2.f}, 0.5f
    );
    movingBody.integrate(0.5f);

    expect(nearlyEqual(movingBody.velocity.y, 5.f),
           "integration updates velocity from acceleration");
    expect(nearlyEqual(movingBody.position.x, 1.5f) &&
               nearlyEqual(movingBody.position.y, 2.5f),
           "integration advances position using updated velocity");
    expect(nearlyEqual(movingBody.center().x, 3.5f) &&
               nearlyEqual(movingBody.center().y, 4.5f),
           "centre accounts for the circle radius");

    physics::CircleBody forcedBody(
        2.f, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, 0.5f
    );
    forcedBody.applyForce({4.f, 0.f});
    forcedBody.applyForce({4.f, 0.f});
    forcedBody.integrate(0.5f);

    expect(nearlyEqual(forcedBody.velocity.x, 1.f),
           "multiple forces accumulate and account for body mass");
    expect(nearlyEqual(forcedBody.accumulatedForce.x, 0.f),
           "forces clear after each integration step");

    forcedBody.integrate(0.5f);
    expect(nearlyEqual(forcedBody.velocity.x, 1.f),
           "a cleared force is not applied again on the next step");

    physics::CircleBody wallBody(
        1.f, {9.f, 2.f}, {4.f, 0.f}, {0.f, 0.f}, 0.5f
    );
    wallBody.resolveBounds(10.f, 10.f, 0.9f, 1.f);

    expect(nearlyEqual(wallBody.position.x, 8.f),
           "right-wall penetration is corrected");
    expect(nearlyEqual(wallBody.velocity.x, -2.f),
           "right-wall velocity reflects with restitution");

    physics::CircleBody floorBody(
        1.f, {2.f, 9.f}, {10.f, 10.f}, {0.f, 0.f}, 0.5f
    );
    floorBody.resolveBounds(10.f, 10.f, 0.9f, 6.f);

    expect(nearlyEqual(floorBody.position.y, 8.f),
           "floor penetration is corrected");
    expect(nearlyEqual(floorBody.velocity.x, 9.f),
           "floor friction reduces horizontal velocity");
    expect(nearlyEqual(floorBody.velocity.y, 0.f),
           "small floor bounces come to rest");

    if (failures == 0)
    {
        std::cout << "All circle body tests passed.\n";
    }

    return failures == 0 ? 0 : 1;
}
