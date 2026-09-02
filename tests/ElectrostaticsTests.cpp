#include "physics/Electrostatics.hpp"

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

physics::CircleBody makeBody(float x, float charge)
{
    physics::CircleBody body(
        1.f, {x, 0.f}, {0.f, 0.f}, {0.f, 0.f}, 0.5f
    );
    body.charge = charge;
    return body;
}
}

int main()
{
    auto positiveA = makeBody(0.f, 1.f);
    auto positiveB = makeBody(10.f, 1.f);
    expect(physics::applyElectrostaticPair(
               positiveA, positiveB, 1000.f, 1.f),
           "two charged bodies create an interaction");
    expect(positiveA.acceleration.x < 0.f &&
               positiveB.acceleration.x > 0.f,
           "like charges repel symmetrically");

    auto positive = makeBody(0.f, 1.f);
    auto negative = makeBody(10.f, -1.f);
    physics::applyElectrostaticPair(positive, negative, 1000.f, 1.f);
    expect(positive.acceleration.x > 0.f && negative.acceleration.x < 0.f,
           "opposite charges attract");

    auto charged = makeBody(0.f, 1.f);
    auto neutral = makeBody(10.f, 0.f);
    expect(!physics::applyElectrostaticPair(
               charged, neutral, 1000.f, 1.f),
           "neutral bodies skip electrostatic interaction");
    expect(charged.acceleration.x == 0.f && neutral.acceleration.x == 0.f,
           "a skipped neutral pair does not change acceleration");

    if (failures == 0)
    {
        std::cout << "All electrostatics tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
