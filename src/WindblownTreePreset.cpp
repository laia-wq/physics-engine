#include "presets/WindblownTreePreset.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace presets
{
using app::ParticleMaterial;

void buildWindblownTree(
    std::vector<app::CircleView>& bodies,
    std::vector<physics::Spring>& springs
)
{
    constexpr float RADIUS = 1.65f;
    bodies.clear();
    springs.clear();

    const auto addParticle = [&](sf::Vector2f center,
                                 ParticleMaterial material,
                                 int group,
                                 bool fixed = false)
    {
        bodies.emplace_back(
            RADIUS,
            center - sf::Vector2f(RADIUS, RADIUS),
            sf::Vector2f(0.f, 0.f),
            0.2f
        );
        auto& particle = bodies.back();
        particle.material = fixed ? ParticleMaterial::Fixed : material;
        particle.groupId = group;
        if (fixed)
        {
            particle.body.inverseMass = 0.f;
        }
        return bodies.size() - 1;
    };
    const auto connect = [&](std::size_t first, std::size_t second)
    {
        const sf::Vector2f difference =
            bodies[second].body.center() - bodies[first].body.center();
        springs.push_back({
            first,
            second,
            std::sqrt(difference.x * difference.x +
                difference.y * difference.y),
            6200.f,
            250.f
        });
    };

    // The hill is a shallow anchored mesh, not a solid platform. Its
    // changing row spacing continues the project's wireframe language.
    constexpr std::size_t HILL_COLUMNS = 65;
    constexpr std::size_t HILL_ROWS = 6;
    std::array<std::array<std::size_t, HILL_COLUMNS>, HILL_ROWS> hill{};
    for (std::size_t row = 0; row < HILL_ROWS; ++row)
    {
        for (std::size_t column = 0; column < HILL_COLUMNS; ++column)
        {
            const float parameter = static_cast<float>(column) /
                static_cast<float>(HILL_COLUMNS - 1);
            const float normalized = 0.5f + 0.5f * std::sin(
                (parameter - 0.5f) * 3.14159265359f
            );
            const float x = 48.f + normalized * 704.f;
            const float surface = 535.f -
                34.f * std::sin(normalized * 3.14159265359f);
            const float depth = std::pow(
                static_cast<float>(row), 1.35f
            ) * 4.2f;
            hill[row][column] = addParticle(
                sf::Vector2f(x, surface + depth),
                ParticleMaterial::Structural,
                10,
                row == HILL_ROWS - 1
            );
            if (column > 0)
            {
                connect(hill[row][column - 1], hill[row][column]);
            }
            if (row > 0)
            {
                connect(hill[row - 1][column], hill[row][column]);
                if (column > 0)
                {
                    connect(hill[row - 1][column - 1], hill[row][column]);
                }
                if (column + 1 < HILL_COLUMNS)
                {
                    connect(hill[row - 1][column + 1], hill[row][column]);
                }
            }
        }
    }

    // Five interconnected strands give the leaning trunk visible width
    // and let it bend without behaving like a single piece of string.
    constexpr std::size_t TRUNK_ROWS = 37;
    constexpr std::size_t TRUNK_COLUMNS = 5;
    std::array<std::array<std::size_t, TRUNK_COLUMNS>, TRUNK_ROWS> trunk{};
    for (std::size_t row = 0; row < TRUNK_ROWS; ++row)
    {
        const float rawT = static_cast<float>(row) /
            static_cast<float>(TRUNK_ROWS - 1);
        const float t = std::sin(rawT * 3.14159265359f * 0.5f);
        const sf::Vector2f center(
            355.f + 44.f * t + 20.f * t * t +
                7.f * std::sin(t * 5.4f) + 3.f * std::sin(t * 13.f),
            505.f - 245.f * t + 3.5f * std::sin(t * 8.3f)
        );
        const float halfWidth = (11.5f - 7.7f * t) *
            (1.f + 0.09f * std::sin(t * 17.f));
        for (std::size_t column = 0; column < TRUNK_COLUMNS; ++column)
        {
            const float acrossParameter = -1.f + 2.f *
                static_cast<float>(column) /
                static_cast<float>(TRUNK_COLUMNS - 1);
            const float across = std::sin(
                acrossParameter * 3.14159265359f * 0.5f
            );
            trunk[row][column] = addParticle(
                center + sf::Vector2f(across * halfWidth, across * 2.f),
                ParticleMaterial::Structural,
                20,
                row == 0
            );
            if (row > 0 && row % 9 == 0)
            {
                bodies[trunk[row][column]].body.inverseMass = 0.f;
                bodies[trunk[row][column]].showFixedOutline = false;
            }
            if (column > 0)
            {
                connect(trunk[row][column - 1], trunk[row][column]);
            }
            if (row > 0)
            {
                connect(trunk[row - 1][column], trunk[row][column]);
                if (column > 0)
                {
                    connect(trunk[row - 1][column - 1], trunk[row][column]);
                }
                if (column + 1 < TRUNK_COLUMNS)
                {
                    connect(trunk[row - 1][column + 1], trunk[row][column]);
                }
            }
        }
    }
    for (std::size_t column = 0; column < TRUNK_COLUMNS; ++column)
    {
        connect(hill[0][28 + column], trunk[0][column]);
    }

    const auto growBranch = [&](std::size_t start,
                                sf::Vector2f step,
                                std::size_t count,
                                int group)
    {
        std::array<std::size_t, 3> previous{start, start, start};
        sf::Vector2f position = bodies[start].body.center();
        std::vector<std::size_t> centerline;
        centerline.reserve(count);
        const float stepLength = std::sqrt(
            step.x * step.x + step.y * step.y
        );
        const sf::Vector2f normal(
            -step.y / stepLength, step.x / stepLength
        );
        for (std::size_t index = 0; index < count; ++index)
        {
            const float bend = static_cast<float>(index) /
                static_cast<float>(std::max<std::size_t>(1, count - 1));
            const float stepScale = 0.72f + 0.56f *
                std::sin((bend + 0.05f) * 3.14159265359f * 0.8f);
            const float irregularCurve =
                2.8f * std::sin(static_cast<float>(index) * 0.82f +
                    static_cast<float>(group) * 0.63f) +
                (group % 2 == 0 ? 4.5f : -3.5f) * bend * bend;
            position += step * stepScale + normal * irregularCurve;
            const float width = 4.5f * (1.f - 0.72f * bend);
            std::array<std::size_t, 3> current{};
            for (std::size_t strand = 0; strand < 3; ++strand)
            {
                const float across = static_cast<float>(strand) - 1.f;
                current[strand] = addParticle(
                    position + normal * across * width,
                    ParticleMaterial::Structural,
                    group
                );
                if (index % 4 == 0)
                {
                    bodies[current[strand]].body.inverseMass = 0.f;
                    bodies[current[strand]].showFixedOutline = false;
                }
                connect(previous[strand], current[strand]);
                if (strand > 0)
                {
                    connect(current[strand - 1], current[strand]);
                    connect(previous[strand - 1], current[strand]);
                }
            }
            previous = current;
            centerline.push_back(current[1]);
        }
        return centerline;
    };

    const std::array branches{
        growBranch(trunk[17][2], sf::Vector2f(-8.5f, -6.f), 14, 21),
        growBranch(trunk[22][2], sf::Vector2f(9.f, -5.5f), 17, 22),
        growBranch(trunk[27][2], sf::Vector2f(-6.5f, -8.f), 12, 23),
        growBranch(trunk[32][2], sf::Vector2f(9.5f, -6.f), 14, 24),
        growBranch(trunk[36][2], sf::Vector2f(7.f, -8.f), 11, 25)
    };

    struct CanopyPlacement
    {
        std::size_t branch;
        float along;
        sf::Vector2f offset;
        float halfWidth;
        float halfHeight;
    };
    const std::array canopy{
        CanopyPlacement{0, 0.38f, {-20.f, 8.f}, 45.f, 28.f},
        CanopyPlacement{0, 0.68f, {-22.f, -5.f}, 54.f, 32.f},
        CanopyPlacement{0, 1.00f, {-28.f, 6.f}, 63.f, 38.f},
        CanopyPlacement{1, 0.24f, {16.f, -10.f}, 43.f, 27.f},
        CanopyPlacement{1, 0.48f, {24.f, 8.f}, 58.f, 33.f},
        CanopyPlacement{1, 0.74f, {24.f, -12.f}, 64.f, 38.f},
        CanopyPlacement{1, 1.00f, {30.f, 4.f}, 76.f, 42.f},
        CanopyPlacement{2, 0.42f, {-16.f, -16.f}, 43.f, 29.f},
        CanopyPlacement{2, 0.72f, {-24.f, -12.f}, 54.f, 34.f},
        CanopyPlacement{2, 1.00f, {-22.f, -22.f}, 59.f, 36.f},
        CanopyPlacement{3, 0.36f, {18.f, 8.f}, 45.f, 27.f},
        CanopyPlacement{3, 0.68f, {24.f, -9.f}, 61.f, 36.f},
        CanopyPlacement{3, 1.00f, {34.f, 8.f}, 80.f, 43.f},
        CanopyPlacement{4, 0.38f, {10.f, -18.f}, 40.f, 26.f},
        CanopyPlacement{4, 0.70f, {20.f, -22.f}, 55.f, 34.f},
        CanopyPlacement{4, 1.00f, {28.f, -25.f}, 70.f, 40.f}
    };
    for (std::size_t cluster = 0; cluster < canopy.size(); ++cluster)
    {
        constexpr std::size_t LEAF_COLUMNS = 13;
        constexpr std::size_t LEAF_ROWS = 9;
        std::array<std::array<std::optional<std::size_t>, LEAF_COLUMNS>,
            LEAF_ROWS> leaves{};
        const auto& placement = canopy[cluster];
        const auto& branch = branches[placement.branch];
        const std::size_t branchPosition = std::min(
            branch.size() - 1,
            static_cast<std::size_t>(std::round(
                placement.along * static_cast<float>(branch.size() - 1)
            ))
        );
        const std::size_t attachmentNode = branch[branchPosition];
        const sf::Vector2f clusterCenter =
            bodies[attachmentNode].body.center() + placement.offset;
        for (std::size_t row = 0; row < LEAF_ROWS; ++row)
        {
            for (std::size_t column = 0; column < LEAF_COLUMNS; ++column)
            {
                const float u = -1.f + 2.f * static_cast<float>(column) /
                    static_cast<float>(LEAF_COLUMNS - 1);
                const float v = -1.f + 2.f * static_cast<float>(row) /
                    static_cast<float>(LEAF_ROWS - 1);
                const float mask = u * u + v * v;
                const float gapA = (u + 0.3f) * (u + 0.3f) +
                    (v - 0.05f) * (v - 0.05f);
                const float gapB = (u - 0.34f) * (u - 0.34f) +
                    (v + 0.28f) * (v + 0.28f);
                const bool windGap = u < -0.05f && u > -0.42f &&
                    v < -0.32f && v > -0.68f;
                if (mask > 1.f || gapA < 0.065f || gapB < 0.05f ||
                    windGap ||
                    ((row * 3 + column * 5 + cluster) % 23 == 0))
                {
                    continue;
                }
                const float projectedU = std::sin(
                    u * 3.14159265359f * 0.5f
                );
                const float projectedV = std::sin(
                    v * 3.14159265359f * 0.5f
                );
                const float depth = std::sqrt(std::max(0.f, 1.f - mask));
                leaves[row][column] = addParticle(
                    clusterCenter + sf::Vector2f(
                        projectedU *
                            (placement.halfWidth + 7.f * v) +
                            12.f * depth,
                        projectedV * placement.halfHeight -
                            8.f * depth
                    ),
                    ParticleMaterial::Fragile,
                    30 + static_cast<int>(cluster)
                );
                if (column > 0 && leaves[row][column - 1])
                {
                    connect(*leaves[row][column - 1], *leaves[row][column]);
                }
                if (row > 0 && leaves[row - 1][column])
                {
                    connect(*leaves[row - 1][column], *leaves[row][column]);
                }
                if ((row + column + cluster) % 2 == 0 &&
                    row > 0 && column > 0 &&
                    leaves[row - 1][column - 1])
                {
                    connect(*leaves[row - 1][column - 1],
                        *leaves[row][column]);
                }
                if ((row + column + cluster) % 2 != 0 &&
                    row > 0 && column + 1 < LEAF_COLUMNS &&
                    leaves[row - 1][column + 1])
                {
                    connect(*leaves[row - 1][column + 1],
                        *leaves[row][column]);
                }
            }
        }
        std::optional<std::size_t> attachment;
        float nearestDistanceSquared =
            std::numeric_limits<float>::max();
        for (const auto& row : leaves)
        {
            for (const auto leaf : row)
            {
                if (leaf)
                {
                    const sf::Vector2f difference =
                        bodies[*leaf].body.center() -
                        bodies[attachmentNode].body.center();
                    const float distanceSquared =
                        difference.x * difference.x +
                        difference.y * difference.y;
                    if (distanceSquared < nearestDistanceSquared)
                    {
                        nearestDistanceSquared = distanceSquared;
                        attachment = leaf;
                    }
                }
            }
        }
        if (attachment)
        {
            connect(attachmentNode, *attachment);
        }
    }

}
}
