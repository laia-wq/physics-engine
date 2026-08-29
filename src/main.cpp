#include <SFML/Graphics.hpp>

#include <imgui-SFML.h>
#include <imgui.h>

#include "physics/Collision.hpp"

#include <algorithm>
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
}

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
    float floorFriction = 0.98f;
    float spawnRadius = 22.f;
    float spawnRestitution = 0.75f;
    float framesPerSecond = 0.f;

    const auto simulateStep = [&]()
    {
        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            if (selectedBody && index == *selectedBody)
            {
                continue;
            }

            bodies[index].body.acceleration = sf::Vector2f(0.f, gravity);
            bodies[index].body.integrate(FIXED_TIME_STEP);
            bodies[index].body.resolveBounds(
                windowWidth,
                windowHeight,
                floorFriction,
                MINIMUM_BOUNCE_SPEED
            );
        }

        for (std::size_t first = 0; first < bodies.size(); ++first)
        {
            for (std::size_t second = first + 1; second < bodies.size(); ++second)
            {
                physics::resolveCircleCollision(
                    bodies[first].body,
                    bodies[second].body
                );
            }
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
        ImGui::SetNextWindowSize(ImVec2(270.f, 0.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Physics Laboratory");
        ImGui::Text("Status: %s", paused ? "Paused" : "Running");
        ImGui::Text("Bodies: %zu", bodies.size());
        ImGui::Text("Render rate: %.0f FPS", framesPerSecond);
        ImGui::Text("Physics rate: 120 Hz");
        ImGui::Separator();
        ImGui::SliderFloat("Gravity", &gravity, -1000.f, 1500.f, "%.0f px/s^2");
        ImGui::SliderFloat("Spawn radius", &spawnRadius, 6.f, 60.f, "%.0f px");
        ImGui::SliderFloat("Restitution", &spawnRestitution, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Floor friction", &floorFriction, 0.8f, 1.f, "%.3f");
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
            simulateStep();
            stepRequested = false;
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

        for (const auto& view : bodies)
        {
            window.draw(view.shape);
        }

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}
