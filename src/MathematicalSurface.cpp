#include "physics/MathematicalSurface.hpp"

#include <algorithm>
#include <cmath>

namespace physics
{
namespace
{
constexpr float PI = 3.14159265359f;
}

bool isHeightField(int form)
{
    return form == 2 || form == 5 || form == 6;
}

std::size_t mathematicalSurfaceColumns(int form)
{
    return isHeightField(form) ? 35 : (form == 3 ? 56 : 40);
}

std::size_t mathematicalSurfaceRows(int form)
{
    return isHeightField(form) ? 29 : (form == 3 ? 13 : 19);
}

bool mathematicalSurfaceWrapsColumns(int form)
{
    return !isHeightField(form) && form != 3;
}

bool mathematicalSurfaceWrapsRows(int form)
{
    return form == 0;
}

MathematicalSurfacePoint calculateMathematicalSurfacePoint(
    int form,
    std::size_t row,
    std::size_t column,
    const MathematicalSurfaceSettings& settings
)
{
    const std::size_t columns = mathematicalSurfaceColumns(form);
    const std::size_t rows = mathematicalSurfaceRows(form);
    const bool wrapRows = mathematicalSurfaceWrapsRows(form);
    const float u = 2.f * PI * static_cast<float>(column) /
        static_cast<float>(columns);
    const float v = rows > 1
        ? static_cast<float>(row) /
            static_cast<float>(wrapRows ? rows : rows - 1)
        : 0.f;
    float modelX = 0.f;
    float modelY = 0.f;
    float modelZ = 0.f;

    if (form == 0)
    {
        const float tubeAngle = 2.f * PI * v;
        constexpr float MAJOR_RADIUS = 125.f;
        constexpr float TUBE_RADIUS = 48.f;
        modelX = (MAJOR_RADIUS + TUBE_RADIUS * std::cos(tubeAngle)) *
            std::cos(u);
        modelZ = (MAJOR_RADIUS + TUBE_RADIUS * std::cos(tubeAngle)) *
            std::sin(u);
        modelY = TUBE_RADIUS * std::sin(tubeAngle);
    }
    else if (form == 1)
    {
        const float depth = -0.94f + 1.88f * v;
        const float sliceScale = std::sqrt(
            std::max(0.f, 1.f - depth * depth)
        );
        const float heartX = 16.f * std::pow(std::sin(u), 3.f);
        const float heartY = 13.f * std::cos(u) -
            5.f * std::cos(2.f * u) - 2.f * std::cos(3.f * u) -
            std::cos(4.f * u);
        modelX = heartX * 9.2f * sliceScale;
        modelY = heartY * 8.2f * sliceScale;
        modelZ = depth * 72.f;
    }
    else if (form == 3)
    {
        const float width = -1.f + 2.f * v;
        constexpr float RADIUS = 128.f;
        constexpr float HALF_WIDTH = 55.f;
        const float bandRadius =
            RADIUS + HALF_WIDTH * width * std::cos(u * 0.5f);
        modelX = bandRadius * std::cos(u);
        modelZ = bandRadius * std::sin(u);
        modelY = HALF_WIDTH * width * std::sin(u * 0.5f);
    }
    else if (form == 4)
    {
        const float latitude = 0.05f + (PI - 0.1f) * v;
        constexpr float RADIUS = 142.f;
        modelX = RADIUS * std::sin(latitude) * std::cos(u);
        modelZ = RADIUS * std::sin(latitude) * std::sin(u);
        modelY = RADIUS * std::cos(latitude);
    }
    else
    {
        const float across = -1.f + 2.f * static_cast<float>(column) /
            static_cast<float>(columns - 1);
        const float depth = -1.f + 2.f * v;
        modelX = across * 300.f;
        modelZ = depth * 235.f;
        const float distance = std::sqrt(modelX * modelX + modelZ * modelZ);
        if (form == 5)
        {
            modelY = settings.waveHeight * 0.71f *
                std::sin(modelX * 0.025f) * std::cos(modelZ * 0.027f) +
                settings.waveHeight * 0.29f *
                    std::sin(modelX * 0.014f + modelZ * 0.032f);
        }
        else if (form == 6)
        {
            const float leftDistanceSquared =
                (modelX + 105.f) * (modelX + 105.f) + modelZ * modelZ;
            const float rightDistanceSquared =
                (modelX - 105.f) * (modelX - 105.f) + modelZ * modelZ;
            modelY = -settings.doubleWellDepth *
                std::exp(-leftDistanceSquared / 15000.f) -
                settings.doubleWellDepth *
                std::exp(-rightDistanceSquared / 15000.f);
        }
        else
        {
            modelY = -settings.spacetimeWellDepth *
                std::exp(-(distance * distance) / 24000.f);
        }
    }

    MathematicalSurfacePoint result;
    result.downwardDepth = form == 2 ? std::max(0.f, -modelY) : 0.f;
    if (isHeightField(form))
    {
        result.projectedPosition = {
            400.f + modelX + modelZ * 0.12f,
            270.f + modelZ * 0.62f - modelY
        };
    }
    else if (form == 3)
    {
        result.projectedPosition = {
            400.f + modelX * 0.92f + modelZ * 0.22f,
            315.f - modelY * 1.28f + modelZ * 0.5f - modelX * 0.08f
        };
    }
    else
    {
        constexpr float YAW = -0.48f;
        const float rotatedX =
            modelX * std::cos(YAW) - modelZ * std::sin(YAW);
        const float rotatedDepth =
            modelX * std::sin(YAW) + modelZ * std::cos(YAW);
        result.projectedPosition = {
            400.f + rotatedX,
            315.f - modelY + rotatedDepth * (form == 0 ? 0.68f : 0.34f)
        };
    }
    return result;
}
}
