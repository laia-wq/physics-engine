#pragma once

#include "app/ParticleView.hpp"
#include "physics/Spring.hpp"

#include <vector>

namespace presets
{
void buildWindblownTree(
    std::vector<app::CircleView>& bodies,
    std::vector<physics::Spring>& springs
);
}
