#include "physics/Collision.hpp"

#include <iostream>
#include <string_view>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view testName)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << testName << '\n';
        ++failures;
    }
}
}

int main()
{
    expect(
        !physics::circlesOverlap({0.f, 0.f}, 1.f, {3.f, 0.f}, 1.f),
        "separated circles do not overlap"
    );
    expect(
        physics::circlesOverlap({0.f, 0.f}, 1.f, {2.f, 0.f}, 1.f),
        "touching circles count as a collision"
    );
    expect(
        physics::circlesOverlap({0.f, 0.f}, 2.f, {2.f, 0.f}, 1.f),
        "intersecting circles overlap"
    );
    expect(
        physics::circlesOverlap({4.f, -2.f}, 1.f, {4.f, -2.f}, 1.f),
        "circles with the same centre overlap"
    );
    expect(
        !physics::circlesOverlap({0.f, 0.f}, 1.f, {1.5f, 1.5f}, 1.f),
        "diagonally separated circles do not overlap"
    );

    if (failures == 0)
    {
        std::cout << "All collision tests passed.\n";
    }

    return failures == 0 ? 0 : 1;
}
