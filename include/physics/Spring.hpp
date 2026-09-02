#pragma once

#include "physics/CircleBody.hpp"

#include <cstddef>

namespace physics
{
struct Spring
{
    std::size_t first;
    std::size_t second;
    float restLength;
    float stiffness;
    float damping;
};

bool applySpringForce(
    CircleBody& first,
    CircleBody& second,
    float restLength,
    float stiffness,
    float damping
);
}
