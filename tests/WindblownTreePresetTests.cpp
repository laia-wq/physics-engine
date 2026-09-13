#include "presets/WindblownTreePreset.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view name)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << name << '\n';
        ++failures;
    }
}
}

int main()
{
    std::vector<app::CircleView> bodies;
    std::vector<physics::Spring> springs;
    presets::buildWindblownTree(bodies, springs);
    expect(bodies.size() > 1000, "tree contains a detailed particle mesh");
    expect(springs.size() > bodies.size(),
           "tree contains a connected structural graph");

    bool validConnections = true;
    for (const auto& spring : springs)
    {
        validConnections = validConnections &&
            spring.first < bodies.size() && spring.second < bodies.size() &&
            spring.first != spring.second && spring.restLength > 0.f;
    }
    expect(validConnections, "tree connections reference valid particles");

    std::size_t fixedBodies = 0;
    std::size_t fragileBodies = 0;
    for (const auto& body : bodies)
    {
        fixedBodies += body.body.inverseMass == 0.f ? 1 : 0;
        fragileBodies += body.material == app::ParticleMaterial::Fragile ? 1 : 0;
    }
    expect(fixedBodies > 10, "tree includes stable terrain and trunk anchors");
    expect(fragileBodies > 100, "tree includes detachable foliage particles");

    const std::size_t bodyCount = bodies.size();
    const std::size_t springCount = springs.size();
    const sf::Vector2f firstPosition = bodies.front().body.position;
    bodies.front().body.position = {999.f, 999.f};
    presets::buildWindblownTree(bodies, springs);
    expect(bodies.size() == bodyCount && springs.size() == springCount &&
               bodies.front().body.position == firstPosition,
           "reloading the tree restores repeatable construction");

    if (failures == 0)
    {
        std::cout << "All windblown-tree preset tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
