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

    std::vector<CircleView> bodies;

    const auto resetScene = [&bodies]()
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
    };

    resetScene();

    const auto loadStressScene = [&bodies](std::size_t bodyCount)
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
    };

    sf::Clock clock;
    float accumulator = 0.f;
    bool paused = false;
    bool stepRequested = false;
    std::optional<std::size_t> selectedBody;
    float selectedInverseMass = 0.f;
    sf::Vector2f dragOffset;
    sf::Vector2f lastMousePosition;
    sf::Vector2f throwVelocity;
    float gravity = GRAVITY;
    float windForce = 0.f;
    float floorFriction = 0.98f;
    float spawnRadius = 22.f;
    float spawnRestitution = 0.75f;
    float framesPerSecond = 0.f;
    bool showVelocityVectors = false;
    bool showContacts = false;
    bool showCollisionNormals = false;
    bool showSpatialGrid = false;
    bool useSpatialGrid = false;
    float gridCellSize = 50.f;
    std::vector<DebugContact> debugContacts;
    std::size_t allPairsCount = 0;
    std::size_t candidatePairChecks = 0;
    std::size_t actualCollisions = 0;
    double physicsStepMilliseconds = 0.0;

    const auto simulateStep = [&]()
    {
        const auto stepStart = std::chrono::steady_clock::now();
        candidatePairChecks = 0;
        actualCollisions = 0;
        allPairsCount = bodies.size() * (bodies.size() - (bodies.empty() ? 0 : 1)) / 2;

        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            if (selectedBody && index == *selectedBody)
            {
                continue;
            }

            bodies[index].body.acceleration = sf::Vector2f(0.f, gravity);
            bodies[index].body.applyForce(sf::Vector2f(windForce, 0.f));
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

        if (useSpatialGrid)
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
        else
        {
            for (std::size_t first = 0; first < bodies.size(); ++first)
            {
                for (std::size_t second = first + 1; second < bodies.size(); ++second)
                {
                    processPair(first, second);
                }
            }
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

                if (keyPressed->scancode == sf::Keyboard::Scancode::R)
                {
                    resetScene();
                    selectedBody.reset();
                    paused = false;
                    accumulator = 0.f;
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
            }

            if (const auto* mousePressed =
                    event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mousePressed->button == sf::Mouse::Button::Left)
                {
                    if (ImGui::GetIO().WantCaptureMouse)
                    {
                        continue;
                    }

                    const sf::Vector2f mousePosition =
                        toWorldPosition(mousePressed->position);
                    selectedBody = findBodyAt(bodies, mousePosition);

                    if (selectedBody)
                    {
                        auto& selected = bodies[*selectedBody].body;
                        selectedInverseMass = selected.inverseMass;
                        selected.inverseMass = 0.f;
                        selected.velocity = sf::Vector2f(0.f, 0.f);
                        dragOffset = mousePosition - selected.position;
                        lastMousePosition = mousePosition;
                        throwVelocity = sf::Vector2f(0.f, 0.f);
                    }
                    else
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
                    }
                }
            }

            if (const auto* mouseReleased =
                    event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (
                    mouseReleased->button == sf::Mouse::Button::Left &&
                    selectedBody && *selectedBody < bodies.size()
                )
                {
                    auto& selected = bodies[*selectedBody].body;
                    selected.inverseMass = selectedInverseMass;
                    selected.velocity = throwVelocity;
                    selectedBody.reset();
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

        ImGui::SetNextWindowPos(ImVec2(12.f, 12.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.f, 0.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Physics Laboratory");
        ImGui::Text("Status: %s", paused ? "Paused" : "Running");
        ImGui::Text("Bodies: %zu", bodies.size());
        ImGui::Text("Render rate: %.0f FPS", framesPerSecond);
        ImGui::Text("Physics rate: 120 Hz");
        ImGui::Text("Search: %s", useSpatialGrid ? "Uniform grid" : "All pairs");
        ImGui::Text("All possible pairs: %zu", allPairsCount);
        ImGui::Text("Candidate checks: %zu", candidatePairChecks);
        const float candidateReduction = allPairsCount > 0
            ? 100.f * (1.f - static_cast<float>(candidatePairChecks) /
                static_cast<float>(allPairsCount))
            : 0.f;
        ImGui::Text("Candidate reduction: %.1f%%", candidateReduction);
        ImGui::Text("Actual contacts: %zu", actualCollisions);
        ImGui::Text("Physics step: %.3f ms", physicsStepMilliseconds);
        ImGui::Checkbox("Use spatial grid", &useSpatialGrid);
        ImGui::BeginDisabled(!useSpatialGrid);
        ImGui::SliderFloat("Grid cell size", &gridCellSize, 20.f, 120.f, "%.0f px");
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::SliderFloat("Gravity", &gravity, -1000.f, 1500.f, "%.0f px/s^2");
        ImGui::SliderFloat("Wind force", &windForce, -5000.f, 5000.f, "%.0f");
        ImGui::SliderFloat("Spawn radius", &spawnRadius, 6.f, 60.f, "%.0f px");
        ImGui::SliderFloat("Restitution", &spawnRestitution, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Floor friction", &floorFriction, 0.8f, 1.f, "%.3f");
        ImGui::Separator();
        ImGui::Text("Debug visualization");
        ImGui::Checkbox("Velocity vectors", &showVelocityVectors);
        ImGui::Checkbox("Contact points", &showContacts);
        ImGui::Checkbox("Collision normals", &showCollisionNormals);
        ImGui::BeginDisabled(!useSpatialGrid);
        ImGui::Checkbox("Spatial grid", &showSpatialGrid);
        ImGui::EndDisabled();
        ImGui::Separator();

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
        if (ImGui::Button("Reset"))
        {
            resetScene();
            selectedBody.reset();
            accumulator = 0.f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            bodies.clear();
            selectedBody.reset();
        }
        ImGui::TextDisabled("Click: spawn  Drag: throw");
        ImGui::TextDisabled("Space: pause  N: step  R: reset");
        ImGui::Separator();
        ImGui::Text("Performance scenes");
        if (ImGui::Button("Load 100 bodies"))
        {
            loadStressScene(100);
            selectedBody.reset();
            accumulator = 0.f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load 300 bodies"))
        {
            loadStressScene(300);
            selectedBody.reset();
            accumulator = 0.f;
        }
        ImGui::End();

        if (selectedBody && *selectedBody < bodies.size())
        {
            const sf::Vector2f mousePosition =
                toWorldPosition(sf::Mouse::getPosition(window));
            const sf::Vector2f mouseMovement = mousePosition - lastMousePosition;

            if (frameTime > 0.0001f)
            {
                throwVelocity = mouseMovement / frameTime;
                limitMagnitude(throwVelocity, MAX_THROW_SPEED);
            }

            auto& selected = bodies[*selectedBody].body;
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

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}
