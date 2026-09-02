#pragma once

#include "physics/CircleBody.hpp"

namespace physics
{
bool applyElectrostaticPair(
    CircleBody& first,
    CircleBody& second,
    float strength,
    float softening
);
}
