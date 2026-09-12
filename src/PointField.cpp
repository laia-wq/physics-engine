#include "physics/PointField.hpp"

#include <cmath>

namespace physics
{
sf::Vector2f calculatePointFieldAcceleration(
    const CircleBody& body,
    const std::vector<PointField>& fields,
    float simulationTime,
    float softening,
    float ordinaryResponse,
    float materialResponse
)
{
    sf::Vector2f totalAcceleration(0.f, 0.f);
    for (const auto& field : fields)
    {
        if (!field.enabled)
        {
            continue;
        }

        const sf::Vector2f difference = field.position - body.center();
        const float softenedDistanceSquared =
            difference.x * difference.x + difference.y * difference.y +
            softening * softening;
        const sf::Vector2f direction =
            difference / std::sqrt(softenedDistanceSquared);
        const sf::Vector2f tangent(-direction.y, direction.x);
        constexpr float PI = 3.14159265359f;
        const float oscillation = 1.f + field.oscillationAmount *
            std::sin(2.f * PI * field.oscillationFrequency * simulationTime);
        const float response = (field.chargeSensitive
            ? body.charge * body.inverseMass * 400.f
            : ordinaryResponse) * materialResponse;

        totalAcceleration +=
            direction * (field.radialStrength * oscillation * response /
                softenedDistanceSquared) +
            tangent * (field.vortexStrength * oscillation * response /
                softenedDistanceSquared);
    }
    return totalAcceleration;
}
}
