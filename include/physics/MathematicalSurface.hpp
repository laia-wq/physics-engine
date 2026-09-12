#pragma once

#include <SFML/System/Vector2.hpp>

#include <cstddef>

namespace physics
{
struct MathematicalSurfaceSettings
{
    float spacetimeWellDepth;
    float waveHeight;
    float doubleWellDepth;
};

struct MathematicalSurfacePoint
{
    sf::Vector2f projectedPosition;
    float downwardDepth = 0.f;
};

bool isHeightField(int form);
std::size_t mathematicalSurfaceColumns(int form);
std::size_t mathematicalSurfaceRows(int form);
bool mathematicalSurfaceWrapsColumns(int form);
bool mathematicalSurfaceWrapsRows(int form);

MathematicalSurfacePoint calculateMathematicalSurfacePoint(
    int form,
    std::size_t row,
    std::size_t column,
    const MathematicalSurfaceSettings& settings
);
}
