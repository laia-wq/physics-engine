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

    if (failures == 0)
    {
        std::cout << "All preset construction tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
