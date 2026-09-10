#pragma once

#include "physics/CircleBody.hpp"

namespace physics
{
bool solveDistanceConstraint(
    CircleBody& first,
    CircleBody& second,
    float restLength,
    float stiffness
);
}
