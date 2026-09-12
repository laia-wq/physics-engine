#include "physics/PointField.hpp"

#include <cmath>
#include <iostream>
#include <string_view>
#include <vector>

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

physics::CircleBody makeBody(float charge = 1.f)
{
    physics::CircleBody body(
        2.f, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, 0.5f
    );
    body.charge = charge;
    body.inverseMass = 0.25f;
    return body;
}
}

int main()
{
    const auto body = makeBody();
    const physics::PointField attractor{{12.f, 2.f}, 1000.f, 0.f, true};
    auto acceleration = physics::calculatePointFieldAcceleration(
        body, {attractor}, 0.f, 1.f, 1.f, 1.f
    );
    expect(acceleration.x > 0.f && std::abs(acceleration.y) < 0.001f,
           "positive radial strength attracts toward the field");

    const physics::PointField repulsor{{12.f, 2.f}, -1000.f, 0.f, true};
    acceleration = physics::calculatePointFieldAcceleration(
        body, {repulsor}, 0.f, 1.f, 1.f, 1.f
    );
    expect(acceleration.x < 0.f,
           "negative radial strength repels away from the field");

    const physics::PointField vortex{{12.f, 2.f}, 0.f, 1000.f, true};
    acceleration = physics::calculatePointFieldAcceleration(
        body, {vortex}, 0.f, 1.f, 1.f, 1.f
    );
    expect(acceleration.y > 0.f && std::abs(acceleration.x) < 0.001f,
           "vortex strength accelerates perpendicular to the radius");

    auto disabled = attractor;
    disabled.enabled = false;
    acceleration = physics::calculatePointFieldAcceleration(
        body, {disabled}, 0.f, 1.f, 1.f, 1.f
    );
    expect(acceleration == sf::Vector2f(0.f, 0.f),
           "disabled fields have no effect");

    auto oscillating = attractor;
    oscillating.oscillationAmount = 1.f;
    oscillating.oscillationFrequency = 1.f;
    acceleration = physics::calculatePointFieldAcceleration(
        body, {oscillating}, 0.25f, 1.f, 1.f, 1.f
    );
    const auto baseline = physics::calculatePointFieldAcceleration(
        body, {attractor}, 0.f, 1.f, 1.f, 1.f
    );
    expect(std::abs(acceleration.x - 2.f * baseline.x) < 0.01f,
           "oscillation scales field strength over time");

    auto chargedField = attractor;
    chargedField.chargeSensitive = true;
    const auto positive = physics::calculatePointFieldAcceleration(
        makeBody(1.f), {chargedField}, 0.f, 1.f, 1.f, 1.f
    );
    const auto negative = physics::calculatePointFieldAcceleration(
        makeBody(-1.f), {chargedField}, 0.f, 1.f, 1.f, 1.f
    );
    expect(positive.x > 0.f && negative.x < 0.f,
           "charge-sensitive fields reverse for opposite charges");

    if (failures == 0)
    {
        std::cout << "All point-field tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
