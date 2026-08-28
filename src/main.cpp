#include <SFML/Graphics.hpp>

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
constexpr float FLOOR_FRICTION = 0.98f;
constexpr float MINIMUM_BOUNCE_SPEED = 15.f;
constexpr float FIXED_TIME_STEP = 1.f / 120.f;
constexpr float MAX_FRAME_TIME = 0.25f;
constexpr float SPAWN_RADIUS = 22.f;
constexpr float SPAWN_RESTITUTION = 0.75f;
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

    std::vector<CircleView> bodies;

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

    sf::Clock clock;
    float accumulator = 0.f;
    bool paused = false;
    bool stepRequested = false;
    std::optional<std::size_t> selectedBody;
    float selectedInverseMass = 0.f;
    sf::Vector2f dragOffset;
    sf::Vector2f lastMousePosition;
    sf::Vector2f throwVelocity;

    const auto simulateStep = [&]()
    {
        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            if (selectedBody && index == *selectedBody)
            {
                continue;
            }

            bodies[index].body.integrate(FIXED_TIME_STEP);
            bodies[index].body.resolveBounds(
                windowWidth,
                windowHeight,
                FLOOR_FRICTION,
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
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* keyPressed =
                    event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->scancode == sf::Keyboard::Scancode::R)
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
                            mousePosition.x - SPAWN_RADIUS,
                            0.f,
                            windowWidth - SPAWN_RADIUS * 2.f
                        );
                        const float y = std::clamp(
                            mousePosition.y - SPAWN_RADIUS,
                            0.f,
                            windowHeight - SPAWN_RADIUS * 2.f
                        );
                        bodies.emplace_back(
                            SPAWN_RADIUS,
                            sf::Vector2f(x, y),
                            sf::Vector2f(0.f, 0.f),
                            SPAWN_RESTITUTION
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

        window.display();
    }

    return 0;
}