#include <SFML/Graphics.hpp>

#include "physics/Collision.hpp"

#include <cmath>
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
}

struct PhysicsBody
{
    sf::CircleShape shape;

    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Vector2f acceleration;

    float radius;
    float restitution;

    PhysicsBody(
        float r,
        sf::Vector2f startPosition,
        sf::Vector2f startVelocity,
        float bounce
    )
        : shape(r),
          position(startPosition),
          velocity(startVelocity),
          acceleration(0.f, GRAVITY),
          radius(r),
          restitution(bounce)
    {
        shape.setPosition(position);
    }

    void update(float dt)
    {
        velocity += acceleration * dt;
        position += velocity * dt;
    }

    void handleWindowCollisions(float width, float height)
    {
        // Left wall
        if (position.x < 0.f)
        {
            position.x = 0.f;
            velocity.x = -velocity.x * restitution;
        }

        // Right wall
        if (position.x + radius * 2.f > width)
        {
            position.x = width - radius * 2.f;
            velocity.x = -velocity.x * restitution;
        }

        // Ceiling
        if (position.y < 0.f)
        {
            position.y = 0.f;
            velocity.y = -velocity.y * restitution;
        }

        // Floor
        if (position.y + radius * 2.f > height)
        {
            position.y = height - radius * 2.f;

            velocity.y = -velocity.y * restitution;

            // Basic friction
            velocity.x *= FLOOR_FRICTION;

            // Stop tiny bouncing
            if (std::abs(velocity.y) < MINIMUM_BOUNCE_SPEED)
            {
                velocity.y = 0.f;
            }
        }
    }

    void syncShape()
    {
        shape.setPosition(position);
    }

    sf::Vector2f center() const
    {
        return position + sf::Vector2f(radius, radius);
    }
};

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

    std::vector<PhysicsBody> bodies;

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
                }
            }
        }

        float frameTime = clock.restart().asSeconds();

        // Avoid trying to simulate an unbounded backlog after a long pause.
        if (frameTime > MAX_FRAME_TIME)
        {
            frameTime = MAX_FRAME_TIME;
        }

        accumulator += frameTime;

        while (accumulator >= FIXED_TIME_STEP)
        {
            for (auto& body : bodies)
            {
                body.update(FIXED_TIME_STEP);

                body.handleWindowCollisions(
                    windowWidth,
                    windowHeight
                );
            }

            accumulator -= FIXED_TIME_STEP;
        }

        for (auto& body : bodies)
        {
            body.shape.setFillColor(sf::Color::White);
            body.syncShape();
        }

        for (std::size_t first = 0; first < bodies.size(); ++first)
        {
            for (std::size_t second = first + 1; second < bodies.size(); ++second)
            {
                if (physics::circlesOverlap(
                        bodies[first].center(),
                        bodies[first].radius,
                        bodies[second].center(),
                        bodies[second].radius
                    ))
                {
                    bodies[first].shape.setFillColor(sf::Color::Red);
                    bodies[second].shape.setFillColor(sf::Color::Red);
                }
            }
        }

        window.clear();

        for (const auto& body : bodies)
        {
            window.draw(body.shape);
        }

        window.display();
    }

    return 0;
}