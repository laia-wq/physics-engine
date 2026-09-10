#include <SFML/Graphics.hpp>

#include <imgui-SFML.h>
#include <imgui.h>

#include "physics/BroadPhase.hpp"
#include "physics/Collision.hpp"
#include "physics/DistanceConstraint.hpp"
#include "physics/Electrostatics.hpp"
#include "physics/Spring.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace
{
constexpr float WINDOW_WIDTH = 800.f;
constexpr float WINDOW_HEIGHT = 600.f;
constexpr float MINIMUM_BOUNCE_SPEED = 15.f;
constexpr float FIXED_TIME_STEP = 1.f / 120.f;
constexpr float MAX_FRAME_TIME = 0.25f;
constexpr float MAX_THROW_SPEED = 1500.f;
constexpr float VELOCITY_VECTOR_SCALE = 0.12f;
constexpr float MAX_DEBUG_VECTOR_LENGTH = 120.f;
constexpr float NORMAL_VECTOR_LENGTH = 35.f;
}

struct DebugContact
{
    sf::Vector2f point;
    sf::Vector2f normal;
};

struct PointField
{
    sf::Vector2f position;
    float radialStrength;
    float vortexStrength;
    bool enabled;
    float oscillationAmount = 0.f;
    float oscillationFrequency = 1.f;
    float remainingLifetime = -1.f;
    bool chargeSensitive = false;
};

enum class ParticleMaterial
{
    Structural,
    Flexible,
    Fragile,
    Fixed
};

const char* materialName(ParticleMaterial material)
{
    switch (material)
    {
        case ParticleMaterial::Structural: return "Structural";
        case ParticleMaterial::Flexible: return "Flexible";
        case ParticleMaterial::Fragile: return "Fragile";
        case ParticleMaterial::Fixed: return "Fixed";
    }
    return "Unknown";
}

float materialForceResponse(ParticleMaterial material)
{
    switch (material)
    {
        case ParticleMaterial::Structural: return 0.35f;
        case ParticleMaterial::Flexible: return 1.f;
        case ParticleMaterial::Fragile: return 1.4f;
        case ParticleMaterial::Fixed: return 0.f;
    }
    return 1.f;
}

float materialBreakingScale(ParticleMaterial material)
{
    switch (material)
    {
        case ParticleMaterial::Structural: return 1.6f;
        case ParticleMaterial::Flexible: return 1.f;
        case ParticleMaterial::Fragile: return 0.4f;
        case ParticleMaterial::Fixed: return 1.6f;
    }
    return 1.f;
}

sf::Color materialColor(ParticleMaterial material)
{
    switch (material)
    {
        case ParticleMaterial::Structural: return sf::Color(90, 145, 255);
        case ParticleMaterial::Flexible: return sf::Color(80, 235, 180);
        case ParticleMaterial::Fragile: return sf::Color(255, 120, 185);
        case ParticleMaterial::Fixed: return sf::Color(255, 210, 70);
    }
    return sf::Color::White;
}

struct CircleView
{
    physics::CircleBody body;
    sf::CircleShape shape;
    ParticleMaterial material = ParticleMaterial::Flexible;
    int groupId = 0;

    CircleView(
        float radius,
        sf::Vector2f position,
        sf::Vector2f velocity,
        float restitution
    )
        : body(
              radius,
              position,
              velocity,
              sf::Vector2f(0.f, 0.f),
              restitution
          ),
          shape(radius)
    {
        sync();
    }

    void sync()
    {
        shape.setPosition(body.position);
    }
};

sf::Vector2f toWorldPosition(sf::Vector2i pixelPosition)
{
    return sf::Vector2f(
        static_cast<float>(pixelPosition.x),
        static_cast<float>(pixelPosition.y)
    );
}

bool containsPoint(const CircleView& view, sf::Vector2f point)
{
    const sf::Vector2f difference = point - view.body.center();
    return difference.x * difference.x + difference.y * difference.y <=
        view.body.radius * view.body.radius;
}

float distanceSquaredToSegment(
    sf::Vector2f point,
    sf::Vector2f start,
    sf::Vector2f end
)
{
    const sf::Vector2f segment = end - start;
    const float lengthSquared =
        segment.x * segment.x + segment.y * segment.y;
    if (lengthSquared <= 0.000001f)
    {
        const sf::Vector2f difference = point - start;
        return difference.x * difference.x + difference.y * difference.y;
    }
    const sf::Vector2f fromStart = point - start;
    const float projection = std::clamp(
        (fromStart.x * segment.x + fromStart.y * segment.y) /
            lengthSquared,
        0.f,
        1.f
    );
    const sf::Vector2f nearest = start + segment * projection;
    const sf::Vector2f difference = point - nearest;
    return difference.x * difference.x + difference.y * difference.y;
}

std::optional<std::size_t> findBodyAt(
    const std::vector<CircleView>& bodies,
    sf::Vector2f point
)
{
    // In dense structures, pinned anchors can sit beneath moving particles.
    // Give anchors selection priority so they remain editable.
    for (std::size_t index = bodies.size(); index > 0; --index)
    {
        if (bodies[index - 1].body.inverseMass == 0.f &&
            containsPoint(bodies[index - 1], point))
        {
            return index - 1;
        }
    }
    for (std::size_t index = bodies.size(); index > 0; --index)
    {
        if (containsPoint(bodies[index - 1], point))
        {
            return index - 1;
        }
    }

    return std::nullopt;
}

void limitMagnitude(sf::Vector2f& vector, float maximumLength)
{
    const float lengthSquared = vector.x * vector.x + vector.y * vector.y;
    if (lengthSquared > maximumLength * maximumLength)
    {
        const float scale = maximumLength / std::sqrt(lengthSquared);
        vector *= scale;
    }
}

void drawLine(
    sf::RenderTarget& target,
    sf::Vector2f start,
    sf::Vector2f end,
    sf::Color color
)
{
    const std::array vertices{
        sf::Vertex{start, color},
        sf::Vertex{end, color}
    };
    target.draw(vertices.data(), vertices.size(), sf::PrimitiveType::Lines);
}

void vectorPad(
    const char* label,
    float values[2],
    float maximumValue,
    float padSize = 180.f
)
{
    ImGui::PushID("vector-pad");
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("pad", ImVec2(padSize, padSize));

    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float normalizedX = std::clamp(
            (mouse.x - topLeft.x) / padSize * 2.f - 1.f,
            -1.f,
            1.f
        );
        const float normalizedY = std::clamp(
            (mouse.y - topLeft.y) / padSize * 2.f - 1.f,
            -1.f,
            1.f
        );
        values[0] = normalizedX * maximumValue;
        values[1] = normalizedY * maximumValue;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 bottomRight(topLeft.x + padSize, topLeft.y + padSize);
    const ImVec2 center(topLeft.x + padSize * 0.5f, topLeft.y + padSize * 0.5f);
    drawList->AddRectFilled(topLeft, bottomRight, IM_COL32(20, 29, 39, 255));
    drawList->AddRect(topLeft, bottomRight, IM_COL32(90, 120, 145, 255));
    drawList->AddLine(
        ImVec2(center.x, topLeft.y),
        ImVec2(center.x, bottomRight.y),
        IM_COL32(65, 85, 105, 255)
    );
    drawList->AddLine(
        ImVec2(topLeft.x, center.y),
        ImVec2(bottomRight.x, center.y),
        IM_COL32(65, 85, 105, 255)
    );

    const ImVec2 handle(
        center.x + values[0] / maximumValue * padSize * 0.5f,
        center.y + values[1] / maximumValue * padSize * 0.5f
    );
    drawList->AddLine(center, handle, IM_COL32(80, 190, 255, 255), 2.f);
    drawList->AddCircleFilled(handle, 7.f, IM_COL32(80, 190, 255, 255));
    ImGui::Text("X: %.0f   Y: %.0f", values[0], values[1]);
    ImGui::PopID();
    ImGui::PopID();
}

