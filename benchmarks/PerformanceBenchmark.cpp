#include "physics/BroadPhase.hpp"
#include "physics/Collision.hpp"

#include <SFML/System/Vector2.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <vector>

namespace
{
constexpr float WORLD_WIDTH = 800.f;
constexpr float WORLD_HEIGHT = 600.f;
constexpr int WARMUP_RUNS = 5;
constexpr int MEASURED_RUNS = 80;

std::vector<physics::CircleBounds> makeScene(std::size_t bodyCount)
{
    std::vector<physics::CircleBounds> bodies;
    bodies.reserve(bodyCount);
    for (std::size_t index = 0; index < bodyCount; ++index)
    {
        const float radius = 4.f + static_cast<float>((index * 17U) % 5U);
        const float availableWidth = WORLD_WIDTH - radius * 2.f;
        const float availableHeight = WORLD_HEIGHT - radius * 2.f;
        const float x = std::fmod(
            17.f + static_cast<float>(index * 97U), availableWidth
        );
        const float y = std::fmod(
            23.f + static_cast<float>(index * 53U), availableHeight
        );
        bodies.push_back({sf::Vector2f(x, y), radius});
    }
    return bodies;
}

std::size_t countBruteForceContacts(
    const std::vector<physics::CircleBounds>& bodies
)
{
    std::size_t contacts = 0;
    for (std::size_t first = 0; first < bodies.size(); ++first)
    {
        for (std::size_t second = first + 1; second < bodies.size(); ++second)
        {
            contacts += physics::circlesOverlap(
                bodies[first].position + sf::Vector2f(
                    bodies[first].radius, bodies[first].radius
                ),
                bodies[first].radius,
                bodies[second].position + sf::Vector2f(
                    bodies[second].radius, bodies[second].radius
                ),
                bodies[second].radius
            ) ? 1U : 0U;
        }
    }
    return contacts;
}

std::size_t countGridContacts(
    const std::vector<physics::CircleBounds>& bodies,
    float cellSize,
    std::size_t& candidateCount
)
{
    const auto pairs = physics::buildUniformGridPairs(bodies, cellSize);
    candidateCount = pairs.size();
    std::size_t contacts = 0;
    for (const auto [first, second] : pairs)
    {
        contacts += physics::circlesOverlap(
            bodies[first].position + sf::Vector2f(
                bodies[first].radius, bodies[first].radius
            ),
            bodies[first].radius,
            bodies[second].position + sf::Vector2f(
                bodies[second].radius, bodies[second].radius
            ),
            bodies[second].radius
        ) ? 1U : 0U;
    }
    return contacts;
}

template <typename Operation>
double averageMilliseconds(Operation operation)
{
    for (int run = 0; run < WARMUP_RUNS; ++run) operation();
    const auto start = std::chrono::steady_clock::now();
    for (int run = 0; run < MEASURED_RUNS; ++run) operation();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() /
        static_cast<double>(MEASURED_RUNS);
}
}

int main()
{
    const std::array<std::size_t, 4> bodyCounts{100, 300, 600, 1000};
    std::cout << "Deterministic collision-search benchmark\n"
              << "Average of " << MEASURED_RUNS
              << " runs after " << WARMUP_RUNS << " warmups\n\n"
              << "| Bodies | All pairs | Grid candidates | Reduction | "
                 "Brute force (ms) | Grid (ms) | Speedup |\n"
              << "|---:|---:|---:|---:|---:|---:|---:|\n";

    std::size_t resultGuard = 0;
    for (const std::size_t bodyCount : bodyCounts)
    {
        const auto bodies = makeScene(bodyCount);
        const float cellSize = bodyCount >= 600 ? 20.f : 50.f;
        std::size_t candidateCount = 0;
        const std::size_t bruteContacts = countBruteForceContacts(bodies);
        const std::size_t gridContacts =
            countGridContacts(bodies, cellSize, candidateCount);
        if (bruteContacts != gridContacts)
        {
            std::cerr << "Grid missed contacts for " << bodyCount
                      << " bodies.\n";
            return 1;
        }

        const double bruteMilliseconds = averageMilliseconds([&]()
        {
            resultGuard += countBruteForceContacts(bodies);
        });
        const double gridMilliseconds = averageMilliseconds([&]()
        {
            std::size_t candidates = 0;
            resultGuard += countGridContacts(bodies, cellSize, candidates);
        });
        const std::size_t allPairs = bodyCount * (bodyCount - 1U) / 2U;
        const double reduction = 100.0 *
            (1.0 - static_cast<double>(candidateCount) /
                static_cast<double>(allPairs));
        const double speedup = bruteMilliseconds / gridMilliseconds;

        std::cout << "| " << bodyCount << " | " << allPairs << " | "
                  << candidateCount << " | " << std::fixed
                  << std::setprecision(1) << reduction << "% | "
                  << std::setprecision(3) << bruteMilliseconds << " | "
                  << gridMilliseconds << " | " << std::setprecision(2)
                  << speedup << "x |\n";
    }

    // The measured work contributes to an observable result, preventing an
    // optimizing compiler from discarding it as unused.
    if (resultGuard == 0) std::cerr << "No contacts in benchmark scene.\n";
    return 0;
}
