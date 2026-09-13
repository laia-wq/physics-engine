#include "presets/RollingTerrainPreset.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace presets
{
using app::ParticleMaterial;

void buildRollingTerrain(
    std::vector<app::CircleView>& bodies,
    std::vector<physics::Spring>& springs,
    bool includeTerrainDiagonals
)
{
    constexpr std::size_t COLUMNS = 49;
    constexpr std::size_t ROWS = 39;
    constexpr float NEAR_DEPTH = 280.f;
    constexpr float FAR_DEPTH = 1500.f;
    constexpr float CAMERA_DISTANCE = 220.f;
    constexpr float CAMERA_HEIGHT = 722.f;
    constexpr float FOCAL_LENGTH = 520.f;
    // The far ground begins near the top and the foreground reaches the
    // bottom, creating a full-screen elevated terrain view.
    constexpr float HORIZON_Y = -153.f;

    bodies.clear();
    springs.clear();
    std::array<std::array<std::size_t, COLUMNS>, ROWS> terrain{};

    const auto addParticle = [&](sf::Vector2f center,
                                 float radius,
                                 ParticleMaterial material,
                                 int group,
                                 bool fixed)
    {
        bodies.emplace_back(
            radius,
            center - sf::Vector2f(radius, radius),
            sf::Vector2f(0.f, 0.f),
            0.15f
        );
        bodies.back().material = material;
        bodies.back().groupId = group;
        if (fixed)
        {
            bodies.back().body.inverseMass = 0.f;
            bodies.back().showFixedOutline = false;
        }
        return bodies.size() - 1;
    };
    const auto connect = [&](std::size_t first, std::size_t second)
    {
        const sf::Vector2f difference =
            bodies[second].body.center() - bodies[first].body.center();
        springs.push_back({
            first,
            second,
            std::sqrt(difference.x * difference.x +
                difference.y * difference.y),
            6200.f,
            250.f
        });
    };

    for (std::size_t row = 0; row < ROWS; ++row)
    {
        // Construct the surface from far to near in real depth units.
        const float nearAmount = static_cast<float>(row) /
            static_cast<float>(ROWS - 1);
        // Space rows by their projected ground position. Linear world-Z
        // sampling crowded the horizon and left giant foreground cells.
        const float farGroundY = HORIZON_Y +
            CAMERA_HEIGHT * FOCAL_LENGTH /
                (FAR_DEPTH + CAMERA_DISTANCE);
        const float nearGroundY = HORIZON_Y +
            CAMERA_HEIGHT * FOCAL_LENGTH /
                (NEAR_DEPTH + CAMERA_DISTANCE);
        const float projectedGroundY = farGroundY +
            nearAmount * (nearGroundY - farGroundY);
        const float worldZ =
            CAMERA_HEIGHT * FOCAL_LENGTH /
                (projectedGroundY - HORIZON_Y) - CAMERA_DISTANCE;
        for (std::size_t column = 0; column < COLUMNS; ++column)
        {
            const float across = -1.f + 2.f * static_cast<float>(column) /
                static_cast<float>(COLUMNS - 1);
            const float perspective = FOCAL_LENGTH /
                (worldZ + CAMERA_DISTANCE);
            // Sample the visible part of a wide ground plane. Far rows
            // cover most of the view instead of collapsing into a wedge.
            const float screenHalfWidth = 370.f + 28.f * nearAmount;
            const float worldX = across * screenHalfWidth / perspective;

            // Broad overlapping forms now span the entire landscape.
            const float leftMountain = 175.f * std::exp(
                -std::pow((across + 0.38f) / 0.28f, 2.f) -
                std::pow((worldZ - 1080.f) / 380.f, 2.f)
            );
            const float rightMountain = 215.f * std::exp(
                -std::pow((across - 0.3f) / 0.25f, 2.f) -
                std::pow((worldZ - 1210.f) / 330.f, 2.f)
            );
            const float outerLeftRidge = 105.f * std::exp(
                -std::pow((across + 0.86f) / 0.3f, 2.f) -
                std::pow((worldZ - 900.f) / 420.f, 2.f)
            );
            const float outerRightRidge = 125.f * std::exp(
                -std::pow((across - 0.82f) / 0.31f, 2.f) -
                std::pow((worldZ - 980.f) / 410.f, 2.f)
            );
            const float middleRidge = 52.f * std::exp(
                -std::pow((across + 0.03f) / 0.58f, 2.f) -
                std::pow((worldZ - 720.f) / 340.f, 2.f)
            );
            const float foothills = 34.f * std::exp(
                -std::pow((across + 0.55f) / 0.42f, 2.f) -
                std::pow((worldZ - 520.f) / 210.f, 2.f)
            );
            const float middleLeftHill = 104.f * std::exp(
                -std::pow((across + 0.58f) / 0.32f, 2.f) -
                std::pow((worldZ - 650.f) / 225.f, 2.f)
            );
            const float middleRightHill = 94.f * std::exp(
                -std::pow((across - 0.46f) / 0.34f, 2.f) -
                std::pow((worldZ - 590.f) / 215.f, 2.f)
            );
            const float frontHill = 44.f * std::exp(
                -std::pow((across - 0.08f) / 0.62f, 2.f) -
                std::pow((worldZ - 440.f) / 250.f, 2.f)
            );
            const float leftSideHill = 74.f * std::exp(
                -std::pow((across + 0.88f) / 0.24f, 2.f) -
                std::pow((worldZ - 560.f) / 270.f, 2.f)
            );
            const float rightSideHill = 68.f * std::exp(
                -std::pow((across - 0.87f) / 0.25f, 2.f) -
                std::pow((worldZ - 500.f) / 250.f, 2.f)
            );
            const float nearLeftHill = 158.f * std::exp(
                -std::pow((across + 0.48f) / 0.33f, 2.f) -
                std::pow((worldZ - 410.f) / 180.f, 2.f)
            );
            const float nearCenterHill = 128.f * std::exp(
                -std::pow((across + 0.02f) / 0.3f, 2.f) -
                std::pow((worldZ - 500.f) / 195.f, 2.f)
            );
            const float nearRightHill = 148.f * std::exp(
                -std::pow((across - 0.58f) / 0.32f, 2.f) -
                std::pow((worldZ - 390.f) / 175.f, 2.f)
            );
            const float valley = -34.f * std::exp(
                -std::pow((across - 0.02f) / 0.2f, 2.f) -
                std::pow((worldZ - 860.f) / 430.f, 2.f)
            );
            const float rollingDetail =
                7.f * std::sin(across * 5.2f + worldZ * 0.0032f) +
                2.5f * std::sin(across * 10.3f - worldZ * 0.0055f);
            float worldHeight = std::max(
                -30.f,
                leftMountain + rightMountain + outerLeftRidge +
                    outerRightRidge + middleRidge + foothills +
                    middleLeftHill + middleRightHill + frontHill +
                    leftSideHill + rightSideHill + nearLeftHill +
                    nearCenterHill + nearRightHill + valley + rollingDetail
            );
            const float foregroundTransition = std::clamp(
                (1.f - nearAmount) / 0.14f, 0.f, 1.f
            );
            // Preserve part of the hill height at the nearest row instead
            // of forcing it into a straight line on the screen boundary.
            worldHeight *= 0.32f + 0.68f * foregroundTransition;

            // Height alone bends horizontal contours but leaves the
            // front-to-back grid lines vertical. This restrained lateral
            // displacement makes those lines flow around terrain forms.
            const float heightInfluence = std::clamp(
                worldHeight / 210.f, -0.15f, 1.f
            );
            const float edgeFade = 1.f - across * across;
            const float lateralBend = edgeFade * (
                82.f * ((across + 0.48f) / 0.33f) *
                    (nearLeftHill / 158.f) +
                62.f * ((across + 0.02f) / 0.3f) *
                    (nearCenterHill / 128.f) +
                78.f * ((across - 0.58f) / 0.32f) *
                    (nearRightHill / 148.f) +
                34.f * ((across + 0.58f) / 0.32f) *
                    (middleLeftHill / 104.f) +
                32.f * ((across - 0.46f) / 0.34f) *
                    (middleRightHill / 94.f) +
                46.f * ((across - 0.08f) / 0.62f) *
                    (frontHill / 44.f) +
                34.f * ((across + 0.88f) / 0.24f) *
                    (leftSideHill / 74.f) +
                32.f * ((across - 0.87f) / 0.25f) *
                    (rightSideHill / 68.f) +
                5.f * heightInfluence *
                    std::sin(worldZ * 0.004f + across * 2.5f)
            );

            // A perspective camera performs the actual 3D-to-2D projection.
            const sf::Vector2f center(
                400.f + worldX * perspective + lateralBend,
                HORIZON_Y + (CAMERA_HEIGHT - worldHeight) * perspective
            );
            const float radius = 0.85f + 1.45f * nearAmount;
            terrain[row][column] = addParticle(
                center,
                radius,
                ParticleMaterial::Structural,
                50,
                row == ROWS - 1
            );

            if (column > 0)
            {
                connect(terrain[row][column - 1], terrain[row][column]);
            }
            if (row > 0)
            {
                connect(terrain[row - 1][column], terrain[row][column]);
                if (includeTerrainDiagonals && column > 0)
                {
                    if ((row + column) % 2 == 0)
                    {
                        connect(
                            terrain[row - 1][column - 1],
                            terrain[row][column]
                        );
                    }
                    else
                    {
                        connect(
                            terrain[row - 1][column],
                            terrain[row][column - 1]
                        );
                    }
                }
            }
        }
    }

    // Buildings use the same node-and-connection language as the terrain.
    // Sloped foundations follow the ground; roof and side grids add depth.
    std::vector<sf::Vector2f> buildingEntrances;
    const auto addWireframeBuilding = [&](
        std::size_t terrainRow,
        std::size_t firstTerrainColumn,
        std::size_t terrainColumnSpan,
        std::size_t floors,
        float height,
        int group
    )
    {
        constexpr std::size_t BAYS = 4;
        constexpr std::size_t DEPTH_SEGMENTS = 3;
        const sf::Vector2f leftGround =
            bodies[terrain[terrainRow][firstTerrainColumn]].body.center();
        const sf::Vector2f rightGround = bodies[
            terrain[terrainRow][firstTerrainColumn + terrainColumnSpan]
        ].body.center();
        buildingEntrances.push_back(
            (leftGround + rightGround) * 0.5f - sf::Vector2f(0.f, 2.f)
        );
        std::vector<std::size_t> facade((floors + 1) * (BAYS + 1));
        const auto facadeAt = [&](std::size_t floor, std::size_t bay)
            -> std::size_t&
        {
            return facade[floor * (BAYS + 1) + bay];
        };

        for (std::size_t floor = 0; floor <= floors; ++floor)
        {
            const float verticalAmount = static_cast<float>(floor) /
                static_cast<float>(floors);
            for (std::size_t bay = 0; bay <= BAYS; ++bay)
            {
                const float horizontalAmount = static_cast<float>(bay) /
                    static_cast<float>(BAYS);
                const float groundY = leftGround.y +
                    (rightGround.y - leftGround.y) * horizontalAmount;
                const sf::Vector2f center(
                    leftGround.x +
                        (rightGround.x - leftGround.x) * horizontalAmount,
                    groundY - 2.f - height * verticalAmount
                );
                facadeAt(floor, bay) = addParticle(
                    center, 1.35f, ParticleMaterial::Structural, group, true
                );
                if (bay > 0)
                {
                    connect(facadeAt(floor, bay - 1),
                        facadeAt(floor, bay));
                }
                if (floor > 0)
                {
                    connect(facadeAt(floor - 1, bay),
                        facadeAt(floor, bay));
                }
                if (floor == 0)
                {
                    const std::size_t groundColumn = firstTerrainColumn +
                        static_cast<std::size_t>(std::round(
                            horizontalAmount *
                                static_cast<float>(terrainColumnSpan)
                        ));
                    connect(facadeAt(floor, bay),
                        terrain[terrainRow][groundColumn]);
                }
            }
        }

        // Stretch the top away from the facade to form a perspective roof.
        const float facadeWidth = std::abs(rightGround.x - leftGround.x);
        const float buildingCenterX = (leftGround.x + rightGround.x) * 0.5f;
        const float sideDirection = buildingCenterX < 400.f ? 1.f : -1.f;
        const sf::Vector2f depthOffset(
            sideDirection * std::clamp(facadeWidth * 0.62f, 20.f, 40.f),
            -std::clamp(height * 0.23f, 14.f, 30.f)
        );
        const std::size_t visibleBay = sideDirection > 0.f ? BAYS : 0;
        std::vector<std::size_t> roof(
            (DEPTH_SEGMENTS + 1) * (BAYS + 1)
        );
        const auto roofAt = [&](std::size_t depth, std::size_t bay)
            -> std::size_t&
        {
            return roof[depth * (BAYS + 1) + bay];
        };
        for (std::size_t bay = 0; bay <= BAYS; ++bay)
        {
            roofAt(0, bay) = facadeAt(floors, bay);
        }
        for (std::size_t depth = 1; depth <= DEPTH_SEGMENTS; ++depth)
        {
            const float amount = static_cast<float>(depth) /
                static_cast<float>(DEPTH_SEGMENTS);
            for (std::size_t bay = 0; bay <= BAYS; ++bay)
            {
                const sf::Vector2f front =
                    bodies[facadeAt(floors, bay)].body.center();
                roofAt(depth, bay) = addParticle(
                    front + depthOffset * amount, 1.25f,
                    ParticleMaterial::Structural, group, true
                );
                connect(roofAt(depth - 1, bay), roofAt(depth, bay));
                if (bay > 0)
                {
                    connect(roofAt(depth, bay - 1), roofAt(depth, bay));
                }
            }
        }

        // Repeat the grid over the visible side to create window cells.
        std::vector<std::size_t> side(
            (floors + 1) * (DEPTH_SEGMENTS + 1)
        );
        const auto sideAt = [&](std::size_t floor, std::size_t depth)
            -> std::size_t&
        {
            return side[floor * (DEPTH_SEGMENTS + 1) + depth];
        };
        for (std::size_t floor = 0; floor <= floors; ++floor)
        {
            sideAt(floor, 0) = facadeAt(floor, visibleBay);
        }
        for (std::size_t depth = 1; depth <= DEPTH_SEGMENTS; ++depth)
        {
            const float amount = static_cast<float>(depth) /
                static_cast<float>(DEPTH_SEGMENTS);
            for (std::size_t floor = 0; floor <= floors; ++floor)
            {
                if (floor == floors)
                {
                    sideAt(floor, depth) = roofAt(depth, visibleBay);
                }
                else
                {
                    const sf::Vector2f front =
                        bodies[facadeAt(floor, visibleBay)].body.center();
                    sideAt(floor, depth) = addParticle(
                        front + depthOffset * amount, 1.25f,
                        ParticleMaterial::Structural, group, true
                    );
                    connect(sideAt(floor, depth - 1),
                        sideAt(floor, depth));
                }
                if (floor > 0)
                {
                    connect(sideAt(floor - 1, depth),
                        sideAt(floor, depth));
                }
            }
        }
    };

    // Ten buildings stay within stable inner terrain columns. Wider
    // foreground footprints also create visibly different proportions.
    addWireframeBuilding(14, 9, 4, 3, 52.f, 60);
    addWireframeBuilding(15, 35, 4, 3, 54.f, 61);
    addWireframeBuilding(20, 13, 4, 4, 70.f, 62);
    addWireframeBuilding(21, 32, 4, 4, 74.f, 63);
    addWireframeBuilding(25, 8, 5, 4, 82.f, 64);
    addWireframeBuilding(26, 35, 5, 4, 84.f, 65);
    addWireframeBuilding(30, 13, 5, 5, 98.f, 66);
    addWireframeBuilding(31, 30, 5, 6, 108.f, 67);
    addWireframeBuilding(35, 9, 5, 4, 86.f, 68);
    addWireframeBuilding(35, 34, 5, 5, 94.f, 69);

    // Closely spaced samples turn the centre route into a smooth winding
    // curve rather than an angular zigzag or a strip beneath buildings.
    std::vector<std::size_t> mainPath;
    for (std::size_t row = 6; row < ROWS; row += 2)
    {
        const float progress = static_cast<float>(row - 6) /
            static_cast<float>(ROWS - 7);
        const float windingColumn =
            24.f + 4.2f * std::sin(progress * 7.2f) +
            1.4f * std::sin(progress * 14.4f + 0.8f);
        const std::size_t leftColumn = static_cast<std::size_t>(
            std::floor(windingColumn)
        );
        const std::size_t rightColumn = std::min(
            leftColumn + 1, COLUMNS - 1
        );
        const float blend =
            windingColumn - static_cast<float>(leftColumn);
        sf::Vector2f center =
            bodies[terrain[row][leftColumn]].body.center() * (1.f - blend) +
            bodies[terrain[row][rightColumn]].body.center() * blend;
        center.y -= 2.f;
        const std::size_t current = addParticle(
            center, 1.4f, ParticleMaterial::Structural, 55, true
        );
        if (!mainPath.empty())
        {
            connect(mainPath.back(), current);
        }
        mainPath.push_back(current);
    }
    for (std::size_t building = 0;
         building < buildingEntrances.size(); ++building)
    {
        const sf::Vector2f entrance = buildingEntrances[building];
        const auto closest = std::min_element(
            mainPath.begin(), mainPath.end(),
            [&](std::size_t first, std::size_t second)
            {
                const sf::Vector2f firstDifference =
                    bodies[first].body.center() - entrance;
                const sf::Vector2f secondDifference =
                    bodies[second].body.center() - entrance;
                return firstDifference.x * firstDifference.x +
                        firstDifference.y * firstDifference.y <
                    secondDifference.x * secondDifference.x +
                        secondDifference.y * secondDifference.y;
            }
        );
        const sf::Vector2f junction = bodies[*closest].body.center();
        const sf::Vector2f difference = entrance - junction;
        const float length = std::sqrt(
            difference.x * difference.x + difference.y * difference.y
        );
        const sf::Vector2f normal = length > 0.f
            ? sf::Vector2f(-difference.y, difference.x) / length
            : sf::Vector2f(0.f, 0.f);
        const float curveDirection = entrance.x < junction.x ? -1.f : 1.f;
        const sf::Vector2f control =
            (junction + entrance) * 0.5f +
            normal * curveDirection * std::min(12.f, length * 0.16f);
        std::size_t previous = *closest;
        for (std::size_t step = 1; step <= 5; ++step)
        {
            const float amount = static_cast<float>(step) / 5.f;
            const float inverse = 1.f - amount;
            const sf::Vector2f center =
                junction * (inverse * inverse) +
                control * (2.f * inverse * amount) +
                entrance * (amount * amount);
            const std::size_t current = addParticle(
                center, 1.25f, ParticleMaterial::Structural, 55, true
            );
            connect(previous, current);
            previous = current;
        }
    }

    // Retained temporarily for comparison; the unified primary mesh now
    // reaches the frame itself, so this separate skirt is not generated.
    if (false)
    {
    // Continue the terrain through several rows and columns before it
    // reaches the frame, so the boundary reads as part of the same mesh.
    constexpr std::size_t BORDER_LAYERS = 4;
    std::array<std::size_t, COLUMNS> previousBottom{};
    std::array<std::size_t, BORDER_LAYERS + 1> bottomLeftByLayer{};
    std::array<std::size_t, BORDER_LAYERS + 1> bottomRightByLayer{};
    bottomLeftByLayer[0] = terrain[ROWS - 1][0];
    bottomRightByLayer[0] = terrain[ROWS - 1][COLUMNS - 1];
    for (std::size_t column = 0; column < COLUMNS; ++column)
    {
        previousBottom[column] = terrain[ROWS - 1][column];
    }
    for (std::size_t layer = 1; layer <= BORDER_LAYERS; ++layer)
    {
        const float t = static_cast<float>(layer) /
            static_cast<float>(BORDER_LAYERS);
        std::optional<std::size_t> previousAcross;
        for (std::size_t column = 0; column < COLUMNS; ++column)
        {
            const float across = -1.f + 2.f * static_cast<float>(column) /
                static_cast<float>(COLUMNS - 1);
            const sf::Vector2f source =
                bodies[terrain[ROWS - 1][column]].body.center();
            const float targetX = 2.f + 796.f *
                static_cast<float>(column) /
                static_cast<float>(COLUMNS - 1);
            const float bottomRelief =
                30.f * std::exp(-std::pow((across + 0.62f) / 0.3f, 2.f)) +
                36.f * std::exp(-std::pow((across - 0.58f) / 0.27f, 2.f)) +
                8.f * std::sin(across * 5.4f + 0.7f);
            const float contour =
                bottomRelief * std::sin(t * 3.14159265359f);
            const sf::Vector2f center(
                source.x + (targetX - source.x) * t,
                source.y + (598.f - source.y) * t - contour
            );
            const std::size_t current = addParticle(
                center, 2.3f + (1.2f - 2.3f) * t,
                ParticleMaterial::Structural, 50,
                layer == BORDER_LAYERS
            );
            connect(previousBottom[column], current);
            if (previousAcross)
            {
                connect(*previousAcross, current);
            }
            previousAcross = current;
            previousBottom[column] = current;
            if (column == 0)
            {
                bottomLeftByLayer[layer] = current;
            }
            else if (column == COLUMNS - 1)
            {
                bottomRightByLayer[layer] = current;
            }
        }
    }

    std::array<std::size_t, ROWS> previousLeft{};
    std::array<std::size_t, ROWS> previousRight{};
    for (std::size_t row = 1; row + 1 < ROWS; ++row)
    {
        previousLeft[row] = terrain[row][0];
        previousRight[row] = terrain[row][COLUMNS - 1];
    }
    for (std::size_t layer = 1; layer <= BORDER_LAYERS; ++layer)
    {
        const float t = static_cast<float>(layer) /
            static_cast<float>(BORDER_LAYERS);
        std::optional<std::size_t> lastLeft;
        std::optional<std::size_t> lastRight;
        for (std::size_t row = 1; row + 1 < ROWS; ++row)
        {
            const sf::Vector2f leftSource =
                bodies[terrain[row][0]].body.center();
            const sf::Vector2f rightSource =
                bodies[terrain[row][COLUMNS - 1]].body.center();
            const float depthAmount = static_cast<float>(row) /
                static_cast<float>(ROWS - 1);
            const float edgeRadius = 0.85f + 1.45f * depthAmount;
            const float transitionRadius =
                edgeRadius + (1.2f - edgeRadius) * t;
            const float contour = 6.f *
                std::sin(depthAmount * 8.f + static_cast<float>(layer)) *
                t * (1.f - t);
            const std::size_t left = addParticle(
                sf::Vector2f(
                    leftSource.x + (2.f - leftSource.x) * t,
                    leftSource.y + contour
                ),
                transitionRadius, ParticleMaterial::Structural, 50,
                layer == BORDER_LAYERS
            );
            const std::size_t right = addParticle(
                sf::Vector2f(
                    rightSource.x + (798.f - rightSource.x) * t,
                    rightSource.y - contour
                ),
                transitionRadius, ParticleMaterial::Structural, 50,
                layer == BORDER_LAYERS
            );
            connect(previousLeft[row], left);
            connect(previousRight[row], right);
            if (lastLeft)
            {
                connect(*lastLeft, left);
                connect(*lastRight, right);
            }
            lastLeft = left;
            lastRight = right;
            previousLeft[row] = left;
            previousRight[row] = right;
        }
        // Share the bottom corner of this layer with the foreground strip,
        // closing the seam into one continuous U-shaped grid.
        if (lastLeft)
        {
            connect(*lastLeft, bottomLeftByLayer[layer]);
            connect(*lastRight, bottomRightByLayer[layer]);
        }
    }

    }

}
}
