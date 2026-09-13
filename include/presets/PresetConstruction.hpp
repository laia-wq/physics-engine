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

struct ConnectionDefinition
{
    std::size_t first;
    std::size_t second;
    float restLength;
    float stiffness;
    float damping;
};

struct ConnectedPreset
{
    std::vector<BodyDefinition> bodies;
    std::vector<ConnectionDefinition> connections;
    std::vector<std::size_t> pinnedBodies;
};

std::vector<BodyDefinition> buildFoundationPreset(
    FoundationPreset preset,
    std::size_t stressBodyCount = 0
);

ConnectedPreset buildSpringChain(
    std::size_t bodyCount,
    float spacing,
    float width,
    float stiffness,
    float damping
);

ConnectedPreset buildSoftBodyLattice(
    std::size_t columns,
    std::size_t rows,
    float requestedSpacing,
    float width,
    float height
);

ConnectedPreset buildRadialWeb(
    std::size_t ringCount,
    std::size_t spokeCount
);
}
