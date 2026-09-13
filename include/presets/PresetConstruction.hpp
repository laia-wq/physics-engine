#pragma once

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

namespace presets
{
enum class FoundationPreset
{
    Classic,
    Stress,
    HeadOn,
    ZeroGravity,
    Rain,
    Orbit
};

struct BodyDefinition
{
    float radius;
    sf::Vector2f position;
    sf::Vector2f velocity;
    float restitution;
};

std::vector<BodyDefinition> buildFoundationPreset(
    FoundationPreset preset,
    std::size_t stressBodyCount = 0
);
}
