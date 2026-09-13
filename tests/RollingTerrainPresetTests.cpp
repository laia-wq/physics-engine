#include "presets/RollingTerrainPreset.hpp"

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
    presets::buildRollingTerrain(bodies, springs, false);
    expect(bodies.size() > 1500, "terrain contains a dense full-screen mesh");
    expect(springs.size() > bodies.size(),
           "terrain contains a connected surface and architecture");

    bool validConnections = true;
    std::size_t fixedBodies = 0;
    std::size_t buildingBodies = 0;
    for (const auto& spring : springs)
    {
        validConnections = validConnections &&
            spring.first < bodies.size() && spring.second < bodies.size() &&
            spring.first != spring.second && spring.restLength > 0.f;
    }
    for (const auto& body : bodies)
    {
        fixedBodies += body.body.inverseMass == 0.f ? 1 : 0;
        buildingBodies += body.groupId >= 60 && body.groupId <= 71 ? 1 : 0;
    }
    expect(validConnections, "terrain connections reference valid particles");
    expect(fixedBodies > 100, "terrain has stable screen-edge anchors");
    expect(buildingBodies > 100, "city contains substantial 3D architecture");

    const std::size_t bodyCount = bodies.size();
    const std::size_t rectangularConnectionCount = springs.size();
    const sf::Vector2f firstPosition = bodies.front().body.position;
    bodies.front().body.position = {999.f, 999.f};
    presets::buildRollingTerrain(bodies, springs, false);
    expect(bodies.size() == bodyCount &&
               springs.size() == rectangularConnectionCount &&
               bodies.front().body.position == firstPosition,
           "reloading terrain restores repeatable construction");

    presets::buildRollingTerrain(bodies, springs, true);
    expect(bodies.size() == bodyCount &&
               springs.size() > rectangularConnectionCount,
           "terrain diagonals add connections without changing geometry");

    if (failures == 0)
    {
        std::cout << "All rolling-terrain preset tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
