#include "physics/BroadPhase.hpp"

#include <algorithm>
#include <iostream>
#include <string_view>
#include <vector>

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

bool containsPair(
    const std::vector<physics::BodyPair>& pairs,
    physics::BodyPair expected
)
{
    return std::find(pairs.begin(), pairs.end(), expected) != pairs.end();
}
}

int main()
{
    {
        const std::vector<physics::CircleBounds> bodies{
            {{0.f, 0.f}, 5.f},
            {{200.f, 200.f}, 5.f}
        };
        expect(
            physics::buildUniformGridPairs(bodies, 50.f).empty(),
            "distant bodies are not candidates"
        );
    }

    {
        const std::vector<physics::CircleBounds> bodies{
            {{5.f, 5.f}, 10.f},
            {{20.f, 10.f}, 10.f}
        };
        const auto pairs = physics::buildUniformGridPairs(bodies, 50.f);
        expect(
            pairs.size() == 1 && containsPair(pairs, {0, 1}),
            "nearby bodies become one candidate pair"
        );
    }

    {
        const std::vector<physics::CircleBounds> bodies{
            {{40.f, 10.f}, 10.f},
            {{55.f, 10.f}, 10.f}
        };
        const auto pairs = physics::buildUniformGridPairs(bodies, 50.f);
        expect(
            pairs.size() == 1 && containsPair(pairs, {0, 1}),
            "bodies crossing a cell boundary are still paired once"
        );
    }

    if (failures == 0)
    {
        std::cout << "All broad-phase tests passed.\n";
    }

    return failures == 0 ? 0 : 1;
}
