#pragma once

#include "physics/CircleBody.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>

namespace app
{
enum class ParticleMaterial
{
    Structural,
    Flexible,
    Fragile,
    Fixed
};

inline const char* materialName(ParticleMaterial material)
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

inline float materialForceResponse(ParticleMaterial material)
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

inline float materialBreakingScale(ParticleMaterial material)
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

inline sf::Color materialColor(ParticleMaterial material)
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
}
