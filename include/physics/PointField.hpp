#pragma once

#include "physics/CircleBody.hpp"

#include <vector>

namespace physics
{
struct PointField
{
    sf::Vector2f position;
    float radialStrength;
    float vortexStrength;
    bool enabled;
    float oscillationAmount = 0.f;
    float oscillationFrequency = 1.f;
    float remainingLifetime = -1.f;
    bool chargeSensitive = false;
};

sf::Vector2f calculatePointFieldAcceleration(
    const CircleBody& body,
    const std::vector<PointField>& fields,
    float simulationTime,
    float softening,
    float ordinaryResponse,
    float materialResponse
);
}
