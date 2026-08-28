#include <SFML/Graphics.hpp>

#include "physics/Collision.hpp"

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
            for (auto& view : bodies)
            {
                view.body.integrate(FIXED_TIME_STEP);

                view.body.resolveBounds(
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

        window.clear();

        for (const auto& view : bodies)
        {
            window.draw(view.shape);
        }

        window.display();
    }

    return 0;
}