#include "presets/PresetConstruction.hpp"

#include <iostream>
#include <string_view>

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

bool sameBody(
    const presets::BodyDefinition& first,
    const presets::BodyDefinition& second
)
{
    return first.radius == second.radius &&
        first.position == second.position &&
        first.velocity == second.velocity &&
        first.restitution == second.restitution;
}

bool hasValidConnections(const presets::ConnectedPreset& preset)
{
    for (const auto& connection : preset.connections)
    {
        if (connection.first >= preset.bodies.size() ||
            connection.second >= preset.bodies.size() ||
            connection.first == connection.second ||
            connection.restLength <= 0.f)
        {
            return false;
        }
    }
    return true;
}
}

int main()
{
    using presets::FoundationPreset;
    expect(presets::buildFoundationPreset(FoundationPreset::Classic).size() == 3,
           "classic preset loads three bodies");
    expect(presets::buildFoundationPreset(FoundationPreset::HeadOn).size() == 2,
           "head-on preset loads two bodies");
    expect(presets::buildFoundationPreset(FoundationPreset::ZeroGravity).size() == 16,
           "zero-gravity preset loads sixteen bodies");
    expect(presets::buildFoundationPreset(FoundationPreset::Rain).size() == 60,
           "rain preset loads sixty bodies");
    expect(presets::buildFoundationPreset(FoundationPreset::Orbit).size() == 120,
           "orbit preset loads one hundred twenty bodies");
    expect(presets::buildFoundationPreset(FoundationPreset::Stress, 1000).size() == 1000,
           "stress preset respects requested body count");

    const auto original =
        presets::buildFoundationPreset(FoundationPreset::Orbit);
    auto running = original;
    running.front().position = {0.f, 0.f};
    running.front().velocity = {999.f, 999.f};
    const auto restarted =
        presets::buildFoundationPreset(FoundationPreset::Orbit);
    expect(sameBody(original.front(), restarted.front()),
           "reloading a preset restores its initial body state");
    expect(!sameBody(running.front(), restarted.front()),
           "restart data is independent of mutated simulation data");

    const auto chain = presets::buildSpringChain(80, 20.f, 800.f, 8000.f, 300.f);
    expect(chain.bodies.size() == 80 && chain.connections.size() == 79,
           "chain connects every adjacent particle exactly once");
    expect(chain.pinnedBodies.size() == 1 && chain.pinnedBodies[0] == 0,
           "chain pins its first particle");
    expect(hasValidConnections(chain), "chain connection indices are valid");

    const auto lattice =
        presets::buildSoftBodyLattice(12, 8, 20.f, 800.f, 600.f);
    const std::size_t expectedLatticeConnections =
        8 * 11 + 7 * 12 + 2 * 7 * 11;
    expect(lattice.bodies.size() == 96 &&
               lattice.connections.size() == expectedLatticeConnections,
           "lattice builds horizontal, vertical, and diagonal connections");
    expect(lattice.pinnedBodies.size() == 2,
           "lattice pins both top corners");
    expect(hasValidConnections(lattice),
           "lattice connection indices are valid");

    const auto web = presets::buildRadialWeb(6, 16);
    expect(web.bodies.size() == 97 && web.connections.size() == 192,
           "web builds rings and radial spokes");
    expect(web.pinnedBodies.size() == 1 && web.pinnedBodies[0] == 0,
           "web pins its center particle");
    expect(hasValidConnections(web), "web connection indices are valid");

    const auto apple = presets::buildParticleApple(800.f, 600.f);
    expect(apple.bodies.size() == 1674 && apple.connections.size() == 6425,
           "apple builds its curved body mesh and attached stem");
    expect(apple.pinnedBodies.size() == 1 &&
               apple.pinnedBodies[0] == apple.bodies.size() - 1,
           "apple pins the tip of its structural stem");
    expect(apple.bodies.front().material == presets::Material::Flexible &&
               apple.bodies.back().material == presets::Material::Fixed,
           "apple preserves flexible fruit and fixed stem materials");
    expect(hasValidConnections(apple),
           "apple connection indices and rest lengths are valid");

    if (failures == 0)
    {
        std::cout << "All preset construction tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
