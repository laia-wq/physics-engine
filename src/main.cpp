#include <SFML/Graphics.hpp>

#include <imgui-SFML.h>
#include <imgui.h>

#include "physics/BroadPhase.hpp"
#include "physics/Collision.hpp"

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
constexpr float GRAVITY = 500.f;
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
};

struct CircleView
{
    physics::CircleBody body;
    sf::CircleShape shape;

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
              sf::Vector2f(0.f, GRAVITY),
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

std::optional<std::size_t> findBodyAt(
    const std::vector<CircleView>& bodies,
    sf::Vector2f point
)
{
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
    sf::RenderWindow& window,
    sf::Vector2f start,
    sf::Vector2f end,
    sf::Color color
)
{
    const std::array vertices{
        sf::Vertex{start, color},
        sf::Vertex{end, color}
    };
    window.draw(vertices.data(), vertices.size(), sf::PrimitiveType::Lines);
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

    const auto loadClassicScene = [&bodies, &restartBodies]()
    {
        bodies.clear();
        bodies.emplace_back(
            25.f,
            sf::Vector2f(100.f, 100.f),
            sf::Vector2f(180.f, 0.f),
            0.75f
        );
        bodies.emplace_back(
            35.f,
            sf::Vector2f(300.f, 80.f),
            sf::Vector2f(-100.f, 0.f),
            0.55f
        );
        bodies.emplace_back(
            18.f,
            sf::Vector2f(500.f, 50.f),
            sf::Vector2f(70.f, 0.f),
            0.9f
        );
        restartBodies = bodies;
    };

    loadClassicScene();

    const auto loadStressScene = [&bodies, &restartBodies](std::size_t bodyCount)
    {
        bodies.clear();
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

    const auto loadHeadOnScene = [&bodies, &restartBodies]()
    {
        bodies.clear();
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

    const auto loadZeroGravityScene = [&bodies, &restartBodies]()
    {
        bodies.clear();
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

    const auto loadRainScene = [&bodies, &restartBodies]()
    {
        bodies.clear();
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

    const auto loadOrbitScene = [&bodies, &restartBodies]()
    {
        bodies.clear();
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
    float gravityField[2] = {0.f, GRAVITY};
    float windForce[2] = {0.f, 0.f};
    bool gravityEnabled = true;
    bool windEnabled = true;
    float windSensitivity = 1.f;
    bool canvasMode = false;
    bool canvasTrails = true;
    bool showControls = true;
    bool placingPointField = false;
    bool draggingPointField = false;
    std::size_t canvasPalette = 0;
    std::vector<PointField> pointFields;
    std::optional<std::size_t> selectedPointField;
    int interactionTool = 0;
    float pulseRadialStrength = 8000000.f;
    float pulseVortexStrength = 0.f;
    float pulseDuration = 1.f;
    constexpr float FIELD_SOFTENING = 35.f;
    float floorFriction = 0.98f;
    float spawnRadius = 22.f;
    float spawnMass = spawnRadius * spawnRadius;
    bool automaticSpawnMass = true;
    float spawnRestitution = 0.75f;
    std::size_t populationCount = 300;
    float populationRadii[3] = {5.f, 12.f, 24.f};
    float populationPercentages[3] = {60.f, 30.f, 10.f};
    float framesPerSecond = 0.f;
    bool showVelocityVectors = false;
    bool showContacts = false;
    bool showCollisionNormals = false;
    bool showSpatialGrid = false;
    bool bodyCollisionsEnabled = true;
    bool useSpatialGrid = false;
    float gridCellSize = 50.f;
    std::vector<DebugContact> debugContacts;
    std::size_t allPairsCount = 0;
    std::size_t candidatePairChecks = 0;
    std::size_t actualCollisions = 0;
    double physicsStepMilliseconds = 0.0;

    const auto pointFieldAcceleration = [&](const physics::CircleBody& body)
    {
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
            totalAcceleration +=
                direction * (field.radialStrength * oscillation /
                    softenedDistanceSquared) +
                tangent * (field.vortexStrength * oscillation /
                    softenedDistanceSquared);
        }
        return totalAcceleration;
    };

    const auto windAcceleration = [&](const physics::CircleBody& body)
    {
        if (!windEnabled || body.inverseMass <= 0.f)
        {
            return sf::Vector2f(0.f, 0.f);
        }

        // In 2D, exposed width grows with radius while default mass grows
        // with radius squared. Small circles therefore respond more strongly.
        const float aerodynamicResponse =
            2.f * body.radius * body.inverseMass * windSensitivity;
        return sf::Vector2f(windForce[0], windForce[1]) * aerodynamicResponse;
    };

    const auto loadPopulation = [&](std::size_t bodyCount)
    {
        bodies.clear();
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
        }
        restartBodies = bodies;
        pointFields.clear();
        selectedPointField.reset();
        placingPointField = false;
        draggingPointField = false;
        gravityEnabled = true;
        gravityField[0] = 0.f;
        gravityField[1] = GRAVITY;
        windEnabled = true;
        windForce[0] = 0.f;
        windForce[1] = 0.f;
        useSpatialGrid = bodyCount >= 300;
        gridCellSize = bodyCount >= 600 ? 20.f : 50.f;
        bodyCollisionsEnabled = bodyCount < 600;
        selectedBody.reset();
        draggedBody.reset();
        accumulator = 0.f;
    };

    const auto rebalancePopulationPercentages = [&](std::size_t changedIndex)
    {
        const std::size_t firstOther = (changedIndex + 1) % 3;
        const std::size_t secondOther = (changedIndex + 2) % 3;
        const float remaining = 100.f - populationPercentages[changedIndex];
        const float previousOtherTotal =
            populationPercentages[firstOther] +
            populationPercentages[secondOther];

        if (previousOtherTotal > 0.001f)
        {
            populationPercentages[firstOther] = remaining *
                populationPercentages[firstOther] / previousOtherTotal;
        }
        else
        {
            populationPercentages[firstOther] = remaining * 0.5f;
        }
        populationPercentages[secondOther] =
            remaining - populationPercentages[firstOther];
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
                rebalancePopulationPercentages(index);
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

    const auto simulateStep = [&]()
    {
        simulationTime += FIXED_TIME_STEP;
        const auto stepStart = std::chrono::steady_clock::now();
        candidatePairChecks = 0;
        actualCollisions = 0;
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
                pointFieldAcceleration(bodies[index].body);
            bodies[index].body.acceleration +=
                windAcceleration(bodies[index].body);
            bodies[index].body.integrate(FIXED_TIME_STEP);
            bodies[index].body.resolveBounds(
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
                    bodies.erase(bodies.begin() +
                        static_cast<std::ptrdiff_t>(*selectedBody));
                    selectedBody.reset();
                    draggedBody.reset();
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
                            pulseDuration
                        });
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
                    }
                }
            }

            if (const auto* mouseReleased =
                    event->getIf<sf::Event::MouseButtonReleased>())
            {
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
                gravityField[1] = GRAVITY;
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

        if (ImGui::CollapsingHeader("Preset scenes"))
        {
            if (ImGui::Button("Classic"))
            {
                loadClassicScene();
                gravityField[0] = 0.f;
                gravityField[1] = GRAVITY;
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
            if (ImGui::Button("Attractor"))
            {
                loadStressScene(300);
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
                    windAcceleration(selected) +
                    pointFieldAcceleration(selected);
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
                if (ImGui::Button("Delete selected"))
                {
                    bodies.erase(bodies.begin() +
                        static_cast<std::ptrdiff_t>(*selectedBody));
                    selectedBody.reset();
                    draggedBody.reset();
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
            ImGui::SliderFloat("Floor friction", &floorFriction, 0.8f, 1.f, "%.3f");
        }

        if (ImGui::CollapsingHeader("Debug visualization"))
        {
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
            view.shape.setFillColor(sf::Color::White);
            view.sync();
        }

        for (std::size_t first = 0; first < bodies.size(); ++first)
        {
            for (std::size_t second = first + 1; second < bodies.size(); ++second)
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

        if (selectedBody && *selectedBody < bodies.size())
        {
            bodies[*selectedBody].shape.setFillColor(sf::Color(255, 215, 0));
        }

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
                canvasShape.setFillColor(
                    canvasPalettes[canvasPalette][index % 5]
                );
                canvasTexture.draw(canvasShape, sf::BlendAdd);
            }
            canvasTexture.display();

            window.clear(background);
            const sf::Sprite canvasSprite(canvasTexture.getTexture());
            window.draw(canvasSprite);
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

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}
