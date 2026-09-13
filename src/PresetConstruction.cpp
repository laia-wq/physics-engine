#include "presets/PresetConstruction.hpp"

#include <cmath>

namespace presets
{
namespace
{
float distanceBetween(const BodyDefinition& first, const BodyDefinition& second)
{
    const sf::Vector2f firstCenter = first.position +
        sf::Vector2f(first.radius, first.radius);
    const sf::Vector2f secondCenter = second.position +
        sf::Vector2f(second.radius, second.radius);
    const sf::Vector2f difference = secondCenter - firstCenter;
    return std::sqrt(difference.x * difference.x + difference.y * difference.y);
}

void connect(
    ConnectedPreset& preset,
    std::size_t first,
    std::size_t second,
    float stiffness,
    float damping
)
{
    preset.connections.push_back({
        first,
        second,
        distanceBetween(preset.bodies[first], preset.bodies[second]),
        stiffness,
        damping
    });
}
}

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

ConnectedPreset buildSpringChain(
    std::size_t bodyCount,
    float spacing,
    float width,
    float stiffness,
    float damping
)
{
    ConnectedPreset preset;
    preset.bodies.reserve(bodyCount);
    preset.connections.reserve(bodyCount > 0 ? bodyCount - 1 : 0);
    constexpr float RADIUS = 5.f;
    const std::size_t columns = std::max(
        static_cast<std::size_t>((width - 80.f) / spacing),
        static_cast<std::size_t>(2)
    );
    const std::size_t rowCount = (bodyCount + columns - 1) / columns;
    const float rowSpacing = rowCount > 1
        ? std::min(spacing, 480.f / static_cast<float>(rowCount - 1))
        : spacing;
    for (std::size_t index = 0; index < bodyCount; ++index)
    {
        const std::size_t row = index / columns;
        const std::size_t columnInRow = index % columns;
        const std::size_t column = row % 2 == 0
            ? columnInRow
            : columns - 1 - columnInRow;
        preset.bodies.push_back({
            RADIUS,
            {35.f + static_cast<float>(column) * spacing,
             55.f + static_cast<float>(row) * rowSpacing},
            {0.f, 0.f},
            0.5f
        });
        if (index > 0)
        {
            connect(preset, index - 1, index, stiffness, damping);
        }
    }
    if (bodyCount > 0)
    {
        preset.pinnedBodies.push_back(0);
    }
    return preset;
}

ConnectedPreset buildSoftBodyLattice(
    std::size_t columns,
    std::size_t rows,
    float requestedSpacing,
    float width,
    float height
)
{
    ConnectedPreset preset;
    if (columns < 2 || rows < 2)
    {
        return preset;
    }
    constexpr float RADIUS = 5.f;
    const float spacing = std::min({
        requestedSpacing,
        (width - 20.f) / static_cast<float>(columns - 1),
        (height - 20.f) / static_cast<float>(rows - 1)
    });
    const float startX =
        (width - static_cast<float>(columns - 1) * spacing) * 0.5f - RADIUS;
    const float startY =
        (height - static_cast<float>(rows - 1) * spacing) * 0.5f - RADIUS;
    preset.bodies.reserve(columns * rows);
    for (std::size_t row = 0; row < rows; ++row)
    {
        for (std::size_t column = 0; column < columns; ++column)
        {
            preset.bodies.push_back({
                RADIUS,
                {startX + static_cast<float>(column) * spacing,
                 startY + static_cast<float>(row) * spacing},
                {0.f, 0.f},
                0.35f
            });
        }
    }
    for (std::size_t row = 0; row < rows; ++row)
    {
        for (std::size_t column = 0; column < columns; ++column)
        {
            const std::size_t index = row * columns + column;
            if (column + 1 < columns)
            {
                connect(preset, index, index + 1, 6500.f, 260.f);
            }
            if (row + 1 < rows)
            {
                connect(preset, index, index + columns, 6500.f, 260.f);
            }
            if (row + 1 < rows && column + 1 < columns)
            {
                connect(preset, index, index + columns + 1, 6500.f, 260.f);
            }
            if (row + 1 < rows && column > 0)
            {
                connect(preset, index, index + columns - 1, 6500.f, 260.f);
            }
        }
    }
    preset.pinnedBodies = {0, columns - 1};
    return preset;
}

ConnectedPreset buildRadialWeb(
    std::size_t ringCount,
    std::size_t spokeCount
)
{
    ConnectedPreset preset;
    if (ringCount == 0 || spokeCount < 3)
    {
        return preset;
    }
    constexpr float PI = 3.14159265359f;
    const sf::Vector2f center(400.f, 270.f);
    const float ringSpacing = 220.f / static_cast<float>(ringCount);
    preset.bodies.push_back({
        6.f, center - sf::Vector2f(6.f, 6.f), {0.f, 0.f}, 0.4f
    });
    for (std::size_t ring = 0; ring < ringCount; ++ring)
    {
        const float distance = ringSpacing * static_cast<float>(ring + 1);
        for (std::size_t spoke = 0; spoke < spokeCount; ++spoke)
        {
            const float angle = 2.f * PI * static_cast<float>(spoke) /
                static_cast<float>(spokeCount);
            const sf::Vector2f radial(std::cos(angle), std::sin(angle));
            const sf::Vector2f tangent(-radial.y, radial.x);
            preset.bodies.push_back({
                4.f,
                center + radial * distance - sf::Vector2f(4.f, 4.f),
                tangent * (18.f + static_cast<float>(ring) * 4.f),
                0.4f
            });
        }
    }
    for (std::size_t ring = 0; ring < ringCount; ++ring)
    {
        const std::size_t ringStart = 1 + ring * spokeCount;
        for (std::size_t spoke = 0; spoke < spokeCount; ++spoke)
        {
            connect(
                preset,
                ringStart + spoke,
                ringStart + (spoke + 1) % spokeCount,
                9000.f,
                190.f
            );
            connect(
                preset,
                ring == 0 ? 0 : ringStart - spokeCount + spoke,
                ringStart + spoke,
                9000.f,
                190.f
            );
        }
    }
    preset.pinnedBodies.push_back(0);
    return preset;
}
}