int main()
{
    const float windowWidth = WINDOW_WIDTH;
    const float windowHeight = WINDOW_HEIGHT;

    sf::RenderWindow window(
        sf::VideoMode({
            static_cast<unsigned int>(windowWidth),
            static_cast<unsigned int>(windowHeight)
        }),
        "My Physics Engine"
    );

    if (!ImGui::SFML::Init(window))
    {
        return 1;
    }
    ImGui::GetIO().IniFilename = nullptr;

    sf::RenderTexture canvasTexture(sf::Vector2u(
        static_cast<unsigned int>(windowWidth),
        static_cast<unsigned int>(windowHeight)
    ));
    canvasTexture.clear(sf::Color(5, 8, 20));
    canvasTexture.display();

    const std::array canvasBackgrounds{
        sf::Color(5, 8, 20),
        sf::Color(24, 7, 24),
        sf::Color(3, 18, 20)
    };
    const std::array<std::array<sf::Color, 5>, 3> canvasPalettes{{
        {{sf::Color(80, 210, 255), sf::Color(120, 110, 255),
          sf::Color(255, 90, 190), sf::Color(80, 255, 190),
          sf::Color(245, 245, 255)}},
        {{sf::Color(255, 105, 95), sf::Color(255, 170, 70),
          sf::Color(255, 75, 155), sf::Color(170, 80, 255),
          sf::Color(255, 225, 150)}},
        {{sf::Color(70, 255, 185), sf::Color(60, 190, 255),
          sf::Color(185, 100, 255), sf::Color(210, 255, 110),
          sf::Color(235, 255, 250)}}
    }};

    std::vector<CircleView> bodies;
    std::vector<CircleView> restartBodies;
    std::vector<physics::Spring> springs;
    std::vector<physics::Spring> restartSprings;

    const auto loadClassicScene = [&]()
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        bodies.emplace_back(
            5.f,
            sf::Vector2f(100.f, 100.f),
            sf::Vector2f(180.f, 0.f),
            0.75f
        );
        bodies.emplace_back(
            5.f,
            sf::Vector2f(300.f, 80.f),
            sf::Vector2f(-100.f, 0.f),
            0.55f
        );
        bodies.emplace_back(
            5.f,
            sf::Vector2f(500.f, 50.f),
            sf::Vector2f(70.f, 0.f),
            0.9f
        );
        restartBodies = bodies;
    };

    loadClassicScene();

    const auto loadStressScene = [&](std::size_t bodyCount)
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        constexpr float radius = 5.f;
        constexpr float spacing = 15.f;
        constexpr std::size_t columns = 50;

        for (std::size_t index = 0; index < bodyCount; ++index)
        {
            const float x = 20.f +
                static_cast<float>(index % columns) * spacing;
            const float y = 40.f +
                static_cast<float>(index / columns) * spacing;
            const float horizontalVelocity = index % 2 == 0 ? 35.f : -35.f;
            bodies.emplace_back(
                radius,
                sf::Vector2f(x, y),
                sf::Vector2f(horizontalVelocity, 0.f),
                0.65f
            );
        }
        restartBodies = bodies;
    };

    const auto loadHeadOnScene = [&]()
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        bodies.emplace_back(
            30.f, sf::Vector2f(180.f, 270.f),
            sf::Vector2f(220.f, 0.f), 1.f
        );
        bodies.emplace_back(
            30.f, sf::Vector2f(560.f, 270.f),
            sf::Vector2f(-220.f, 0.f), 1.f
        );
        restartBodies = bodies;
    };

    const auto loadZeroGravityScene = [&]()
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        for (std::size_t index = 0; index < 16; ++index)
        {
            const float x = 80.f + static_cast<float>(index % 4) * 180.f;
            const float y = 70.f + static_cast<float>(index / 4) * 130.f;
            const float vx = index % 2 == 0 ? 90.f : -90.f;
            const float vy = index % 3 == 0 ? 70.f : -50.f;
            bodies.emplace_back(
                14.f + static_cast<float>(index % 3) * 4.f,
                sf::Vector2f(x, y), sf::Vector2f(vx, vy), 0.95f
            );
        }
        restartBodies = bodies;
    };

    const auto loadRainScene = [&]()
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        for (std::size_t index = 0; index < 60; ++index)
        {
            const float x = 15.f + static_cast<float>(index % 20) * 39.f;
            const float y = 20.f + static_cast<float>(index / 20) * 28.f;
            bodies.emplace_back(
                7.f, sf::Vector2f(x, y), sf::Vector2f(0.f, 0.f), 0.7f
            );
        }
        restartBodies = bodies;
    };

    const auto loadOrbitScene = [&]()
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        constexpr float PI = 3.14159265359f;
        constexpr float fieldStrength = 8000000.f;
        const sf::Vector2f fieldCenter(400.f, 300.f);

        for (std::size_t index = 0; index < 120; ++index)
        {
            const float radiusFromCenter =
                75.f + static_cast<float>(index % 6) * 42.f;
            const float angle = 2.f * PI * static_cast<float>(index) / 120.f;
            const sf::Vector2f radial(std::cos(angle), std::sin(angle));
            const float bodyRadius = 4.f + static_cast<float>(index % 4);
            const sf::Vector2f center = fieldCenter + radial * radiusFromCenter;
            const float orbitalSpeed = std::sqrt(fieldStrength / radiusFromCenter);
            const sf::Vector2f tangent(-radial.y, radial.x);
            bodies.emplace_back(
                bodyRadius,
                center - sf::Vector2f(bodyRadius, bodyRadius),
                tangent * orbitalSpeed,
                0.9f
            );
        }
        restartBodies = bodies;
    };

    sf::Clock clock;
    float accumulator = 0.f;
    float simulationTime = 0.f;
    bool paused = false;
    bool stepRequested = false;
    std::optional<std::size_t> selectedBody;
    std::optional<std::size_t> draggedBody;
    float selectedInverseMass = 0.f;
    sf::Vector2f dragOffset;
    sf::Vector2f lastMousePosition;
    sf::Vector2f throwVelocity;
    float gravityField[2] = {0.f, 0.f};
    float windForce[2] = {0.f, 0.f};
    bool gravityEnabled = true;
    bool windEnabled = true;
    float windSensitivity = 1.f;
    bool canvasMode = false;
    bool canvasTrails = true;
    bool showControls = true;
    bool placingPointField = false;
    bool draggingPointField = false;
    bool cuttingConnections = false;
    std::size_t canvasPalette = 0;
    std::vector<PointField> pointFields;
    std::optional<std::size_t> selectedPointField;
    int interactionTool = 0;
    float pulseRadialStrength = 8000000.f;
    float pulseVortexStrength = 0.f;
    float pulseDuration = 1.f;
    bool pulseChargeSensitive = false;
    bool colorByCharge = false;
    bool colorByMaterial = false;
    constexpr float FIELD_SOFTENING = 35.f;
    float floorFriction = 0.98f;
    float spawnRadius = 5.f;
    float spawnMass = spawnRadius * spawnRadius;
    bool automaticSpawnMass = true;
    float spawnRestitution = 0.75f;
    float spawnCharge = 0.f;
    std::size_t populationCount = 300;
    float populationRadii[3] = {5.f, 12.f, 24.f};
    float populationPercentages[3] = {100.f, 0.f, 0.f};
    float chargePercentages[3] = {25.f, 50.f, 25.f};
    float framesPerSecond = 0.f;
    bool showVelocityVectors = false;
    bool showContacts = false;
    bool showCollisionNormals = false;
    bool showSpatialGrid = false;
    bool bodyCollisionsEnabled = true;
    bool mutualElectrostaticsEnabled = false;
    float electrostaticStrength = 200000000.f;
    float electrostaticSoftening = 20.f;
    std::optional<std::size_t> firstSpringBody;
    float newSpringStiffness = 8000.f;
    float newSpringDamping = 300.f;
    bool useDistanceConstraints = false;
    int constraintIterations = 8;
    float constraintStiffness = 0.9f;
    bool breakableSprings = false;
    float springBreakingStrain = 0.65f;
    std::size_t brokenSpringCount = 0;
    std::size_t cutConnectionCount = 0;
    float connectionCutRadius = 18.f;
    int generatedChainCount = 80;
    int generatedLatticeColumns = 12;
    int generatedLatticeRows = 8;
    int generatedWebRings = 6;
    int generatedWebSpokes = 16;
    float generatedStructureSpacing = 20.f;
    bool useSpatialGrid = false;
    float gridCellSize = 50.f;
    std::vector<DebugContact> debugContacts;
    std::size_t allPairsCount = 0;
    std::size_t candidatePairChecks = 0;
    std::size_t actualCollisions = 0;
    std::size_t electrostaticPairChecks = 0;
    double physicsStepMilliseconds = 0.0;

    const auto pointFieldAcceleration = [&](const CircleView& view)
    {
        const auto& body = view.body;
        sf::Vector2f totalAcceleration(0.f, 0.f);
        for (const auto& field : pointFields)
        {
            if (!field.enabled)
            {
                continue;
            }

            const sf::Vector2f difference = field.position - body.center();
            const float softenedDistanceSquared =
                difference.x * difference.x + difference.y * difference.y +
                FIELD_SOFTENING * FIELD_SOFTENING;
            const sf::Vector2f direction =
                difference / std::sqrt(softenedDistanceSquared);
            const sf::Vector2f tangent(-direction.y, direction.x);
            constexpr float PI = 3.14159265359f;
            const float oscillation = 1.f + field.oscillationAmount *
                std::sin(2.f * PI * field.oscillationFrequency * simulationTime);
            constexpr float VISUAL_FIELD_RESPONSE = 12.f;
            const float response = (field.chargeSensitive
                ? body.charge * body.inverseMass * 400.f
                : VISUAL_FIELD_RESPONSE) *
                materialForceResponse(view.material);
            totalAcceleration +=
                direction * (field.radialStrength * oscillation * response /
                    softenedDistanceSquared) +
                tangent * (field.vortexStrength * oscillation * response /
                    softenedDistanceSquared);
        }
        return totalAcceleration;
    };

    const auto windAcceleration = [&](const CircleView& view)
    {
        const auto& body = view.body;
        if (!windEnabled || body.inverseMass <= 0.f)
        {
            return sf::Vector2f(0.f, 0.f);
        }

        // In 2D, exposed width grows with radius while default mass grows
        // with radius squared. Small circles therefore respond more strongly.
        const float aerodynamicResponse =
            2.f * body.radius * body.inverseMass * windSensitivity *
            materialForceResponse(view.material);
        return sf::Vector2f(windForce[0], windForce[1]) * aerodynamicResponse;
    };

    const auto loadPopulation = [&](std::size_t bodyCount)
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        populationCount = bodyCount;
        const float totalPercentage = std::max(
            populationPercentages[0] + populationPercentages[1] +
                populationPercentages[2],
            0.001f
        );
        const float smallBoundary =
            populationPercentages[0] / totalPercentage;
        const float mediumBoundary =
            (populationPercentages[0] + populationPercentages[1]) /
            totalPercentage;
        const float totalChargePercentage = std::max(
            chargePercentages[0] + chargePercentages[1] +
                chargePercentages[2],
            0.001f
        );
        const float positiveBoundary =
            chargePercentages[0] / totalChargePercentage;
        const float neutralBoundary =
            (chargePercentages[0] + chargePercentages[1]) /
            totalChargePercentage;

        for (std::size_t index = 0; index < bodyCount; ++index)
        {
            const std::size_t mixed = index * static_cast<std::size_t>(2654435761U);
            const float sample =
                static_cast<float>(mixed % 10000) / 10000.f;
            const std::size_t type = sample < smallBoundary
                ? 0
                : (sample < mediumBoundary ? 1 : 2);
            const float radius = populationRadii[type];
            const float availableWidth = windowWidth - radius * 2.f;
            const float availableHeight = windowHeight - radius * 2.f;
            const float x = std::fmod(
                17.f + static_cast<float>(index * 97),
                std::max(availableWidth, 1.f)
            );
            const float y = std::fmod(
                23.f + static_cast<float>(index * 53),
                std::max(availableHeight, 1.f)
            );
            const float vx = index % 2 == 0 ? 35.f : -35.f;
            bodies.emplace_back(
                radius,
                sf::Vector2f(x, y),
                sf::Vector2f(vx, 0.f),
                0.7f
            );
            const std::size_t chargeMix =
                index * static_cast<std::size_t>(2246822519U) + 3266489917U;
            const float chargeSample =
                static_cast<float>(chargeMix % 10000) / 10000.f;
            bodies.back().body.charge = chargeSample < positiveBoundary
                ? 1.f
                : (chargeSample < neutralBoundary ? 0.f : -1.f);
        }
        restartBodies = bodies;
        pointFields.clear();
        selectedPointField.reset();
        placingPointField = false;
        draggingPointField = false;
        gravityEnabled = true;
        gravityField[0] = 0.f;
        gravityField[1] = 0.f;
        windEnabled = true;
        windForce[0] = 0.f;
        windForce[1] = 0.f;
        useSpatialGrid = bodyCount >= 300;
        gridCellSize = bodyCount >= 600 ? 20.f : 50.f;
        bodyCollisionsEnabled = bodyCount < 600;
        mutualElectrostaticsEnabled = false;
        selectedBody.reset();
        draggedBody.reset();
        accumulator = 0.f;
    };

    const auto loadSpringChain = [&](std::size_t bodyCount)
    {
        bodies.clear();
        springs.clear();
        useDistanceConstraints = false;
        breakableSprings = false;
        brokenSpringCount = 0;
        cutConnectionCount = 0;
        constexpr float RADIUS = 5.f;
        const float spacing = generatedStructureSpacing;
        const std::size_t columns = std::max(
            static_cast<std::size_t>((windowWidth - 80.f) / spacing),
            static_cast<std::size_t>(2)
        );
        const std::size_t rowCount = (bodyCount + columns - 1) / columns;
        const float rowSpacing = rowCount > 1
            ? std::min(spacing, 480.f / static_cast<float>(rowCount - 1))
            : spacing;
        for (std::size_t index = 0; index < bodyCount; ++index)
        {
            const std::size_t row = index / columns;
            const std::size_t columnInRow = index % columns;
            const std::size_t column = row % 2 == 0
                ? columnInRow
                : columns - 1 - columnInRow;
            bodies.emplace_back(
                RADIUS,
                sf::Vector2f(
                    35.f + static_cast<float>(column) * spacing,
                    55.f + static_cast<float>(row) * rowSpacing
                ),
                sf::Vector2f(0.f, 0.f),
                0.5f
            );
            if (index > 0)
            {
                const sf::Vector2f difference =
                    bodies[index].body.center() -
                    bodies[index - 1].body.center();
                const float restLength = std::sqrt(
                    difference.x * difference.x + difference.y * difference.y
                );
                springs.push_back({
                    index - 1,
                    index,
                    restLength,
                    newSpringStiffness,
                    newSpringDamping
                });
            }
        }
        bodies.front().body.inverseMass = 0.f;
        restartBodies = bodies;
        restartSprings = springs;
        selectedBody.reset();
        draggedBody.reset();
        firstSpringBody.reset();
        gravityEnabled = true;
        gravityField[0] = 0.f;
        gravityField[1] = 0.f;
        windEnabled = false;
        pointFields.clear();
        selectedPointField.reset();
        mutualElectrostaticsEnabled = false;
        bodyCollisionsEnabled = false;
        accumulator = 0.f;
        simulationTime = 0.f;
    };

    const auto loadSoftBodyLattice = [&](std::size_t columns, std::size_t rows)
    {
        bodies.clear();
        springs.clear();
        useDistanceConstraints = false;
        breakableSprings = false;
        brokenSpringCount = 0;
        cutConnectionCount = 0;
        constexpr float RADIUS = 5.f;
        const float spacing = std::min({
            generatedStructureSpacing,
            (windowWidth - 20.f) / static_cast<float>(columns - 1),
            (windowHeight - 20.f) / static_cast<float>(rows - 1)
        });
        const float startX = (windowWidth -
            static_cast<float>(columns - 1) * spacing) * 0.5f - RADIUS;
        const float startY = (windowHeight -
            static_cast<float>(rows - 1) * spacing) * 0.5f - RADIUS;
        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t column = 0; column < columns; ++column)
            {
                bodies.emplace_back(
                    RADIUS,
                    sf::Vector2f(
                        startX + static_cast<float>(column) * spacing,
                        startY + static_cast<float>(row) * spacing
                    ),
                    sf::Vector2f(0.f, 0.f),
                    0.35f
                );
            }
        }
        const auto connect = [&](std::size_t first, std::size_t second)
        {
            const sf::Vector2f difference =
                bodies[second].body.center() - bodies[first].body.center();
            springs.push_back({
                first,
                second,
                std::sqrt(difference.x * difference.x + difference.y * difference.y),
                6500.f,
                260.f
            });
        };
        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t column = 0; column < columns; ++column)
            {
                const std::size_t index = row * columns + column;
                if (column + 1 < columns)
                {
                    connect(index, index + 1);
                }
                if (row + 1 < rows)
                {
                    connect(index, index + columns);
                }
                if (row + 1 < rows && column + 1 < columns)
                {
                    connect(index, index + columns + 1);
                }
                if (row + 1 < rows && column > 0)
                {
                    connect(index, index + columns - 1);
                }
            }
        }
        bodies[0].body.inverseMass = 0.f;
        bodies[columns - 1].body.inverseMass = 0.f;
        restartBodies = bodies;
        restartSprings = springs;
        firstSpringBody.reset();
        gravityEnabled = true;
        gravityField[0] = 0.f;
        gravityField[1] = 0.f;
        windEnabled = false;
        pointFields.clear();
        mutualElectrostaticsEnabled = false;
        bodyCollisionsEnabled = false;
        accumulator = 0.f;
        simulationTime = 0.f;
    };

    const auto loadMaterialRegions = [&]()
    {
        constexpr std::size_t COLUMNS = 30;
        constexpr std::size_t ROWS = 20;
        loadSoftBodyLattice(COLUMNS, ROWS);
        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            const std::size_t row = index / COLUMNS;
            if (row < ROWS / 3)
            {
                bodies[index].material = ParticleMaterial::Structural;
                bodies[index].groupId = 1;
            }
            else if (row < 2 * ROWS / 3)
            {
                bodies[index].material = ParticleMaterial::Flexible;
                bodies[index].groupId = 2;
            }
            else
            {
                bodies[index].material = ParticleMaterial::Fragile;
                bodies[index].groupId = 3;
            }
        }
        bodies[0].material = ParticleMaterial::Fixed;
        bodies[COLUMNS - 1].material = ParticleMaterial::Fixed;
        useDistanceConstraints = true;
        breakableSprings = true;
        colorByMaterial = true;
        colorByCharge = false;
        windEnabled = true;
        windForce[0] = 700.f;
        windForce[1] = 0.f;
        restartBodies = bodies;
        restartSprings = springs;
    };

    const auto loadParticleApple = [&]()
    {
        constexpr std::size_t COLUMNS = 37;
        constexpr std::size_t ROWS = 39;
        constexpr float RADIUS = 1.9f;
        constexpr float PI = 3.14159265359f;
        const sf::Vector2f center(windowWidth * 0.5f, windowHeight * 0.52f);

        bodies.clear();
        springs.clear();
        std::vector<std::optional<std::size_t>> particleAt(COLUMNS * ROWS);

        for (std::size_t row = 0; row < ROWS; ++row)
        {
            for (std::size_t column = 0; column < COLUMNS; ++column)
            {
                const float v = -0.94f + 1.88f *
                    static_cast<float>(row) / static_cast<float>(ROWS - 1);
                const float u = -1.f + 2.f *
                    static_cast<float>(column) /
                    static_cast<float>(COLUMNS - 1);

                // Sine projection compresses particles near the silhouette,
                // like longitude lines curving around a three-dimensional form.
                const float projectedU = std::sin(u * PI * 0.5f);
                const float projectedV = std::sin(v * PI * 0.5f);
                const float roundness = std::sqrt(std::max(
                    0.f, 1.f - projectedV * projectedV
                ));
                // Width samples follow a front-view apple reference: a
                // shallow stem cavity, full upper shoulders, long rounded
                // sides, then a quicker taper around the blossom end.
                constexpr std::array<std::pair<float, float>, 10> profile{{
                    {-1.00f, 0.12f}, {-0.88f, 0.58f}, {-0.68f, 0.88f},
                    {-0.38f, 1.00f}, {-0.05f, 0.98f}, {0.28f, 0.93f},
                    {0.55f, 0.80f}, {0.76f, 0.61f}, {0.92f, 0.31f},
                    {1.00f, 0.10f}
                }};
                float widthScale = profile.back().second;
                for (std::size_t sample = 1; sample < profile.size(); ++sample)
                {
                    if (projectedV <= profile[sample].first)
                    {
                        const auto [previousV, previousWidth] =
                            profile[sample - 1];
                        const auto [nextV, nextWidth] = profile[sample];
                        const float amount = (projectedV - previousV) /
                            (nextV - previousV);
                        widthScale = previousWidth +
                            amount * (nextWidth - previousWidth);
                        break;
                    }
                }
                const float halfWidth = 174.f * widthScale;
                const float frontBulge =
                    std::cos(u * PI * 0.5f) * roundness;
                const float topNotch = projectedV < -0.78f
                    ? 11.f * std::pow(1.f - std::abs(u), 3.f) *
                        (-projectedV - 0.78f) / 0.22f
                    : 0.f;
                const float bottomDimple = projectedV > 0.86f
                    ? -5.f * std::pow(1.f - std::abs(u), 3.f) *
                        (projectedV - 0.86f) / 0.14f
                    : 0.f;
                const sf::Vector2f particleCenter(
                    center.x + projectedU * halfWidth + 8.f * frontBulge,
                    center.y + projectedV * 180.f + topNotch + bottomDimple -
                        6.f * frontBulge
                );

                const std::size_t bodyIndex = bodies.size();
                bodies.emplace_back(
                    RADIUS,
                    particleCenter - sf::Vector2f(RADIUS, RADIUS),
                    sf::Vector2f(0.f, 0.f),
                    0.25f
                );
                particleAt[row * COLUMNS + column] = bodyIndex;
                bodies.back().material = ParticleMaterial::Flexible;
                bodies.back().groupId = 1;
            }
        }

        // A narrow structural stem supplies the identifying detail without
        // relying on a painted fruit colour.
        std::optional<std::size_t> previousStem;
        for (std::size_t index = 0; index < 9; ++index)
        {
            const float t = static_cast<float>(index);
            bodies.emplace_back(
                RADIUS,
                center + sf::Vector2f(5.f + t * 1.6f, -190.f - t * 7.f) -
                    sf::Vector2f(RADIUS, RADIUS),
                sf::Vector2f(0.f, 0.f),
                0.25f
            );
            bodies.back().material = index == 8
                ? ParticleMaterial::Fixed
                : ParticleMaterial::Structural;
            bodies.back().groupId = 2;
            if (index == 8)
            {
                bodies.back().body.inverseMass = 0.f;
            }
            if (previousStem)
            {
                const auto first = *previousStem;
                const auto second = bodies.size() - 1;
                const sf::Vector2f difference =
                    bodies[second].body.center() - bodies[first].body.center();
                springs.push_back({first, second,
                    std::sqrt(difference.x * difference.x + difference.y * difference.y),
                    6500.f, 260.f});
            }
            previousStem = bodies.size() - 1;
        }

        const auto connect = [&](std::size_t first, std::size_t second)
        {
            const sf::Vector2f difference =
                bodies[second].body.center() - bodies[first].body.center();
            springs.push_back({
                first,
                second,
                std::sqrt(difference.x * difference.x +
                    difference.y * difference.y),
                6500.f,
                260.f
            });
        };
        for (std::size_t row = 0; row < ROWS; ++row)
        {
            for (std::size_t column = 0; column < COLUMNS; ++column)
            {
                const auto current = particleAt[row * COLUMNS + column];
                if (!current)
                {
                    continue;
                }
                const auto connectIfPresent = [&](std::size_t otherRow,
                                                  std::size_t otherColumn)
                {
                    const auto other =
                        particleAt[otherRow * COLUMNS + otherColumn];
                    if (other)
                    {
                        connect(*current, *other);
                    }
                };
                if (column + 1 < COLUMNS)
                {
                    connectIfPresent(row, column + 1);
                }
                if (row + 1 < ROWS)
                {
                    connectIfPresent(row + 1, column);
                    if (column + 1 < COLUMNS)
                    {
                        connectIfPresent(row + 1, column + 1);
                    }
                    if (column > 0)
                    {
                        connectIfPresent(row + 1, column - 1);
                    }
                }
            }
        }
        connect(*particleAt[COLUMNS / 2], COLUMNS * ROWS);

        useDistanceConstraints = true;
        constraintIterations = 10;
        constraintStiffness = 0.85f;
        breakableSprings = false;
        brokenSpringCount = 0;
        cutConnectionCount = 0;
        colorByCharge = false;
        colorByMaterial = true;
        gravityEnabled = true;
        gravityField[0] = 0.f;
        gravityField[1] = 0.f;
        windEnabled = true;
        windForce[0] = 0.f;
        windForce[1] = 0.f;
        pointFields.clear();
        selectedPointField.reset();
        mutualElectrostaticsEnabled = false;
        bodyCollisionsEnabled = false;
        selectedBody.reset();
        draggedBody.reset();
        restartBodies = bodies;
        restartSprings = springs;
        accumulator = 0.f;
        simulationTime = 0.f;
    };

    const auto loadRadialWeb = [&](std::size_t ringCount,
                                   std::size_t spokeCount)
    {
        bodies.clear();
        springs.clear();
        useDistanceConstraints = false;
        breakableSprings = false;
        brokenSpringCount = 0;
        cutConnectionCount = 0;
        constexpr float PI = 3.14159265359f;
        const sf::Vector2f center(400.f, 270.f);
        const float ringSpacing = 220.f / static_cast<float>(ringCount);
        bodies.emplace_back(
            6.f, center - sf::Vector2f(6.f, 6.f),
            sf::Vector2f(0.f, 0.f), 0.4f
        );
        bodies[0].body.inverseMass = 0.f;

        for (std::size_t ring = 0; ring < ringCount; ++ring)
        {
            const float distance = ringSpacing * static_cast<float>(ring + 1);
            for (std::size_t spoke = 0; spoke < spokeCount; ++spoke)
            {
                const float angle = 2.f * PI * static_cast<float>(spoke) /
                    static_cast<float>(spokeCount);
                const sf::Vector2f radial(std::cos(angle), std::sin(angle));
                const sf::Vector2f tangent(-radial.y, radial.x);
                bodies.emplace_back(
                    4.f,
                    center + radial * distance - sf::Vector2f(4.f, 4.f),
                    tangent * (18.f + static_cast<float>(ring) * 4.f),
                    0.4f
                );
            }
        }

        const auto connect = [&](std::size_t first, std::size_t second)
        {
            const sf::Vector2f difference =
                bodies[second].body.center() - bodies[first].body.center();
            springs.push_back({
                first,
                second,
                std::sqrt(difference.x * difference.x + difference.y * difference.y),
                9000.f,
                190.f
            });
        };
        for (std::size_t ring = 0; ring < ringCount; ++ring)
        {
            const std::size_t ringStart = 1 + ring * spokeCount;
            for (std::size_t spoke = 0; spoke < spokeCount; ++spoke)
            {
                connect(ringStart + spoke,
                    ringStart + (spoke + 1) % spokeCount);
                if (ring == 0)
                {
                    connect(0, ringStart + spoke);
                }
                else
                {
                    connect(ringStart - spokeCount + spoke, ringStart + spoke);
                }
            }
        }

        restartBodies = bodies;
        restartSprings = springs;
        firstSpringBody.reset();
        gravityEnabled = false;
        windEnabled = false;
        pointFields.clear();
        mutualElectrostaticsEnabled = false;
        bodyCollisionsEnabled = false;
        accumulator = 0.f;
        simulationTime = 0.f;
    };

    const auto deleteBody = [&](std::size_t bodyIndex)
    {
        springs.erase(
            std::remove_if(
                springs.begin(), springs.end(),
                [bodyIndex](const physics::Spring& spring)
                {
                    return spring.first == bodyIndex ||
                        spring.second == bodyIndex;
                }
            ),
            springs.end()
        );
        for (auto& spring : springs)
        {
            if (spring.first > bodyIndex)
            {
                --spring.first;
            }
            if (spring.second > bodyIndex)
            {
                --spring.second;
            }
        }
        bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(bodyIndex));
        selectedBody.reset();
        draggedBody.reset();
        firstSpringBody.reset();
    };

    const auto cutConnectionsAt = [&](sf::Vector2f position)
    {
        const std::size_t connectionCountBefore = springs.size();
        const float radiusSquared =
            connectionCutRadius * connectionCutRadius;
        springs.erase(
            std::remove_if(
                springs.begin(), springs.end(),
                [&](const physics::Spring& connection)
                {
                    if (connection.first >= bodies.size() ||
                        connection.second >= bodies.size())
                    {
                        return false;
                    }
                    return distanceSquaredToSegment(
                        position,
                        bodies[connection.first].body.center(),
                        bodies[connection.second].body.center()
                    ) <= radiusSquared;
                }
            ),
            springs.end()
        );
        cutConnectionCount += connectionCountBefore - springs.size();
    };

    const auto rebalancePercentages = [](
        float percentages[3],
        std::size_t changedIndex
    )
    {
        const std::size_t firstOther = (changedIndex + 1) % 3;
        const std::size_t secondOther = (changedIndex + 2) % 3;
        const float remaining = 100.f - percentages[changedIndex];
        const float previousOtherTotal =
            percentages[firstOther] + percentages[secondOther];

        if (previousOtherTotal > 0.001f)
        {
            percentages[firstOther] = remaining *
                percentages[firstOther] / previousOtherTotal;
        }
        else
        {
            percentages[firstOther] = remaining * 0.5f;
        }
        percentages[secondOther] = remaining - percentages[firstOther];
    };

    const auto drawSizeMixtureControls = [&]()
    {
        if (!ImGui::TreeNode("Size mixture"))
        {
            return;
        }

        constexpr const char* TYPE_NAMES[3] = {
            "Small balls", "Medium balls", "Large balls"
        };
        constexpr float MIN_RADII[3] = {2.f, 6.f, 12.f};
        constexpr float MAX_RADII[3] = {15.f, 30.f, 50.f};

        for (std::size_t index = 0; index < 3; ++index)
        {
            ImGui::PushID(static_cast<int>(index));
            ImGui::TextUnformatted(TYPE_NAMES[index]);
            ImGui::Indent();
            ImGui::SliderFloat(
                "Radius", &populationRadii[index],
                MIN_RADII[index], MAX_RADII[index], "%.0f px"
            );
            if (ImGui::SliderFloat(
                    "Population", &populationPercentages[index],
                    0.f, 100.f, "%.0f%%"
                ))
            {
                rebalancePercentages(populationPercentages, index);
            }
            ImGui::Unindent();
            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::TextDisabled("Total: 100%% (adjusted automatically)");
        if (ImGui::Button("Regenerate mixture"))
        {
            loadPopulation(populationCount);
        }
        ImGui::TreePop();
    };

    const auto drawChargeMixtureControls = [&]()
    {
        if (!ImGui::TreeNode("Charge mixture"))
        {
            return;
        }
        constexpr const char* LABELS[3] = {
            "Positive (+1)", "Neutral (0)", "Negative (-1)"
        };
        for (std::size_t index = 0; index < 3; ++index)
        {
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::SliderFloat(
                    LABELS[index], &chargePercentages[index],
                    0.f, 100.f, "%.0f%%"
                ))
            {
                rebalancePercentages(chargePercentages, index);
            }
            ImGui::PopID();
        }
        ImGui::TextDisabled("Total: 100%% (adjusted automatically)");
        if (ImGui::Button("Regenerate charges"))
        {
            loadPopulation(populationCount);
        }
        ImGui::TreePop();
    };

    const auto simulateStep = [&]()
    {
        simulationTime += FIXED_TIME_STEP;
        const auto stepStart = std::chrono::steady_clock::now();
        candidatePairChecks = 0;
        actualCollisions = 0;
        electrostaticPairChecks = 0;
        allPairsCount = bodies.size() * (bodies.size() - (bodies.empty() ? 0 : 1)) / 2;

        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            if (draggedBody && index == *draggedBody)
            {
                continue;
            }

            bodies[index].body.acceleration = gravityEnabled
                ? sf::Vector2f(gravityField[0], gravityField[1])
                : sf::Vector2f(0.f, 0.f);
            bodies[index].body.acceleration +=
                pointFieldAcceleration(bodies[index]);
            bodies[index].body.acceleration +=
                windAcceleration(bodies[index]);
        }

        if (mutualElectrostaticsEnabled)
        {
            for (std::size_t first = 0; first < bodies.size(); ++first)
            {
                for (std::size_t second = first + 1;
                     second < bodies.size(); ++second)
                {
                    ++electrostaticPairChecks;
                    physics::applyElectrostaticPair(
                        bodies[first].body,
                        bodies[second].body,
                        electrostaticStrength,
                        electrostaticSoftening
                    );
                }
            }
        }

        if (!useDistanceConstraints)
        {
            for (const auto& spring : springs)
            {
                if (spring.first < bodies.size() &&
                    spring.second < bodies.size())
                {
                    physics::applySpringForce(
                        bodies[spring.first].body,
                        bodies[spring.second].body,
                        spring.restLength,
                        spring.stiffness,
                        spring.damping
                    );
                }
            }
        }

        std::vector<sf::Vector2f> previousPositions;
        if (useDistanceConstraints)
        {
            previousPositions.reserve(bodies.size());
            for (const auto& view : bodies)
            {
                previousPositions.push_back(view.body.position);
            }
        }

        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            if (draggedBody && index == *draggedBody)
            {
                continue;
            }
            bodies[index].body.integrate(FIXED_TIME_STEP);
        }

        if (breakableSprings)
        {
            const std::size_t connectionCountBefore = springs.size();
            springs.erase(
                std::remove_if(
                    springs.begin(), springs.end(),
                    [&](const physics::Spring& spring)
                    {
                        if (spring.first >= bodies.size() ||
                            spring.second >= bodies.size() ||
                            spring.restLength <= 0.f)
                        {
                            return false;
                        }
                        const sf::Vector2f difference =
                            bodies[spring.second].body.center() -
                            bodies[spring.first].body.center();
                        const float currentLength = std::sqrt(
                            difference.x * difference.x +
                            difference.y * difference.y
                        );
                        const float materialScale = std::min(
                            materialBreakingScale(
                                bodies[spring.first].material
                            ),
                            materialBreakingScale(
                                bodies[spring.second].material
                            )
                        );
                        return (currentLength - spring.restLength) /
                            spring.restLength >
                            springBreakingStrain * materialScale;
                    }
                ),
                springs.end()
            );
            brokenSpringCount += connectionCountBefore - springs.size();
        }

        if (useDistanceConstraints)
        {
            for (int iteration = 0; iteration < constraintIterations;
                 ++iteration)
            {
                for (const auto& connection : springs)
                {
                    if (connection.first < bodies.size() &&
                        connection.second < bodies.size())
                    {
                        physics::solveDistanceConstraint(
                            bodies[connection.first].body,
                            bodies[connection.second].body,
                            connection.restLength,
                            constraintStiffness
                        );
                    }
                }
            }
            for (std::size_t index = 0; index < bodies.size(); ++index)
            {
                if (bodies[index].body.inverseMass > 0.f &&
                    (!draggedBody || index != *draggedBody))
                {
                    bodies[index].body.velocity =
                        (bodies[index].body.position -
                         previousPositions[index]) / FIXED_TIME_STEP;
                }
            }
        }

        for (auto& view : bodies)
        {
            view.body.resolveBounds(
                windowWidth,
                windowHeight,
                floorFriction,
                MINIMUM_BOUNCE_SPEED
            );
        }

        const auto processPair = [&](std::size_t first, std::size_t second)
        {
            ++candidatePairChecks;
            const sf::Vector2f firstCenter = bodies[first].body.center();
            const sf::Vector2f secondCenter = bodies[second].body.center();
            const sf::Vector2f difference = secondCenter - firstCenter;
            const float distanceSquared =
                difference.x * difference.x + difference.y * difference.y;
            const float combinedRadius =
                bodies[first].body.radius + bodies[second].body.radius;

            if (distanceSquared <= combinedRadius * combinedRadius)
            {
                ++actualCollisions;
                const sf::Vector2f normal = distanceSquared > 0.00000001f
                    ? difference / std::sqrt(distanceSquared)
                    : sf::Vector2f(1.f, 0.f);
                debugContacts.push_back({
                    firstCenter + normal * bodies[first].body.radius,
                    normal
                });
            }

            physics::resolveCircleCollision(
                bodies[first].body,
                bodies[second].body
            );
        };

        if (bodyCollisionsEnabled && useSpatialGrid)
        {
            std::vector<physics::CircleBounds> bounds;
            bounds.reserve(bodies.size());
            for (const auto& view : bodies)
            {
                bounds.push_back({view.body.position, view.body.radius});
            }

            const auto pairs =
                physics::buildUniformGridPairs(bounds, gridCellSize);
            for (const auto& pair : pairs)
            {
                processPair(pair.first, pair.second);
            }
        }
        else if (bodyCollisionsEnabled)
        {
            for (std::size_t first = 0; first < bodies.size(); ++first)
            {
                for (std::size_t second = first + 1; second < bodies.size(); ++second)
                {
                    processPair(first, second);
                }
            }
        }

        for (std::size_t index = 0; index < pointFields.size();)
        {
            auto& field = pointFields[index];
            if (field.remainingLifetime >= 0.f)
            {
                field.remainingLifetime -= FIXED_TIME_STEP;
            }
            if (field.remainingLifetime < 0.f &&
                field.remainingLifetime > -0.5f)
            {
                pointFields.erase(pointFields.begin() +
                    static_cast<std::ptrdiff_t>(index));
                if (selectedPointField)
                {
                    if (*selectedPointField == index)
                    {
                        selectedPointField.reset();
                    }
                    else if (*selectedPointField > index)
                    {
                        --(*selectedPointField);
                    }
                }
                continue;
            }
            ++index;
        }

        const auto stepEnd = std::chrono::steady_clock::now();
        physicsStepMilliseconds =
            std::chrono::duration<double, std::milli>(stepEnd - stepStart)
                .count();
    };

    while (window.isOpen())
    {
        while (auto event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* keyPressed =
                    event->getIf<sf::Event::KeyPressed>())
            {
                if (ImGui::GetIO().WantCaptureKeyboard)
                {
                    continue;
                }

                if (keyPressed->scancode == sf::Keyboard::Scancode::Tab)
                {
                    canvasMode = !canvasMode;
                    canvasTexture.clear(canvasBackgrounds[canvasPalette]);
                    canvasTexture.display();
                    window.setTitle(canvasMode
                        ? "Canvas | Tab: Lab | C: Controls | T: Trails | P: Palette | Right-click: Field"
                        : "Laboratory | Tab: Canvas | C: Controls");
                }
                else if (keyPressed->scancode == sf::Keyboard::Scancode::C)
                {
                    showControls = !showControls;
                }
                else if (
                    canvasMode &&
                    keyPressed->scancode == sf::Keyboard::Scancode::T
                )
                {
                    canvasTrails = !canvasTrails;
                    canvasTexture.clear(canvasBackgrounds[canvasPalette]);
                    canvasTexture.display();
                }
                else if (
                    canvasMode &&
                    keyPressed->scancode == sf::Keyboard::Scancode::P
                )
                {
                    canvasPalette = (canvasPalette + 1) % canvasPalettes.size();
                    canvasTexture.clear(canvasBackgrounds[canvasPalette]);
                    canvasTexture.display();
                }
                else if (keyPressed->scancode == sf::Keyboard::Scancode::R)
                {
                    bodies = restartBodies;
                    springs = restartSprings;
                    pointFields.erase(
                        std::remove_if(
                            pointFields.begin(), pointFields.end(),
                            [](const PointField& field)
                            {
                                return field.remainingLifetime >= 0.f;
                            }
                        ),
                        pointFields.end()
                    );
                    selectedPointField.reset();
                    placingPointField = false;
                    draggingPointField = false;
                    selectedBody.reset();
                    draggedBody.reset();
                    cuttingConnections = false;
                    cutConnectionCount = 0;
                    paused = false;
                    accumulator = 0.f;
                    simulationTime = 0.f;
                    canvasTexture.clear(canvasBackgrounds[canvasPalette]);
                    canvasTexture.display();
                }
                else if (keyPressed->scancode == sf::Keyboard::Scancode::Space)
                {
                    paused = !paused;
                    accumulator = 0.f;
                }
                else if (
                    keyPressed->scancode == sf::Keyboard::Scancode::N && paused
                )
                {
                    stepRequested = true;
                }
                else if (
                    (keyPressed->scancode == sf::Keyboard::Scancode::Delete ||
                     keyPressed->scancode == sf::Keyboard::Scancode::Backspace) &&
                    selectedBody && *selectedBody < bodies.size()
                )
                {
                    deleteBody(*selectedBody);
                }
            }

            if (const auto* mousePressed =
                    event->getIf<sf::Event::MouseButtonPressed>())
            {
                const sf::Vector2f pressedPosition =
                    toWorldPosition(mousePressed->position);
                std::optional<std::size_t> hitField;
                for (std::size_t index = pointFields.size(); index > 0; --index)
                {
                    const sf::Vector2f distance =
                        pressedPosition - pointFields[index - 1].position;
                    if (distance.x * distance.x + distance.y * distance.y <=
                        40.f * 40.f)
                    {
                        hitField = index - 1;
                        break;
                    }
                }

                if (
                    interactionTool == 3 &&
                    hitField &&
                    mousePressed->button == sf::Mouse::Button::Left &&
                    !ImGui::GetIO().WantCaptureMouse
                )
                {
                    selectedPointField = hitField;
                    draggingPointField = true;
                }
                else if (
                    placingPointField &&
                    mousePressed->button == sf::Mouse::Button::Left &&
                    !ImGui::GetIO().WantCaptureMouse
                )
                {
                    pointFields.push_back({pressedPosition, 8000000.f, 0.f, true});
                    selectedPointField = pointFields.size() - 1;
                    placingPointField = false;
                    draggingPointField = true;
                }
                else if (
                    canvasMode &&
                    mousePressed->button == sf::Mouse::Button::Right &&
                    !ImGui::GetIO().WantCaptureMouse
                )
                {
                    if (selectedPointField && *selectedPointField < pointFields.size())
                    {
                        pointFields[*selectedPointField].position = pressedPosition;
                    }
                    else
                    {
                        pointFields.push_back({pressedPosition, 8000000.f, 0.f, true});
                        selectedPointField = pointFields.size() - 1;
                    }
                    placingPointField = false;
                }
                else if (mousePressed->button == sf::Mouse::Button::Left)
                {
                    if (ImGui::GetIO().WantCaptureMouse)
                    {
                        continue;
                    }

                    const sf::Vector2f mousePosition =
                        toWorldPosition(mousePressed->position);
                    if (interactionTool == 1)
                    {
                        pointFields.push_back({
                            mousePosition,
                            pulseRadialStrength,
                            pulseVortexStrength,
                            true,
                            0.f,
                            1.f,
                            pulseDuration,
                            pulseChargeSensitive
                        });
                    }
                    else if (interactionTool == 4)
                    {
                        cuttingConnections = true;
                        cutConnectionsAt(mousePosition);
                    }
                    else if (interactionTool == 0)
                    {
                        selectedBody = findBodyAt(bodies, mousePosition);
                        if (selectedBody)
                        {
                            draggedBody = selectedBody;
                            auto& selected = bodies[*selectedBody].body;
                            selectedInverseMass = selected.inverseMass;
                            selected.inverseMass = 0.f;
                            selected.velocity = sf::Vector2f(0.f, 0.f);
                            dragOffset = mousePosition - selected.position;
                            lastMousePosition = mousePosition;
                            throwVelocity = sf::Vector2f(0.f, 0.f);
                        }
                    }
                    else if (interactionTool == 2)
                    {
                        const float x = std::clamp(
                            mousePosition.x - spawnRadius,
                            0.f,
                            windowWidth - spawnRadius * 2.f
                        );
                        const float y = std::clamp(
                            mousePosition.y - spawnRadius,
                            0.f,
                            windowHeight - spawnRadius * 2.f
                        );
                        bodies.emplace_back(
                            spawnRadius,
                            sf::Vector2f(x, y),
                            sf::Vector2f(0.f, 0.f),
                            spawnRestitution
                        );
                        if (!automaticSpawnMass)
                        {
                            bodies.back().body.inverseMass = spawnMass > 0.f
                                ? 1.f / spawnMass
                                : 0.f;
                        }
                        bodies.back().body.charge = spawnCharge;
                    }
                }
            }

            if (const auto* mouseReleased =
                    event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (mouseReleased->button == sf::Mouse::Button::Left)
                {
                    cuttingConnections = false;
                }
                if (
                    mouseReleased->button == sf::Mouse::Button::Left &&
                    draggingPointField
                )
                {
                    draggingPointField = false;
                }
                else if (
                    mouseReleased->button == sf::Mouse::Button::Left &&
                    draggedBody && *draggedBody < bodies.size()
                )
                {
                    auto& selected = bodies[*draggedBody].body;
                    selected.inverseMass = selectedInverseMass;
                    selected.velocity = throwVelocity;
                    draggedBody.reset();
                }
            }
        }

        float frameTime = clock.restart().asSeconds();

        // Avoid trying to simulate an unbounded backlog after a long pause.
        if (frameTime > MAX_FRAME_TIME)
        {
            frameTime = MAX_FRAME_TIME;
        }

        ImGui::SFML::Update(window, sf::seconds(frameTime));
        if (frameTime > 0.f)
        {
            framesPerSecond = 0.9f * framesPerSecond + 0.1f / frameTime;
        }

        if (showControls)
        {
        ImGui::SetNextWindowPos(ImVec2(12.f, 12.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(315.f, 500.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Physics Controls");
        ImGui::Text("Status: %s", paused ? "Paused" : "Running");
        ImGui::Text("Bodies: %zu", bodies.size());
        ImGui::TextDisabled("Tab: change view  C: hide controls");

        if (canvasMode && ImGui::CollapsingHeader(
                "Canvas appearance",
                ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            ImGui::Checkbox("Motion trails", &canvasTrails);
            if (ImGui::Button("Next colour palette"))
            {
                canvasPalette = (canvasPalette + 1) % canvasPalettes.size();
                canvasTexture.clear(canvasBackgrounds[canvasPalette]);
                canvasTexture.display();
            }
            ImGui::TextDisabled("T: trails  P: palette");
        }

        if (ImGui::Button(paused ? "Resume" : "Pause"))
        {
            paused = !paused;
            accumulator = 0.f;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!paused);
        if (ImGui::Button("Step"))
        {
            stepRequested = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Restart"))
        {
            bodies = restartBodies;
            springs = restartSprings;
            brokenSpringCount = 0;
            cutConnectionCount = 0;
            cuttingConnections = false;
            pointFields.erase(
                std::remove_if(
                    pointFields.begin(), pointFields.end(),
                    [](const PointField& field)
                    {
                        return field.remainingLifetime >= 0.f;
                    }
                ),
                pointFields.end()
            );
            selectedPointField.reset();
            placingPointField = false;
            draggingPointField = false;
            selectedBody.reset();
            draggedBody.reset();
            accumulator = 0.f;
            simulationTime = 0.f;
            canvasTexture.clear(canvasBackgrounds[canvasPalette]);
            canvasTexture.display();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            bodies.clear();
            springs.clear();
            selectedBody.reset();
            draggedBody.reset();
        }

        if (ImGui::CollapsingHeader(
                "Body count",
                ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            if (ImGui::Button("100"))
            {
                loadPopulation(100);
            }
            ImGui::SameLine();
            if (ImGui::Button("300"))
            {
                loadPopulation(300);
            }
            ImGui::SameLine();
            if (ImGui::Button("600"))
            {
                loadPopulation(600);
            }
            ImGui::SameLine();
            if (ImGui::Button("1000"))
            {
                loadPopulation(1000);
            }
            ImGui::Checkbox("Body collisions", &bodyCollisionsEnabled);
            if (bodies.size() >= 1000 && bodyCollisionsEnabled)
            {
                ImGui::TextDisabled("Experimental: dense contacts may be unstable");
            }
            drawSizeMixtureControls();
            drawChargeMixtureControls();
        }

        if (ImGui::CollapsingHeader(
                "Environment",
                ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            ImGui::Checkbox("Gravity enabled", &gravityEnabled);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset gravity"))
            {
                gravityField[0] = 0.f;
                gravityField[1] = 0.f;
            }
            ImGui::BeginDisabled(!gravityEnabled);
            vectorPad("Gravity direction", gravityField, 1500.f);
            ImGui::EndDisabled();

            ImGui::Checkbox("Wind enabled", &windEnabled);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset wind"))
            {
                windForce[0] = 0.f;
                windForce[1] = 0.f;
            }
            ImGui::BeginDisabled(!windEnabled);
            vectorPad("Wind direction", windForce, 5000.f);
            ImGui::EndDisabled();
            ImGui::SliderFloat(
                "Wind sensitivity", &windSensitivity, 0.1f, 3.f, "%.1fx"
            );
            ImGui::TextDisabled("Small/light bodies respond more strongly");
        }

        if (ImGui::CollapsingHeader("Mutual electrostatics"))
        {
            ImGui::Checkbox(
                "Particles affect each other",
                &mutualElectrostaticsEnabled
            );
            ImGui::SliderFloat(
                "Electric strength",
                &electrostaticStrength,
                10000000.f,
                1000000000.f,
                "%.1e",
                ImGuiSliderFlags_Logarithmic
            );
            ImGui::SliderFloat(
                "Electric softening",
                &electrostaticSoftening,
                5.f,
                80.f,
                "%.0f px"
            );
            ImGui::Text("Pair checks: %zu", electrostaticPairChecks);
            if (mutualElectrostaticsEnabled && bodies.size() > 300)
            {
                ImGui::TextDisabled(
                    "Long-range all-pairs forces may be slow above 300 bodies"
                );
            }
        }

        if (ImGui::CollapsingHeader("Preset scenes"))
        {
            ImGui::TextDisabled("Classical physics");
            if (ImGui::Button("Classic"))
            {
                loadClassicScene();
                mutualElectrostaticsEnabled = false;
                gravityField[0] = 0.f;
                gravityField[1] = 0.f;
                windForce[0] = 0.f;
                windForce[1] = 0.f;
                gravityEnabled = true;
                windEnabled = true;
                pointFields.clear();
                selectedPointField.reset();
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Head-on"))
            {
                loadHeadOnScene();
                mutualElectrostaticsEnabled = false;
                gravityEnabled = false;
                windEnabled = false;
                pointFields.clear();
                selectedPointField.reset();
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Zero-G drift"))
            {
                loadZeroGravityScene();
                mutualElectrostaticsEnabled = false;
                gravityEnabled = false;
                windEnabled = false;
                pointFields.clear();
                selectedPointField.reset();
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
            if (ImGui::Button("Particle rain"))
            {
                loadRainScene();
                mutualElectrostaticsEnabled = false;
                gravityField[0] = 0.f;
                gravityField[1] = 700.f;
                windForce[0] = 0.f;
                windForce[1] = 0.f;
                gravityEnabled = true;
                windEnabled = true;
                pointFields.clear();
                selectedPointField.reset();
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Charge separation"))
            {
                loadPopulation(300);
                gravityEnabled = false;
                windEnabled = false;
                pointFields = {{
                    sf::Vector2f(400.f, 300.f),
                    8000000.f,
                    0.f,
                    true,
                    0.f,
                    1.f,
                    -1.f,
                    true
                }};
                selectedPointField = 0;
                colorByCharge = true;
                bodyCollisionsEnabled = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Mutual charges"))
            {
                loadPopulation(150);
                gravityEnabled = false;
                windEnabled = false;
                pointFields.clear();
                selectedPointField.reset();
                mutualElectrostaticsEnabled = true;
                colorByCharge = true;
                bodyCollisionsEnabled = false;
            }
            ImGui::TextDisabled("Connected systems");
            if (ImGui::Button("Spring chain"))
            {
                loadSpringChain(static_cast<std::size_t>(generatedChainCount));
            }
            ImGui::SameLine();
            if (ImGui::Button("Soft-body lattice"))
            {
                loadSoftBodyLattice(
                    static_cast<std::size_t>(generatedLatticeColumns),
                    static_cast<std::size_t>(generatedLatticeRows)
                );
            }
            ImGui::SameLine();
            if (ImGui::Button("Constraint lattice"))
            {
                loadSoftBodyLattice(20, 15);
                useDistanceConstraints = true;
            }
            if (ImGui::Button("Radial spring web"))
            {
                loadRadialWeb(
                    static_cast<std::size_t>(generatedWebRings),
                    static_cast<std::size_t>(generatedWebSpokes)
                );
            }
            ImGui::SameLine();
            if (ImGui::Button("Full-screen lattice"))
            {
                loadSoftBodyLattice(40, 30);
            }
            ImGui::SameLine();
            if (ImGui::Button("Tearable lattice"))
            {
                loadSoftBodyLattice(40, 30);
                breakableSprings = true;
                brokenSpringCount = 0;
            }
            if (ImGui::Button("Material regions"))
            {
                loadMaterialRegions();
            }
            ImGui::TextDisabled("Particle artwork");
            if (ImGui::Button("Particle apple"))
            {
                loadParticleApple();
            }
            ImGui::TextDisabled("Point fields");
            if (ImGui::Button("Attractor"))
            {
                loadStressScene(300);
                mutualElectrostaticsEnabled = false;
                gravityEnabled = false;
                windEnabled = false;
                pointFields = {{sf::Vector2f(400.f, 300.f), 8000000.f, 0.f, true}};
                selectedPointField = 0;
                useSpatialGrid = true;
                gridCellSize = 50.f;
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Repulsor"))
            {
                loadStressScene(300);
                mutualElectrostaticsEnabled = false;
                gravityEnabled = false;
                windEnabled = false;
                pointFields = {{sf::Vector2f(400.f, 300.f), -10000000.f, 0.f, true}};
                selectedPointField = 0;
                useSpatialGrid = true;
                gridCellSize = 50.f;
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Vortex##preset"))
            {
                loadStressScene(300);
                mutualElectrostaticsEnabled = false;
                gravityEnabled = false;
                windEnabled = false;
                pointFields = {{sf::Vector2f(400.f, 300.f), 4000000.f, 10000000.f, true}};
                selectedPointField = 0;
                useSpatialGrid = true;
                gridCellSize = 50.f;
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
            if (ImGui::Button("Orbital rings"))
            {
                loadOrbitScene();
                mutualElectrostaticsEnabled = false;
                gravityEnabled = false;
                windEnabled = false;
                pointFields = {{sf::Vector2f(400.f, 300.f), 8000000.f, 0.f, true}};
                selectedPointField = 0;
                useSpatialGrid = true;
                gridCellSize = 50.f;
                bodyCollisionsEnabled = true;
                selectedBody.reset();
                draggedBody.reset();
            }
        }

        if (ImGui::CollapsingHeader(
                "Interaction tool",
                ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            if (ImGui::RadioButton("Select / throw", &interactionTool, 0))
            {
                placingPointField = false;
                draggingPointField = false;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Force pulse", &interactionTool, 1))
            {
                placingPointField = false;
                draggingPointField = false;
            }
            if (ImGui::RadioButton("Spawn body", &interactionTool, 2))
            {
                placingPointField = false;
                draggingPointField = false;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Move fields", &interactionTool, 3))
            {
                placingPointField = false;
                draggingPointField = false;
            }
            if (ImGui::RadioButton("Cut connections", &interactionTool, 4))
            {
                placingPointField = false;
                draggingPointField = false;
                draggedBody.reset();
            }

            if (interactionTool != 4)
            {
                cuttingConnections = false;
            }

            if (interactionTool == 4)
            {
                ImGui::SliderFloat(
                    "Cut brush radius", &connectionCutRadius,
                    5.f, 60.f, "%.0f px"
                );
                ImGui::Text("Connections cut: %zu", cutConnectionCount);
                ImGui::TextDisabled(
                    "Drag through connection lines; R restores them"
                );
            }

            if (interactionTool == 1)
            {
                if (ImGui::SmallButton("Attract pulse"))
                {
                    pulseRadialStrength = 8000000.f;
                    pulseVortexStrength = 0.f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Repel pulse"))
                {
                    pulseRadialStrength = -8000000.f;
                    pulseVortexStrength = 0.f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Vortex pulse"))
                {
                    pulseRadialStrength = 2000000.f;
                    pulseVortexStrength = 10000000.f;
                }
                ImGui::SliderFloat(
                    "Pulse attract / repel", &pulseRadialStrength,
                    -20000000.f, 20000000.f, "%.1e"
                );
                ImGui::SliderFloat(
                    "Pulse vortex", &pulseVortexStrength,
                    -20000000.f, 20000000.f, "%.1e"
                );
                ImGui::SliderFloat(
                    "Pulse duration", &pulseDuration, 0.1f, 5.f, "%.1f s"
                );
                ImGui::Checkbox(
                    "Pulse responds to charge", &pulseChargeSensitive
                );
                ImGui::TextDisabled("Click the scene to apply a temporary field");
            }
        }

        if (ImGui::CollapsingHeader(
                "Selected body",
                ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            if (selectedBody && *selectedBody < bodies.size())
            {
                const auto& selected = bodies[*selectedBody].body;
                const float inverseMass = draggedBody
                    ? selectedInverseMass
                    : selected.inverseMass;
                const float mass = inverseMass > 0.f ? 1.f / inverseMass : 0.f;
                const sf::Vector2f center = selected.center();
                const sf::Vector2f netAcceleration =
                    (gravityEnabled
                        ? sf::Vector2f(gravityField[0], gravityField[1])
                        : sf::Vector2f(0.f, 0.f)) +
                    windAcceleration(bodies[*selectedBody]) +
                    pointFieldAcceleration(bodies[*selectedBody]);
                ImGui::Text("Mass: %.1f", mass);
                ImGui::Text("Position: (%.1f, %.1f)", center.x, center.y);
                ImGui::Text(
                    "Velocity: (%.1f, %.1f)",
                    selected.velocity.x,
                    selected.velocity.y
                );
                ImGui::Text(
                    "Net acceleration: (%.1f, %.1f)",
                    netAcceleration.x,
                    netAcceleration.y
                );
                ImGui::Text(
                    "Radius: %.1f  Restitution: %.2f",
                    selected.radius,
                    selected.restitution
                );
                ImGui::Text("Charge: %+.1f", selected.charge);
                ImGui::Text(
                    "Material: %s  Group: %d",
                    materialName(bodies[*selectedBody].material),
                    bodies[*selectedBody].groupId
                );
                constexpr const char* MATERIAL_NAMES[] = {
                    "Structural", "Flexible", "Fragile", "Fixed"
                };
                int materialIndex = static_cast<int>(
                    bodies[*selectedBody].material
                );
                if (ImGui::Combo(
                        "Particle material", &materialIndex,
                        MATERIAL_NAMES, 4
                    ))
                {
                    auto& selectedView = bodies[*selectedBody];
                    selectedView.material = static_cast<ParticleMaterial>(
                        materialIndex
                    );
                    if (selectedView.material == ParticleMaterial::Fixed)
                    {
                        selectedView.body.inverseMass = 0.f;
                    }
                    else if (selectedView.body.inverseMass == 0.f)
                    {
                        selectedView.body.inverseMass = 1.f /
                            (selectedView.body.radius * selectedView.body.radius);
                    }
                    restartBodies = bodies;
                }
                if (ImGui::InputInt(
                        "Particle group",
                        &bodies[*selectedBody].groupId
                    ))
                {
                    bodies[*selectedBody].groupId = std::max(
                        bodies[*selectedBody].groupId, 0
                    );
                    restartBodies = bodies;
                }
                ImGui::Text("Pinned: %s", inverseMass == 0.f ? "yes" : "no");
                if (inverseMass > 0.f)
                {
                    if (ImGui::Button("Pin selected"))
                    {
                        bodies[*selectedBody].body.inverseMass = 0.f;
                        bodies[*selectedBody].material =
                            ParticleMaterial::Fixed;
                        restartBodies = bodies;
                    }
                }
                else if (ImGui::Button("Unpin selected"))
                {
                    const float radius = bodies[*selectedBody].body.radius;
                    bodies[*selectedBody].body.inverseMass =
                        1.f / (radius * radius);
                    if (bodies[*selectedBody].material ==
                        ParticleMaterial::Fixed)
                    {
                        bodies[*selectedBody].material =
                            ParticleMaterial::Flexible;
                    }
                    restartBodies = bodies;
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete selected"))
                {
                    deleteBody(*selectedBody);
                }
            }
            else
            {
                ImGui::TextDisabled("Click a body to inspect it");
            }
        }

        if (ImGui::CollapsingHeader("Spawn and materials"))
        {
            ImGui::SliderFloat("Spawn radius", &spawnRadius, 6.f, 60.f, "%.0f px");
            ImGui::Checkbox("Automatic mass from radius", &automaticSpawnMass);
            if (automaticSpawnMass)
            {
                ImGui::Text("Spawn mass: %.1f", spawnRadius * spawnRadius);
            }
            else
            {
                ImGui::SliderFloat("Spawn mass", &spawnMass, 0.f, 5000.f, "%.1f");
                ImGui::TextDisabled("Mass 0 creates a static body");
            }
            ImGui::SliderFloat("Restitution", &spawnRestitution, 0.f, 1.f, "%.2f");
            ImGui::SliderFloat("Charge", &spawnCharge, -1.f, 1.f, "%+.1f");
            ImGui::SliderFloat("Floor friction", &floorFriction, 0.8f, 1.f, "%.3f");
        }

        if (ImGui::CollapsingHeader(
                "Springs and connections",
                ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            ImGui::Text("Connections: %zu", springs.size());
            ImGui::Checkbox(
                "Use rigid distance constraints", &useDistanceConstraints
            );
            ImGui::BeginDisabled(!useDistanceConstraints);
            ImGui::SliderInt(
                "Solver iterations", &constraintIterations, 1, 20
            );
            ImGui::SliderFloat(
                "Constraint stiffness", &constraintStiffness,
                0.1f, 1.f, "%.2f"
            );
            ImGui::TextDisabled(
                "More iterations preserve shape but require more work"
            );
            ImGui::EndDisabled();
            ImGui::Checkbox("Breakable connections", &breakableSprings);
            ImGui::BeginDisabled(!breakableSprings);
            ImGui::SliderFloat(
                "Breaking strain", &springBreakingStrain,
                0.1f, 2.f, "%.2f"
            );
            ImGui::Text("Broken since load: %zu", brokenSpringCount);
            ImGui::TextDisabled("0.65 breaks at 65%% beyond resting length");
            ImGui::EndDisabled();
            ImGui::TextDisabled("Chain — one linked path");
            ImGui::SliderInt(
                "Chain particles", &generatedChainCount, 2, 1000
            );
            if (ImGui::Button("Generate chain"))
            {
                loadSpringChain(
                    static_cast<std::size_t>(generatedChainCount)
                );
            }

            ImGui::Separator();
            ImGui::TextDisabled("Lattice — a rectangular spring mesh");
            ImGui::SliderInt(
                "Lattice columns", &generatedLatticeColumns, 2, 40
            );
            ImGui::SliderInt(
                "Lattice rows", &generatedLatticeRows, 2, 30
            );
            ImGui::SliderFloat(
                "Lattice / chain spacing", &generatedStructureSpacing,
                12.f, 40.f, "%.0f px"
            );
            if (ImGui::Button("Generate lattice"))
            {
                loadSoftBodyLattice(
                    static_cast<std::size_t>(generatedLatticeColumns),
                    static_cast<std::size_t>(generatedLatticeRows)
                );
            }
            ImGui::SameLine();
            if (ImGui::Button("Fill screen"))
            {
                loadSoftBodyLattice(40, 30);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Web — concentric rings and radial spokes");
            ImGui::SliderInt("Web rings", &generatedWebRings, 2, 15);
            ImGui::SliderInt("Web spokes", &generatedWebSpokes, 4, 40);
            if (ImGui::Button("Generate web"))
            {
                loadRadialWeb(
                    static_cast<std::size_t>(generatedWebRings),
                    static_cast<std::size_t>(generatedWebSpokes)
                );
            }
            ImGui::Separator();
            ImGui::TextDisabled("Manual connection (advanced)");
            ImGui::SliderFloat(
                "New spring stiffness", &newSpringStiffness,
                500.f, 20000.f, "%.0f"
            );
            ImGui::SliderFloat(
                "New spring damping", &newSpringDamping,
                0.f, 1000.f, "%.0f"
            );

            if (!firstSpringBody)
            {
                ImGui::BeginDisabled(
                    !selectedBody || *selectedBody >= bodies.size()
                );
                if (ImGui::Button("Use selected as first endpoint"))
                {
                    firstSpringBody = selectedBody;
                }
                ImGui::EndDisabled();
            }
            else
            {
                ImGui::Text("First endpoint: body %zu", *firstSpringBody + 1);
                const bool canConnect = selectedBody &&
                    *selectedBody < bodies.size() &&
                    *selectedBody != *firstSpringBody;
                ImGui::BeginDisabled(!canConnect);
                if (ImGui::Button("Connect first to selected"))
                {
                    const sf::Vector2f difference =
                        bodies[*selectedBody].body.center() -
                        bodies[*firstSpringBody].body.center();
                    const float restLength = std::sqrt(
                        difference.x * difference.x +
                        difference.y * difference.y
                    );
                    springs.push_back({
                        *firstSpringBody,
                        *selectedBody,
                        restLength,
                        newSpringStiffness,
                        newSpringDamping
                    });
                    restartBodies = bodies;
                    restartSprings = springs;
                    firstSpringBody.reset();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel connection"))
                {
                    firstSpringBody.reset();
                }
            }

            ImGui::BeginDisabled(springs.empty());
            if (ImGui::Button("Remove all springs"))
            {
                springs.clear();
                restartSprings.clear();
                firstSpringBody.reset();
            }
            ImGui::EndDisabled();
            ImGui::TextDisabled(
                "Select body A, set it, then select body B and connect"
            );
        }

        if (ImGui::CollapsingHeader("Debug visualization"))
        {
            if (ImGui::Checkbox("Colour by charge", &colorByCharge) &&
                colorByCharge)
            {
                colorByMaterial = false;
            }
            if (ImGui::Checkbox("Colour by material", &colorByMaterial) &&
                colorByMaterial)
            {
                colorByCharge = false;
            }
            ImGui::Checkbox("Velocity vectors", &showVelocityVectors);
            ImGui::Checkbox("Contact points", &showContacts);
            ImGui::Checkbox("Collision normals", &showCollisionNormals);
            ImGui::BeginDisabled(!useSpatialGrid);
            ImGui::Checkbox("Spatial grid", &showSpatialGrid);
            ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Point field controls"))
        {
            ImGui::Text("Fields: %zu", pointFields.size());
            if (ImGui::Button(
                    placingPointField ? "Click and drag in scene..." : "Add attractor"
                ))
            {
                placingPointField = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add repulsor"))
            {
                pointFields.push_back({{400.f, 300.f}, -8000000.f, 0.f, true});
                selectedPointField = pointFields.size() - 1;
            }
            if (ImGui::Button("Add vortex"))
            {
                pointFields.push_back({{400.f, 300.f}, 3000000.f, 10000000.f, true});
                selectedPointField = pointFields.size() - 1;
            }
            for (std::size_t index = 0; index < pointFields.size(); ++index)
            {
                if (pointFields[index].remainingLifetime >= 0.f)
                {
                    continue;
                }
                ImGui::PushID(static_cast<int>(index));
                const std::string label = "Field " + std::to_string(index + 1);
                if (ImGui::Selectable(label.c_str(), selectedPointField == index))
                {
                    selectedPointField = index;
                }
                ImGui::PopID();
            }
            if (selectedPointField && *selectedPointField < pointFields.size())
            {
                auto& field = pointFields[*selectedPointField];
                ImGui::Checkbox("Selected field enabled", &field.enabled);
                ImGui::Checkbox("Responds to charge", &field.chargeSensitive);
                ImGui::SliderFloat(
                    "Attract / repel", &field.radialStrength,
                    -20000000.f, 20000000.f, "%.1e"
                );
                ImGui::SliderFloat(
                    "Vortex##field-strength", &field.vortexStrength,
                    -20000000.f, 20000000.f, "%.1e"
                );
                ImGui::SliderFloat(
                    "Oscillation amount", &field.oscillationAmount,
                    0.f, 2.f, "%.2f"
                );
                ImGui::SliderFloat(
                    "Oscillation frequency", &field.oscillationFrequency,
                    0.1f, 3.f, "%.2f Hz"
                );
                ImGui::TextDisabled("Above 1.0 can alternate attract/repel");
                ImGui::TextDisabled("Drag its coloured ring in the scene");
                if (ImGui::Button("Delete selected field"))
                {
                    pointFields.erase(pointFields.begin() +
                        static_cast<std::ptrdiff_t>(*selectedPointField));
                    selectedPointField.reset();
                    draggingPointField = false;
                }
            }
        }

        if (ImGui::CollapsingHeader("Performance"))
        {
            ImGui::Text("Render: %.0f FPS  Physics: 120 Hz", framesPerSecond);
            ImGui::Text("Search: %s", useSpatialGrid ? "Uniform grid" : "All pairs");
            ImGui::Text("All pairs: %zu", allPairsCount);
            ImGui::Text("Candidates: %zu", candidatePairChecks);
            const float candidateReduction = allPairsCount > 0
                ? 100.f * (1.f - static_cast<float>(candidatePairChecks) /
                    static_cast<float>(allPairsCount))
                : 0.f;
            ImGui::Text("Reduction: %.1f%%", candidateReduction);
            ImGui::Text("Contacts: %zu", actualCollisions);
            ImGui::Text("Physics step: %.3f ms", physicsStepMilliseconds);
            ImGui::Checkbox("Use spatial grid", &useSpatialGrid);
            ImGui::BeginDisabled(!useSpatialGrid);
            ImGui::SliderFloat("Cell size", &gridCellSize, 20.f, 120.f, "%.0f px");
            ImGui::EndDisabled();
            if (ImGui::Button("Load 100 bodies"))
            {
                loadPopulation(100);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load 300 bodies"))
            {
                loadPopulation(300);
            }
            if (ImGui::Button("Load 600 bodies"))
            {
                loadPopulation(600);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load 1000 bodies"))
            {
                loadPopulation(1000);
            }
        }

        ImGui::TextDisabled("Choose a tool above, then click the scene");
        ImGui::TextDisabled("Space: pause  N: step  R: restart");
        ImGui::End();
        }

        if (draggingPointField && selectedPointField &&
            *selectedPointField < pointFields.size())
        {
            pointFields[*selectedPointField].position =
                toWorldPosition(sf::Mouse::getPosition(window));
        }

        if (cuttingConnections && interactionTool == 4)
        {
            cutConnectionsAt(
                toWorldPosition(sf::Mouse::getPosition(window))
            );
        }

        if (draggedBody && *draggedBody < bodies.size())
        {
            const sf::Vector2f mousePosition =
                toWorldPosition(sf::Mouse::getPosition(window));
            const sf::Vector2f mouseMovement = mousePosition - lastMousePosition;

            if (frameTime > 0.0001f)
            {
                throwVelocity = mouseMovement / frameTime;
                limitMagnitude(throwVelocity, MAX_THROW_SPEED);
            }

            auto& selected = bodies[*draggedBody].body;
            selected.position.x = std::clamp(
                mousePosition.x - dragOffset.x,
                0.f,
                windowWidth - selected.radius * 2.f
            );
            selected.position.y = std::clamp(
                mousePosition.y - dragOffset.y,
                0.f,
                windowHeight - selected.radius * 2.f
            );
            selected.velocity = sf::Vector2f(0.f, 0.f);
            lastMousePosition = mousePosition;
        }

        if (paused)
        {
            accumulator = 0.f;
        }
        else
        {
            accumulator += frameTime;
        }

        if (stepRequested)
        {
            debugContacts.clear();
            simulateStep();
            stepRequested = false;
        }

        if (!paused && accumulator >= FIXED_TIME_STEP)
        {
            debugContacts.clear();
        }

        while (!paused && accumulator >= FIXED_TIME_STEP)
        {
            simulateStep();
            accumulator -= FIXED_TIME_STEP;
        }

        for (auto& view : bodies)
        {
            const sf::Color chargeColor = view.body.charge > 0.1f
                ? sf::Color(255, 110, 80)
                : (view.body.charge < -0.1f
                    ? sf::Color(70, 170, 255)
                    : sf::Color::White);
            view.shape.setFillColor(colorByMaterial
                ? materialColor(view.material)
                : (colorByCharge ? chargeColor : sf::Color::White));
            view.shape.setOutlineColor(sf::Color(255, 215, 70));
            view.shape.setOutlineThickness(
                view.body.inverseMass == 0.f ? 3.f : 0.f
            );
            view.sync();
        }

        if (bodyCollisionsEnabled)
        {
            for (std::size_t first = 0; first < bodies.size(); ++first)
            {
                for (std::size_t second = first + 1;
                     second < bodies.size(); ++second)
                {
                    if (physics::circlesOverlap(
                            bodies[first].body.center(),
                            bodies[first].body.radius,
                            bodies[second].body.center(),
                            bodies[second].body.radius
                        ))
                    {
                        bodies[first].shape.setFillColor(sf::Color::Red);
                        bodies[second].shape.setFillColor(sf::Color::Red);
                    }
                }
            }
        }

        if (selectedBody && *selectedBody < bodies.size())
        {
            bodies[*selectedBody].shape.setFillColor(sf::Color(255, 215, 0));
        }

        const auto drawSpringConnections = [&](sf::RenderTarget& target,
                                                unsigned char alpha = 255)
        {
            for (const auto& spring : springs)
            {
                if (spring.first >= bodies.size() ||
                    spring.second >= bodies.size())
                {
                    continue;
                }
                const sf::Vector2f start = bodies[spring.first].body.center();
                const sf::Vector2f end = bodies[spring.second].body.center();
                const sf::Vector2f difference = end - start;
                const float length = std::sqrt(
                    difference.x * difference.x + difference.y * difference.y
                );
                const float extension = length - spring.restLength;
                sf::Color color = extension > 2.f
                    ? sf::Color(255, 120, 100)
                    : (extension < -2.f
                        ? sf::Color(90, 180, 255)
                        : sf::Color(210, 220, 235));
                color.a = alpha;
                drawLine(target, start, end, color);
            }
        };

        const auto drawPointFieldMarkers = [&]()
        {
            for (std::size_t index = 0; index < pointFields.size(); ++index)
            {
                const auto& field = pointFields[index];
                sf::CircleShape marker(12.f);
                marker.setOrigin(sf::Vector2f(12.f, 12.f));
                marker.setPosition(field.position);
                marker.setFillColor(sf::Color::Transparent);
                sf::Color color = field.vortexStrength != 0.f
                    ? sf::Color(255, 215, 70, 220)
                    : (field.radialStrength >= 0.f
                        ? sf::Color(80, 220, 255, 220)
                        : sf::Color(255, 90, 190, 220));
                if (!field.enabled)
                {
                    color.a = 80;
                }
                marker.setOutlineColor(color);
                marker.setOutlineThickness(
                    selectedPointField == index ? 4.f : 2.f
                );
                window.draw(marker);
            }
        };

        const auto drawCutBrush = [&]()
        {
            if (interactionTool != 4 || ImGui::GetIO().WantCaptureMouse)
            {
                return;
            }
            sf::CircleShape brush(connectionCutRadius);
            brush.setOrigin(sf::Vector2f(
                connectionCutRadius, connectionCutRadius
            ));
            brush.setPosition(
                toWorldPosition(sf::Mouse::getPosition(window))
            );
            brush.setFillColor(sf::Color(255, 70, 90, 30));
            brush.setOutlineColor(sf::Color(255, 100, 120, 220));
            brush.setOutlineThickness(2.f);
            window.draw(brush);
        };

        if (canvasMode)
        {
            const sf::Color background = canvasBackgrounds[canvasPalette];
            if (canvasTrails)
            {
                sf::RectangleShape fade(sf::Vector2f(windowWidth, windowHeight));
                fade.setFillColor(sf::Color(
                    background.r,
                    background.g,
                    background.b,
                    22
                ));
                canvasTexture.draw(fade);
            }
            else
            {
                canvasTexture.clear(background);
            }

            for (std::size_t index = 0; index < bodies.size(); ++index)
            {
                sf::CircleShape canvasShape = bodies[index].shape;
                if (colorByMaterial)
                {
                    canvasShape.setFillColor(
                        materialColor(bodies[index].material)
                    );
                }
                else if (colorByCharge && bodies[index].body.charge > 0.1f)
                {
                    canvasShape.setFillColor(sf::Color(255, 90, 130));
                }
                else if (colorByCharge && bodies[index].body.charge < -0.1f)
                {
                    canvasShape.setFillColor(sf::Color(70, 210, 255));
                }
                else
                {
                    canvasShape.setFillColor(
                        canvasPalettes[canvasPalette][index % 5]
                    );
                }
                canvasTexture.draw(canvasShape, sf::BlendAdd);
            }
            drawSpringConnections(canvasTexture, 55);
            canvasTexture.display();

            window.clear(background);
            const sf::Sprite canvasSprite(canvasTexture.getTexture());
            window.draw(canvasSprite);
            drawSpringConnections(window);
            if (selectedBody && *selectedBody < bodies.size())
            {
                const auto& selected = bodies[*selectedBody].body;
                sf::CircleShape selectionRing(selected.radius + 4.f);
                selectionRing.setOrigin(sf::Vector2f(
                    selected.radius + 4.f,
                    selected.radius + 4.f
                ));
                selectionRing.setPosition(selected.center());
                selectionRing.setFillColor(sf::Color::Transparent);
                selectionRing.setOutlineColor(sf::Color::White);
                selectionRing.setOutlineThickness(2.f);
                window.draw(selectionRing);
            }
            drawPointFieldMarkers();
            drawCutBrush();
            ImGui::SFML::Render(window);
            window.display();
            continue;
        }

        window.clear();

        if (showSpatialGrid && useSpatialGrid)
        {
            const sf::Color gridColor(45, 70, 90);
            for (float x = 0.f; x <= windowWidth; x += gridCellSize)
            {
                drawLine(
                    window,
                    sf::Vector2f(x, 0.f),
                    sf::Vector2f(x, windowHeight),
                    gridColor
                );
            }
            for (float y = 0.f; y <= windowHeight; y += gridCellSize)
            {
                drawLine(
                    window,
                    sf::Vector2f(0.f, y),
                    sf::Vector2f(windowWidth, y),
                    gridColor
                );
            }
        }

        drawSpringConnections(window);

        for (const auto& view : bodies)
        {
            window.draw(view.shape);
        }

        if (showVelocityVectors)
        {
            for (const auto& view : bodies)
            {
                sf::Vector2f vector =
                    view.body.velocity * VELOCITY_VECTOR_SCALE;
                limitMagnitude(vector, MAX_DEBUG_VECTOR_LENGTH);
                drawLine(
                    window,
                    view.body.center(),
                    view.body.center() + vector,
                    sf::Color(80, 255, 120)
                );
            }
        }

        for (const auto& contact : debugContacts)
        {
            if (showCollisionNormals)
            {
                drawLine(
                    window,
                    contact.point,
                    contact.point + contact.normal * NORMAL_VECTOR_LENGTH,
                    sf::Color(80, 220, 255)
                );
            }

            if (showContacts)
            {
                sf::CircleShape marker(4.f);
                marker.setOrigin(sf::Vector2f(4.f, 4.f));
                marker.setPosition(contact.point);
                marker.setFillColor(sf::Color(255, 80, 220));
                window.draw(marker);
            }
        }

        drawPointFieldMarkers();
        drawCutBrush();

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}
