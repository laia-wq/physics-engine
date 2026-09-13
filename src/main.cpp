#include <SFML/Graphics.hpp>

#include <imgui-SFML.h>
#include <imgui.h>

#include "physics/BroadPhase.hpp"
#include "physics/Collision.hpp"
#include "physics/DistanceConstraint.hpp"
#include "physics/Electrostatics.hpp"
#include "physics/MathematicalSurface.hpp"
#include "physics/PointField.hpp"
#include "physics/Spring.hpp"
#include "presets/PresetConstruction.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
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

using PointField = physics::PointField;

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
    bool showFixedOutline = true;

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

    const auto loadFoundationScene = [&](
        presets::FoundationPreset preset,
        std::size_t stressBodyCount = 0
    )
    {
        bodies.clear();
        springs.clear();
        restartSprings.clear();
        for (const auto& definition :
             presets::buildFoundationPreset(preset, stressBodyCount))
        {
            bodies.emplace_back(
                definition.radius,
                definition.position,
                definition.velocity,
                definition.restitution
            );
        }
        restartBodies = bodies;
    };

    const auto loadClassicScene = [&]()
    {
        loadFoundationScene(presets::FoundationPreset::Classic);
    };
    const auto loadStressScene = [&](std::size_t bodyCount)
    {
        loadFoundationScene(presets::FoundationPreset::Stress, bodyCount);
    };
    const auto loadHeadOnScene = [&]()
    {
        loadFoundationScene(presets::FoundationPreset::HeadOn);
    };
    const auto loadZeroGravityScene = [&]()
    {
        loadFoundationScene(presets::FoundationPreset::ZeroGravity);
    };
    const auto loadRainScene = [&]()
    {
        loadFoundationScene(presets::FoundationPreset::Rain);
    };
    const auto loadOrbitScene = [&]()
    {
        loadFoundationScene(presets::FoundationPreset::Orbit);
    };

    loadClassicScene();

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
    float spacetimeWellDepth = 150.f;
    float waveSurfaceHeight = 48.f;
    float doubleWellDepth = 105.f;
    bool mathematicalMeshDiagonals = true;
    int activeMathematicalForm = -1;
    bool terrainMeshDiagonals = false;
    bool useSpatialGrid = false;
    float gridCellSize = 50.f;
    std::vector<DebugContact> debugContacts;
    std::size_t allPairsCount = 0;
    std::size_t candidatePairChecks = 0;
    std::size_t actualCollisions = 0;
    std::size_t electrostaticPairChecks = 0;
    double physicsStepMilliseconds = 0.0;
    bool screenshotRequested = false;
    std::string screenshotStatus;

    const auto pointFieldAcceleration = [&](const CircleView& view)
    {
        constexpr float VISUAL_FIELD_RESPONSE = 12.f;
        return physics::calculatePointFieldAcceleration(
            view.body,
            pointFields,
            simulationTime,
            FIELD_SOFTENING,
            VISUAL_FIELD_RESPONSE,
            materialForceResponse(view.material)
        );
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
        constexpr std::size_t ROWS = 45;
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
                    {-1.00f, 0.24f}, {-0.88f, 0.60f}, {-0.68f, 0.88f},
                    {-0.38f, 1.00f}, {-0.05f, 0.98f}, {0.28f, 0.93f},
                    {0.55f, 0.80f}, {0.76f, 0.61f}, {0.92f, 0.31f},
                    {1.00f, 0.21f}
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

    const auto loadWindblownTree = [&]()
    {
        constexpr float RADIUS = 1.65f;
        bodies.clear();
        springs.clear();

        const auto addParticle = [&](sf::Vector2f center,
                                     ParticleMaterial material,
                                     int group,
                                     bool fixed = false)
        {
            bodies.emplace_back(
                RADIUS,
                center - sf::Vector2f(RADIUS, RADIUS),
                sf::Vector2f(0.f, 0.f),
                0.2f
            );
            auto& particle = bodies.back();
            particle.material = fixed ? ParticleMaterial::Fixed : material;
            particle.groupId = group;
            if (fixed)
            {
                particle.body.inverseMass = 0.f;
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

        // The hill is a shallow anchored mesh, not a solid platform. Its
        // changing row spacing continues the project's wireframe language.
        constexpr std::size_t HILL_COLUMNS = 65;
        constexpr std::size_t HILL_ROWS = 6;
        std::array<std::array<std::size_t, HILL_COLUMNS>, HILL_ROWS> hill{};
        for (std::size_t row = 0; row < HILL_ROWS; ++row)
        {
            for (std::size_t column = 0; column < HILL_COLUMNS; ++column)
            {
                const float parameter = static_cast<float>(column) /
                    static_cast<float>(HILL_COLUMNS - 1);
                const float normalized = 0.5f + 0.5f * std::sin(
                    (parameter - 0.5f) * 3.14159265359f
                );
                const float x = 48.f + normalized * 704.f;
                const float surface = 535.f -
                    34.f * std::sin(normalized * 3.14159265359f);
                const float depth = std::pow(
                    static_cast<float>(row), 1.35f
                ) * 4.2f;
                hill[row][column] = addParticle(
                    sf::Vector2f(x, surface + depth),
                    ParticleMaterial::Structural,
                    10,
                    row == HILL_ROWS - 1
                );
                if (column > 0)
                {
                    connect(hill[row][column - 1], hill[row][column]);
                }
                if (row > 0)
                {
                    connect(hill[row - 1][column], hill[row][column]);
                    if (column > 0)
                    {
                        connect(hill[row - 1][column - 1], hill[row][column]);
                    }
                    if (column + 1 < HILL_COLUMNS)
                    {
                        connect(hill[row - 1][column + 1], hill[row][column]);
                    }
                }
            }
        }

        // Five interconnected strands give the leaning trunk visible width
        // and let it bend without behaving like a single piece of string.
        constexpr std::size_t TRUNK_ROWS = 37;
        constexpr std::size_t TRUNK_COLUMNS = 5;
        std::array<std::array<std::size_t, TRUNK_COLUMNS>, TRUNK_ROWS> trunk{};
        for (std::size_t row = 0; row < TRUNK_ROWS; ++row)
        {
            const float rawT = static_cast<float>(row) /
                static_cast<float>(TRUNK_ROWS - 1);
            const float t = std::sin(rawT * 3.14159265359f * 0.5f);
            const sf::Vector2f center(
                355.f + 44.f * t + 20.f * t * t +
                    7.f * std::sin(t * 5.4f) + 3.f * std::sin(t * 13.f),
                505.f - 245.f * t + 3.5f * std::sin(t * 8.3f)
            );
            const float halfWidth = (11.5f - 7.7f * t) *
                (1.f + 0.09f * std::sin(t * 17.f));
            for (std::size_t column = 0; column < TRUNK_COLUMNS; ++column)
            {
                const float acrossParameter = -1.f + 2.f *
                    static_cast<float>(column) /
                    static_cast<float>(TRUNK_COLUMNS - 1);
                const float across = std::sin(
                    acrossParameter * 3.14159265359f * 0.5f
                );
                trunk[row][column] = addParticle(
                    center + sf::Vector2f(across * halfWidth, across * 2.f),
                    ParticleMaterial::Structural,
                    20,
                    row == 0
                );
                if (row > 0 && row % 9 == 0)
                {
                    bodies[trunk[row][column]].body.inverseMass = 0.f;
                    bodies[trunk[row][column]].showFixedOutline = false;
                }
                if (column > 0)
                {
                    connect(trunk[row][column - 1], trunk[row][column]);
                }
                if (row > 0)
                {
                    connect(trunk[row - 1][column], trunk[row][column]);
                    if (column > 0)
                    {
                        connect(trunk[row - 1][column - 1], trunk[row][column]);
                    }
                    if (column + 1 < TRUNK_COLUMNS)
                    {
                        connect(trunk[row - 1][column + 1], trunk[row][column]);
                    }
                }
            }
        }
        for (std::size_t column = 0; column < TRUNK_COLUMNS; ++column)
        {
            connect(hill[0][28 + column], trunk[0][column]);
        }

        const auto growBranch = [&](std::size_t start,
                                    sf::Vector2f step,
                                    std::size_t count,
                                    int group)
        {
            std::array<std::size_t, 3> previous{start, start, start};
            sf::Vector2f position = bodies[start].body.center();
            std::vector<std::size_t> centerline;
            centerline.reserve(count);
            const float stepLength = std::sqrt(
                step.x * step.x + step.y * step.y
            );
            const sf::Vector2f normal(
                -step.y / stepLength, step.x / stepLength
            );
            for (std::size_t index = 0; index < count; ++index)
            {
                const float bend = static_cast<float>(index) /
                    static_cast<float>(std::max<std::size_t>(1, count - 1));
                const float stepScale = 0.72f + 0.56f *
                    std::sin((bend + 0.05f) * 3.14159265359f * 0.8f);
                const float irregularCurve =
                    2.8f * std::sin(static_cast<float>(index) * 0.82f +
                        static_cast<float>(group) * 0.63f) +
                    (group % 2 == 0 ? 4.5f : -3.5f) * bend * bend;
                position += step * stepScale + normal * irregularCurve;
                const float width = 4.5f * (1.f - 0.72f * bend);
                std::array<std::size_t, 3> current{};
                for (std::size_t strand = 0; strand < 3; ++strand)
                {
                    const float across = static_cast<float>(strand) - 1.f;
                    current[strand] = addParticle(
                        position + normal * across * width,
                        ParticleMaterial::Structural,
                        group
                    );
                    if (index % 4 == 0)
                    {
                        bodies[current[strand]].body.inverseMass = 0.f;
                        bodies[current[strand]].showFixedOutline = false;
                    }
                    connect(previous[strand], current[strand]);
                    if (strand > 0)
                    {
                        connect(current[strand - 1], current[strand]);
                        connect(previous[strand - 1], current[strand]);
                    }
                }
                previous = current;
                centerline.push_back(current[1]);
            }
            return centerline;
        };

        const std::array branches{
            growBranch(trunk[17][2], sf::Vector2f(-8.5f, -6.f), 14, 21),
            growBranch(trunk[22][2], sf::Vector2f(9.f, -5.5f), 17, 22),
            growBranch(trunk[27][2], sf::Vector2f(-6.5f, -8.f), 12, 23),
            growBranch(trunk[32][2], sf::Vector2f(9.5f, -6.f), 14, 24),
            growBranch(trunk[36][2], sf::Vector2f(7.f, -8.f), 11, 25)
        };

        struct CanopyPlacement
        {
            std::size_t branch;
            float along;
            sf::Vector2f offset;
            float halfWidth;
            float halfHeight;
        };
        const std::array canopy{
            CanopyPlacement{0, 0.38f, {-20.f, 8.f}, 45.f, 28.f},
            CanopyPlacement{0, 0.68f, {-22.f, -5.f}, 54.f, 32.f},
            CanopyPlacement{0, 1.00f, {-28.f, 6.f}, 63.f, 38.f},
            CanopyPlacement{1, 0.24f, {16.f, -10.f}, 43.f, 27.f},
            CanopyPlacement{1, 0.48f, {24.f, 8.f}, 58.f, 33.f},
            CanopyPlacement{1, 0.74f, {24.f, -12.f}, 64.f, 38.f},
            CanopyPlacement{1, 1.00f, {30.f, 4.f}, 76.f, 42.f},
            CanopyPlacement{2, 0.42f, {-16.f, -16.f}, 43.f, 29.f},
            CanopyPlacement{2, 0.72f, {-24.f, -12.f}, 54.f, 34.f},
            CanopyPlacement{2, 1.00f, {-22.f, -22.f}, 59.f, 36.f},
            CanopyPlacement{3, 0.36f, {18.f, 8.f}, 45.f, 27.f},
            CanopyPlacement{3, 0.68f, {24.f, -9.f}, 61.f, 36.f},
            CanopyPlacement{3, 1.00f, {34.f, 8.f}, 80.f, 43.f},
            CanopyPlacement{4, 0.38f, {10.f, -18.f}, 40.f, 26.f},
            CanopyPlacement{4, 0.70f, {20.f, -22.f}, 55.f, 34.f},
            CanopyPlacement{4, 1.00f, {28.f, -25.f}, 70.f, 40.f}
        };
        for (std::size_t cluster = 0; cluster < canopy.size(); ++cluster)
        {
            constexpr std::size_t LEAF_COLUMNS = 13;
            constexpr std::size_t LEAF_ROWS = 9;
            std::array<std::array<std::optional<std::size_t>, LEAF_COLUMNS>,
                LEAF_ROWS> leaves{};
            const auto& placement = canopy[cluster];
            const auto& branch = branches[placement.branch];
            const std::size_t branchPosition = std::min(
                branch.size() - 1,
                static_cast<std::size_t>(std::round(
                    placement.along * static_cast<float>(branch.size() - 1)
                ))
            );
            const std::size_t attachmentNode = branch[branchPosition];
            const sf::Vector2f clusterCenter =
                bodies[attachmentNode].body.center() + placement.offset;
            for (std::size_t row = 0; row < LEAF_ROWS; ++row)
            {
                for (std::size_t column = 0; column < LEAF_COLUMNS; ++column)
                {
                    const float u = -1.f + 2.f * static_cast<float>(column) /
                        static_cast<float>(LEAF_COLUMNS - 1);
                    const float v = -1.f + 2.f * static_cast<float>(row) /
                        static_cast<float>(LEAF_ROWS - 1);
                    const float mask = u * u + v * v;
                    const float gapA = (u + 0.3f) * (u + 0.3f) +
                        (v - 0.05f) * (v - 0.05f);
                    const float gapB = (u - 0.34f) * (u - 0.34f) +
                        (v + 0.28f) * (v + 0.28f);
                    const bool windGap = u < -0.05f && u > -0.42f &&
                        v < -0.32f && v > -0.68f;
                    if (mask > 1.f || gapA < 0.065f || gapB < 0.05f ||
                        windGap ||
                        ((row * 3 + column * 5 + cluster) % 23 == 0))
                    {
                        continue;
                    }
                    const float projectedU = std::sin(
                        u * 3.14159265359f * 0.5f
                    );
                    const float projectedV = std::sin(
                        v * 3.14159265359f * 0.5f
                    );
                    const float depth = std::sqrt(std::max(0.f, 1.f - mask));
                    leaves[row][column] = addParticle(
                        clusterCenter + sf::Vector2f(
                            projectedU *
                                (placement.halfWidth + 7.f * v) +
                                12.f * depth,
                            projectedV * placement.halfHeight -
                                8.f * depth
                        ),
                        ParticleMaterial::Fragile,
                        30 + static_cast<int>(cluster)
                    );
                    if (column > 0 && leaves[row][column - 1])
                    {
                        connect(*leaves[row][column - 1], *leaves[row][column]);
                    }
                    if (row > 0 && leaves[row - 1][column])
                    {
                        connect(*leaves[row - 1][column], *leaves[row][column]);
                    }
                    if ((row + column + cluster) % 2 == 0 &&
                        row > 0 && column > 0 &&
                        leaves[row - 1][column - 1])
                    {
                        connect(*leaves[row - 1][column - 1],
                            *leaves[row][column]);
                    }
                    if ((row + column + cluster) % 2 != 0 &&
                        row > 0 && column + 1 < LEAF_COLUMNS &&
                        leaves[row - 1][column + 1])
                    {
                        connect(*leaves[row - 1][column + 1],
                            *leaves[row][column]);
                    }
                }
            }
            std::optional<std::size_t> attachment;
            float nearestDistanceSquared =
                std::numeric_limits<float>::max();
            for (const auto& row : leaves)
            {
                for (const auto leaf : row)
                {
                    if (leaf)
                    {
                        const sf::Vector2f difference =
                            bodies[*leaf].body.center() -
                            bodies[attachmentNode].body.center();
                        const float distanceSquared =
                            difference.x * difference.x +
                            difference.y * difference.y;
                        if (distanceSquared < nearestDistanceSquared)
                        {
                            nearestDistanceSquared = distanceSquared;
                            attachment = leaf;
                        }
                    }
                }
            }
            if (attachment)
            {
                connect(attachmentNode, *attachment);
            }
        }

        useDistanceConstraints = true;
        constraintIterations = 14;
        constraintStiffness = 0.84f;
        breakableSprings = false;
        brokenSpringCount = 0;
        cutConnectionCount = 0;
        colorByCharge = false;
        colorByMaterial = true;
        gravityEnabled = true;
        gravityField[0] = 0.f;
        gravityField[1] = 0.f;
        windEnabled = true;
        windForce[0] = 190.f;
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

    [[maybe_unused]] const auto loadParticlePortrait = [&]()
    {
        constexpr std::size_t COLUMNS = 43;
        constexpr std::size_t ROWS = 53;
        constexpr float RADIUS = 1.55f;
        constexpr float PI = 3.14159265359f;
        const sf::Vector2f center(400.f, 285.f);

        bodies.clear();
        springs.clear();
        std::vector<std::optional<std::size_t>> face(COLUMNS * ROWS);

        const auto addParticle = [&](sf::Vector2f particleCenter,
                                     ParticleMaterial material,
                                     int group,
                                     bool fixed = false)
        {
            bodies.emplace_back(
                RADIUS,
                particleCenter - sf::Vector2f(RADIUS, RADIUS),
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

        constexpr std::array<std::pair<float, float>, 10> headProfile{{
            {-1.f, 0.2f}, {-0.91f, 0.62f}, {-0.72f, 0.9f},
            {-0.38f, 0.98f}, {-0.08f, 1.f}, {0.22f, 0.96f},
            {0.48f, 0.82f}, {0.7f, 0.61f}, {0.9f, 0.31f},
            {1.f, 0.12f}
        }};
        const auto profileWidth = [&](float v)
        {
            float width = headProfile.back().second;
            for (std::size_t index = 1; index < headProfile.size(); ++index)
            {
                if (v <= headProfile[index].first)
                {
                    const auto [v0, width0] = headProfile[index - 1];
                    const auto [v1, width1] = headProfile[index];
                    const float amount = (v - v0) / (v1 - v0);
                    width = width0 + amount * (width1 - width0);
                    break;
                }
            }
            return width;
        };

        for (std::size_t row = 0; row < ROWS; ++row)
        {
            const float rawV = -1.f + 2.f * static_cast<float>(row) /
                static_cast<float>(ROWS - 1);
            const float v = std::sin(rawV * PI * 0.5f);
            const float halfWidth = 142.f * profileWidth(v);
            for (std::size_t column = 0; column < COLUMNS; ++column)
            {
                const float u = -1.f + 2.f * static_cast<float>(column) /
                    static_cast<float>(COLUMNS - 1);
                const float projectedU = std::sin(u * PI * 0.5f);

                // Empty feature regions make the surrounding connection loops
                // describe the eyes, nostrils, and mouth.
                const float leftEye =
                    std::pow((u + 0.39f) / 0.19f, 2.f) +
                    std::pow((v + 0.015f) / 0.062f, 2.f);
                const float rightEye =
                    std::pow((u - 0.37f) / 0.18f, 2.f) +
                    std::pow((v + 0.005f) / 0.06f, 2.f);
                const float leftBrow =
                    std::pow((u + 0.39f) / 0.23f, 2.f) +
                    std::pow((v + 0.145f + 0.025f * u) / 0.027f, 2.f);
                const float rightBrow =
                    std::pow((u - 0.37f) / 0.22f, 2.f) +
                    std::pow((v + 0.14f - 0.02f * u) / 0.026f, 2.f);
                const float leftNostril =
                    std::pow((u + 0.095f) / 0.052f, 2.f) +
                    std::pow((v - 0.34f) / 0.035f, 2.f);
                const float rightNostril =
                    std::pow((u - 0.105f) / 0.052f, 2.f) +
                    std::pow((v - 0.34f) / 0.035f, 2.f);
                const float mouth =
                    std::pow((u + 0.005f) / 0.37f, 2.f) +
                    std::pow((v - 0.59f) / 0.042f, 2.f);
                if (leftEye < 1.f || rightEye < 1.f ||
                    leftBrow < 1.f || rightBrow < 1.f ||
                    leftNostril < 1.f || rightNostril < 1.f || mouth < 1.f)
                {
                    continue;
                }

                const float surfaceDepth = std::cos(u * PI * 0.5f) *
                    std::sqrt(std::max(0.f, 1.f - v * v));
                const float cheekPlanes =
                    13.f * std::exp(-std::pow((std::abs(u) - 0.43f) / 0.2f, 2.f)) *
                    std::exp(-std::pow((v - 0.16f) / 0.25f, 2.f));
                const float browPlane =
                    6.f * std::exp(-std::pow((v + 0.14f) / 0.11f, 2.f)) *
                    std::cos(u * PI);
                const float nosePlane =
                    19.f * std::exp(-std::pow(u / 0.16f, 2.f)) *
                    std::exp(-std::pow((v - 0.16f) / 0.31f, 2.f));
                const float lipPlane =
                    9.f * std::exp(-std::pow(u / 0.38f, 2.f)) *
                    std::exp(-std::pow((v - 0.59f) / 0.1f, 2.f));
                const float facialAsymmetry =
                    2.4f * std::sin((v + 0.35f) * 4.1f) *
                    (1.f - std::abs(u));

                const sf::Vector2f particleCenter(
                    center.x + projectedU * halfWidth +
                        9.f * surfaceDepth + facialAsymmetry +
                        (u >= 0.f ? 0.22f : -0.18f) * nosePlane,
                    center.y + v * 205.f - 9.f * surfaceDepth -
                        cheekPlanes + browPlane - 0.35f * nosePlane + lipPlane
                );
                const ParticleMaterial material = v < -0.61f ||
                    (std::abs(u) > 0.86f && v < 0.18f)
                    ? ParticleMaterial::Structural
                    : ParticleMaterial::Flexible;
                face[row * COLUMNS + column] = addParticle(
                    particleCenter,
                    material,
                    material == ParticleMaterial::Structural ? 42 : 41
                );
            }
        }

        for (std::size_t row = 0; row < ROWS; ++row)
        {
            for (std::size_t column = 0; column < COLUMNS; ++column)
            {
                const auto current = face[row * COLUMNS + column];
                if (!current)
                {
                    continue;
                }
                const auto connectAt = [&](std::size_t otherRow,
                                           std::size_t otherColumn)
                {
                    const auto other = face[otherRow * COLUMNS + otherColumn];
                    if (other)
                    {
                        connect(*current, *other);
                    }
                };
                if (column + 1 < COLUMNS)
                {
                    connectAt(row, column + 1);
                }
                if (row + 1 < ROWS)
                {
                    connectAt(row + 1, column);
                    if ((row + column) % 2 == 0 && column + 1 < COLUMNS)
                    {
                        connectAt(row + 1, column + 1);
                    }
                    if ((row + column) % 2 != 0 && column > 0)
                    {
                        connectAt(row + 1, column - 1);
                    }
                }
            }
        }

        // A stretched neck-and-shoulder mesh prevents the portrait from
        // reading as a floating mask and provides stable lower anchors.
        constexpr std::size_t NECK_COLUMNS = 31;
        constexpr std::size_t NECK_ROWS = 10;
        std::array<std::array<std::size_t, NECK_COLUMNS>, NECK_ROWS> neck{};
        for (std::size_t row = 0; row < NECK_ROWS; ++row)
        {
            const float t = static_cast<float>(row) /
                static_cast<float>(NECK_ROWS - 1);
            const float halfWidth = 46.f + 150.f * t * t;
            for (std::size_t column = 0; column < NECK_COLUMNS; ++column)
            {
                const float u = -1.f + 2.f * static_cast<float>(column) /
                    static_cast<float>(NECK_COLUMNS - 1);
                const float projectedU = std::sin(u * PI * 0.5f);
                neck[row][column] = addParticle(
                    sf::Vector2f(
                        center.x + 8.f + projectedU * halfWidth,
                        center.y + 202.f + 7.f * static_cast<float>(row) +
                            10.f * u * u * t
                    ),
                    ParticleMaterial::Structural,
                    43,
                    row == NECK_ROWS - 1
                );
                if (column > 0)
                {
                    connect(neck[row][column - 1], neck[row][column]);
                }
                if (row > 0)
                {
                    connect(neck[row - 1][column], neck[row][column]);
                    if (column > 0)
                    {
                        connect(neck[row - 1][column - 1], neck[row][column]);
                    }
                }
            }
        }
        const auto chin = face[(ROWS - 1) * COLUMNS + COLUMNS / 2];
        if (chin)
        {
            connect(*chin, neck[0][NECK_COLUMNS / 2]);
        }

        useDistanceConstraints = true;
        constraintIterations = 14;
        constraintStiffness = 0.86f;
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

    // Mathematical wireframes are generated as real 3D point grids and then
    // projected into the existing 2D particle renderer.
    const auto loadMathematicalWireframe = [&](int form)
    {
        activeMathematicalForm = form;
        const bool heightField = physics::isHeightField(form);
        const std::size_t columns = physics::mathematicalSurfaceColumns(form);
        const std::size_t rows = physics::mathematicalSurfaceRows(form);
        const bool wrapColumns =
            physics::mathematicalSurfaceWrapsColumns(form);
        const bool wrapRows = physics::mathematicalSurfaceWrapsRows(form);

        bodies.clear();
        springs.clear();
        std::vector<std::size_t> mesh(columns * rows);
        const auto meshAt = [&](std::size_t row, std::size_t column)
            -> std::size_t&
        {
            return mesh[row * columns + column];
        };
        const auto addFixedParticle = [&](sf::Vector2f center,
                                          float radius,
                                          int group = 52)
        {
            bodies.emplace_back(
                radius,
                center - sf::Vector2f(radius, radius),
                sf::Vector2f(0.f, 0.f),
                0.15f
            );
            bodies.back().material = ParticleMaterial::Structural;
            bodies.back().groupId = group;
            bodies.back().body.inverseMass = 0.f;
            bodies.back().showFixedOutline = false;
            return bodies.size() - 1;
        };
        const auto connect = [&](std::size_t first, std::size_t second)
        {
            const sf::Vector2f difference =
                bodies[second].body.center() - bodies[first].body.center();
            springs.push_back({
                first, second,
                std::sqrt(difference.x * difference.x +
                    difference.y * difference.y),
                6200.f, 250.f
            });
        };

        float blackHoleBottom = 0.f;
        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t column = 0; column < columns; ++column)
            {
                const auto surfacePoint =
                    physics::calculateMathematicalSurfacePoint(
                        form,
                        row,
                        column,
                        {spacetimeWellDepth, waveSurfaceHeight, doubleWellDepth}
                    );
                blackHoleBottom = std::max(
                    blackHoleBottom,
                    surfacePoint.downwardDepth
                );
                const bool mobiusBoundary =
                    form == 3 && (row == 0 || row + 1 == rows);
                meshAt(row, column) = addFixedParticle(
                    surfacePoint.projectedPosition,
                    heightField ? 1.2f : (mobiusBoundary ? 2.f : 1.3f)
                );
            }
        }

        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t column = 0; column < columns; ++column)
            {
                if (column + 1 < columns)
                {
                    connect(meshAt(row, column), meshAt(row, column + 1));
                }
                else if (wrapColumns)
                {
                    connect(meshAt(row, column), meshAt(row, 0));
                }
                else if (form == 3 && column + 1 == columns)
                {
                    connect(
                        meshAt(row, column),
                        meshAt(rows - 1 - row, 0)
                    );
                }
                if (row + 1 < rows)
                {
                    connect(meshAt(row, column), meshAt(row + 1, column));
                    if (mathematicalMeshDiagonals && column + 1 < columns)
                    {
                        // Alternate the diagonal direction so the surface is
                        // triangulated without developing a visual lean.
                        if ((row + column) % 2 == 0)
                        {
                            connect(
                                meshAt(row, column),
                                meshAt(row + 1, column + 1)
                            );
                        }
                        else
                        {
                            connect(
                                meshAt(row + 1, column),
                                meshAt(row, column + 1)
                            );
                        }
                    }
                }
                else if (wrapRows)
                {
                    connect(meshAt(row, column), meshAt(0, column));
                }
            }
        }
        if (form == 2)
        {
            const sf::Vector2f horizonCenter(400.f, 270.f + blackHoleBottom);
            addFixedParticle(horizonCenter, 23.f, 58);
        }

        useDistanceConstraints = false;
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

    const auto loadRollingTerrain = [&]()
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
                    if (terrainMeshDiagonals && column > 0)
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

        useDistanceConstraints = true;
        constraintIterations = 5;
        constraintStiffness = 0.84f;
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

    const auto saveScreenshot = [&]()
    {
        std::error_code error;
        const std::filesystem::path directory("screenshots");
        std::filesystem::create_directories(directory, error);
        if (error)
        {
            screenshotStatus = "Could not create screenshots folder";
            return;
        }
        const auto timestamp = std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        const std::filesystem::path path =
            directory / ("physics-engine-" + std::to_string(timestamp) + ".png");
        sf::Texture capture(window.getSize());
        capture.update(window);
        if (capture.copyToImage().saveToFile(path))
        {
            screenshotStatus =
                "Saved: " + std::filesystem::absolute(path, error).string();
        }
        else
        {
            screenshotStatus = "Screenshot export failed";
        }
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
                else if (keyPressed->scancode == sf::Keyboard::Scancode::F12)
                {
                    screenshotRequested = true;
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
        if (ImGui::Button("Save screenshot"))
        {
            screenshotRequested = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("F12");
        if (!screenshotStatus.empty())
        {
            ImGui::TextWrapped("%s", screenshotStatus.c_str());
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

        ImGui::End();
        ImGui::SetNextWindowPos(ImVec2(340.f, 12.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(330.f, 500.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Preset Gallery");
        {
            ImGui::TextDisabled("LEVEL 1  |  Physics foundations");
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
            ImGui::Separator();
            ImGui::TextDisabled("LEVEL 2  |  Connected systems");
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
            ImGui::Separator();
            ImGui::TextDisabled("LEVEL 3  |  Interactive force fields");
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

            ImGui::Separator();
            ImGui::TextDisabled("LEVEL 4  |  Mathematical forms");
            if (ImGui::BeginTable("MathematicalPresetTable", 2))
            {
                const auto mathematicalButton = [&](const char* label,
                                                     int form)
                {
                    ImGui::TableNextColumn();
                    if (ImGui::Button(label, ImVec2(-1.f, 0.f)))
                    {
                        loadMathematicalWireframe(form);
                    }
                };
                mathematicalButton("3D heart", 1);
                mathematicalButton("Torus / donut", 0);
                mathematicalButton("Mobius strip", 3);
                mathematicalButton("Sphere", 4);
                mathematicalButton("Wave surface", 5);
                mathematicalButton("Double-well field", 6);
                mathematicalButton("Spacetime distortion", 2);
                ImGui::EndTable();
            }
            if (ImGui::SliderFloat(
                    "Distortion depth", &spacetimeWellDepth,
                    25.f, 235.f, "%.0f px"
                ))
            {
                loadMathematicalWireframe(2);
            }
            if (ImGui::SliderFloat(
                    "Wave height", &waveSurfaceHeight,
                    5.f, 100.f, "%.0f px"
                ))
            {
                loadMathematicalWireframe(5);
            }
            if (ImGui::SliderFloat(
                    "Double-well depth", &doubleWellDepth,
                    15.f, 180.f, "%.0f px"
                ))
            {
                loadMathematicalWireframe(6);
            }
            if (ImGui::Checkbox(
                    "Mesh diagonals", &mathematicalMeshDiagonals
                ) && activeMathematicalForm >= 0)
            {
                loadMathematicalWireframe(activeMathematicalForm);
            }
            ImGui::TextDisabled(
                "Alternating diagonals reveal triangular surface structure"
            );
            ImGui::TextDisabled(
                "Shallow curvature -> deep black-hole funnel analogy"
            );

            ImGui::Separator();
            ImGui::TextDisabled("LEVEL 5  |  Wireframe showcase");
            if (ImGui::Button("Particle apple"))
            {
                loadParticleApple();
            }
            ImGui::SameLine();
            if (ImGui::Button("Windblown tree"))
            {
                loadWindblownTree();
            }
            if (ImGui::Button("Rolling terrain"))
            {
                loadRollingTerrain();
            }
            if (ImGui::Checkbox(
                    "Terrain diagonals", &terrainMeshDiagonals
                ))
            {
                loadRollingTerrain();
            }
            ImGui::TextDisabled(
                "Triangulates hills only; buildings remain rectangular"
            );
        }
        ImGui::End();
        ImGui::Begin("Physics Controls");

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
                view.body.inverseMass == 0.f && view.showFixedOutline ? 3.f : 0.f
            );
            if (view.groupId == 58)
            {
                view.shape.setFillColor(sf::Color::Black);
                view.shape.setOutlineColor(sf::Color(110, 165, 255));
                view.shape.setOutlineThickness(3.f);
            }
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

        const auto isBuildingBody = [&](std::size_t index)
        {
            return index < bodies.size() &&
                bodies[index].groupId >= 60 && bodies[index].groupId <= 71;
        };
        const auto drawSpringConnections = [&](sf::RenderTarget& target,
                                                unsigned char alpha = 255,
                                                int buildingLayer = 0,
                                                int buildingGroup = -1)
        {
            for (const auto& spring : springs)
            {
                if (spring.first >= bodies.size() ||
                    spring.second >= bodies.size())
                {
                    continue;
                }
                const bool buildingConnection =
                    isBuildingBody(spring.first) ||
                    isBuildingBody(spring.second);
                if ((buildingLayer < 0 && buildingConnection) ||
                    (buildingLayer > 0 && !buildingConnection))
                {
                    continue;
                }
                if (buildingLayer > 0 && buildingGroup >= 60)
                {
                    const int connectionGroup = isBuildingBody(spring.first)
                        ? bodies[spring.first].groupId
                        : bodies[spring.second].groupId;
                    if (connectionGroup != buildingGroup)
                    {
                        continue;
                    }
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
        const auto drawBuildingOccluders = [&](sf::RenderTarget& target,
                                                int onlyGroup = -1)
        {
            for (int group = 60; group <= 71; ++group)
            {
                if (onlyGroup >= 60 && group != onlyGroup)
                {
                    continue;
                }
                std::vector<sf::Vector2f> points;
                for (const auto& view : bodies)
                {
                    if (view.groupId == group)
                    {
                        points.push_back(view.body.center());
                    }
                }
                if (points.size() < 3)
                {
                    continue;
                }

                // Trace the actual outer outline instead of covering the
                // building with an oversized rectangular blank area.
                std::sort(points.begin(), points.end(),
                    [](const sf::Vector2f& first, const sf::Vector2f& second)
                    {
                        return first.x < second.x ||
                            (first.x == second.x && first.y < second.y);
                    });
                const auto cross = [](const sf::Vector2f& origin,
                                      const sf::Vector2f& first,
                                      const sf::Vector2f& second)
                {
                    const sf::Vector2f a = first - origin;
                    const sf::Vector2f b = second - origin;
                    return a.x * b.y - a.y * b.x;
                };
                std::vector<sf::Vector2f> hull;
                for (const sf::Vector2f point : points)
                {
                    while (hull.size() >= 2 &&
                           cross(hull[hull.size() - 2], hull.back(), point) <= 0.f)
                    {
                        hull.pop_back();
                    }
                    hull.push_back(point);
                }
                const std::size_t lowerSize = hull.size();
                for (auto iterator = points.rbegin() + 1;
                     iterator != points.rend(); ++iterator)
                {
                    while (hull.size() > lowerSize &&
                           cross(hull[hull.size() - 2], hull.back(), *iterator) <= 0.f)
                    {
                        hull.pop_back();
                    }
                    hull.push_back(*iterator);
                }
                if (hull.size() > 1)
                {
                    hull.pop_back();
                }
                sf::ConvexShape silhouette(hull.size());
                for (std::size_t index = 0; index < hull.size(); ++index)
                {
                    silhouette.setPoint(index, hull[index]);
                }
                silhouette.setFillColor(sf::Color::Black);
                target.draw(silhouette);
            }
        };
        const auto drawBuildingsBackToFront = [&](sf::RenderTarget& target)
        {
            std::vector<std::pair<float, int>> buildingOrder;
            for (int group = 60; group <= 71; ++group)
            {
                float groundDepth = -std::numeric_limits<float>::infinity();
                bool found = false;
                for (const auto& view : bodies)
                {
                    if (view.groupId == group)
                    {
                        groundDepth = std::max(
                            groundDepth, view.body.center().y
                        );
                        found = true;
                    }
                }
                if (found)
                {
                    buildingOrder.emplace_back(groundDepth, group);
                }
            }
            std::sort(buildingOrder.begin(), buildingOrder.end());
            for (const auto [depth, group] : buildingOrder)
            {
                static_cast<void>(depth);
                drawBuildingOccluders(target, group);
                drawSpringConnections(target, 255, 1, group);
                for (const auto& view : bodies)
                {
                    if (view.groupId == group)
                    {
                        target.draw(view.shape);
                    }
                }
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
                if (isBuildingBody(index))
                {
                    continue;
                }
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
            drawSpringConnections(canvasTexture, 55, -1);
            drawBuildingsBackToFront(canvasTexture);
            canvasTexture.display();

            window.clear(background);
            const sf::Sprite canvasSprite(canvasTexture.getTexture());
            window.draw(canvasSprite);
            drawSpringConnections(window, 255, -1);
            drawBuildingsBackToFront(window);
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
            if (screenshotRequested)
            {
                saveScreenshot();
                screenshotRequested = false;
            }
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

        drawSpringConnections(window, 255, -1);

        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            if (!isBuildingBody(index))
            {
                window.draw(bodies[index].shape);
            }
        }
        drawBuildingsBackToFront(window);

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
        if (screenshotRequested)
        {
            saveScreenshot();
            screenshotRequested = false;
        }
    }

    ImGui::SFML::Shutdown();
    return 0;
}
