#include "presets/PresetConstruction.hpp"

#include <cmath>

namespace presets
{
std::vector<BodyDefinition> buildFoundationPreset(
    FoundationPreset preset,
    std::size_t stressBodyCount
)
{
    std::vector<BodyDefinition> bodies;
    if (preset == FoundationPreset::Classic)
    {
        return {
            {5.f, {100.f, 100.f}, {180.f, 0.f}, 0.75f},
            {5.f, {300.f, 80.f}, {-100.f, 0.f}, 0.55f},
            {5.f, {500.f, 50.f}, {70.f, 0.f}, 0.9f}
        };
    }
    if (preset == FoundationPreset::Stress)
    {
        bodies.reserve(stressBodyCount);
        constexpr float RADIUS = 5.f;
        constexpr float SPACING = 15.f;
        constexpr std::size_t COLUMNS = 50;
        for (std::size_t index = 0; index < stressBodyCount; ++index)
        {
            bodies.push_back({
                RADIUS,
                {20.f + static_cast<float>(index % COLUMNS) * SPACING,
                 40.f + static_cast<float>(index / COLUMNS) * SPACING},
                {index % 2 == 0 ? 35.f : -35.f, 0.f},
                0.65f
            });
        }
        return bodies;
    }
    if (preset == FoundationPreset::HeadOn)
    {
        return {
            {30.f, {180.f, 270.f}, {220.f, 0.f}, 1.f},
            {30.f, {560.f, 270.f}, {-220.f, 0.f}, 1.f}
        };
    }
    if (preset == FoundationPreset::ZeroGravity)
    {
        bodies.reserve(16);
        for (std::size_t index = 0; index < 16; ++index)
        {
            bodies.push_back({
                14.f + static_cast<float>(index % 3) * 4.f,
                {80.f + static_cast<float>(index % 4) * 180.f,
                 70.f + static_cast<float>(index / 4) * 130.f},
                {index % 2 == 0 ? 90.f : -90.f,
                 index % 3 == 0 ? 70.f : -50.f},
                0.95f
            });
        }
        return bodies;
    }
    if (preset == FoundationPreset::Rain)
    {
        bodies.reserve(60);
        for (std::size_t index = 0; index < 60; ++index)
        {
            bodies.push_back({
                7.f,
                {15.f + static_cast<float>(index % 20) * 39.f,
                 20.f + static_cast<float>(index / 20) * 28.f},
                {0.f, 0.f},
                0.7f
            });
        }
        return bodies;
    }

    constexpr float PI = 3.14159265359f;
    constexpr float FIELD_STRENGTH = 8000000.f;
    const sf::Vector2f fieldCenter(400.f, 300.f);
    bodies.reserve(120);
    for (std::size_t index = 0; index < 120; ++index)
    {
        const float distance = 75.f + static_cast<float>(index % 6) * 42.f;
        const float angle = 2.f * PI * static_cast<float>(index) / 120.f;
        const sf::Vector2f radial(std::cos(angle), std::sin(angle));
        const float radius = 4.f + static_cast<float>(index % 4);
        const sf::Vector2f center = fieldCenter + radial * distance;
        const sf::Vector2f tangent(-radial.y, radial.x);
        bodies.push_back({
            radius,
            center - sf::Vector2f(radius, radius),
            tangent * std::sqrt(FIELD_STRENGTH / distance),
            0.9f
        });
    }
    return bodies;
}
}
