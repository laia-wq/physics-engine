#pragma once

#include "app/ParticleView.hpp"
#include "physics/Spring.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace app
{
bool isBuildingBody(const std::vector<CircleView>& bodies, std::size_t index);
void updateParticleAppearance(
    std::vector<CircleView>& bodies,
    bool colorByMaterial,
    bool colorByCharge,
    bool highlightCollisions,
    std::optional<std::size_t> selectedBody
);
void drawSpringConnections(
    sf::RenderTarget& target,
    const std::vector<CircleView>& bodies,
    const std::vector<physics::Spring>& springs,
    unsigned char alpha = 255,
    int buildingLayer = 0,
    int buildingGroup = -1
);
void drawNonBuildingBodies(
    sf::RenderTarget& target,
    const std::vector<CircleView>& bodies
);
void drawBuildingsBackToFront(
    sf::RenderTarget& target,
    const std::vector<CircleView>& bodies,
    const std::vector<physics::Spring>& springs
);
}
