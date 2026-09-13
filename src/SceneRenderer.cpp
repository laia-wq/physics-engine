#include "app/SceneRenderer.hpp"
#include "physics/Collision.hpp"

#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
void drawLine(sf::RenderTarget& target, sf::Vector2f start,
              sf::Vector2f end, sf::Color color)
{
    const std::array vertices{sf::Vertex{start, color}, sf::Vertex{end, color}};
    target.draw(vertices.data(), vertices.size(), sf::PrimitiveType::Lines);
}

void drawBuildingOccluder(sf::RenderTarget& target,
                          const std::vector<app::CircleView>& bodies, int group)
{
    std::vector<sf::Vector2f> points;
    for (const auto& view : bodies)
    {
        if (view.groupId == group) points.push_back(view.body.center());
    }
    if (points.size() < 3) return;

    // Trace the actual outline so terrain and distant buildings are hidden
    // without placing an oversized rectangular blank area around the mesh.
    std::sort(points.begin(), points.end(),
        [](const sf::Vector2f& first, const sf::Vector2f& second)
        {
            return first.x < second.x ||
                (first.x == second.x && first.y < second.y);
        });
    const auto cross = [](const sf::Vector2f& origin,
                          const sf::Vector2f& first,
                          const sf::Vector2f& second)
    {
        const sf::Vector2f a = first - origin;
        const sf::Vector2f b = second - origin;
        return a.x * b.y - a.y * b.x;
    };
    std::vector<sf::Vector2f> hull;
    for (const sf::Vector2f point : points)
    {
        while (hull.size() >= 2 &&
               cross(hull[hull.size() - 2], hull.back(), point) <= 0.f)
            hull.pop_back();
        hull.push_back(point);
    }
    const std::size_t lowerSize = hull.size();
    for (auto iterator = points.rbegin() + 1; iterator != points.rend(); ++iterator)
    {
        while (hull.size() > lowerSize &&
               cross(hull[hull.size() - 2], hull.back(), *iterator) <= 0.f)
            hull.pop_back();
        hull.push_back(*iterator);
    }
    if (hull.size() > 1) hull.pop_back();

    sf::ConvexShape silhouette(hull.size());
    for (std::size_t index = 0; index < hull.size(); ++index)
        silhouette.setPoint(index, hull[index]);
    silhouette.setFillColor(sf::Color::Black);
    target.draw(silhouette);
}
}

namespace app
{
bool isBuildingBody(const std::vector<CircleView>& bodies, std::size_t index)
{
    return index < bodies.size() &&
        bodies[index].groupId >= 60 && bodies[index].groupId <= 71;
}

void updateParticleAppearance(std::vector<CircleView>& bodies,
    bool colorByMaterial, bool colorByCharge, bool highlightCollisions,
    std::optional<std::size_t> selectedBody)
{
    for (auto& view : bodies)
    {
        const sf::Color chargeColor = view.body.charge > 0.1f
            ? sf::Color(255, 110, 80)
            : (view.body.charge < -0.1f ? sf::Color(70, 170, 255)
                                       : sf::Color::White);
        view.shape.setFillColor(colorByMaterial ? materialColor(view.material)
            : (colorByCharge ? chargeColor : sf::Color::White));
        view.shape.setOutlineColor(sf::Color(255, 215, 70));
        view.shape.setOutlineThickness(
            view.body.inverseMass == 0.f && view.showFixedOutline ? 3.f : 0.f);
        if (view.groupId == 58)
        {
            view.shape.setFillColor(sf::Color::Black);
            view.shape.setOutlineColor(sf::Color(110, 165, 255));
            view.shape.setOutlineThickness(3.f);
        }
        view.sync();
    }

    if (highlightCollisions)
    {
        for (std::size_t first = 0; first < bodies.size(); ++first)
            for (std::size_t second = first + 1; second < bodies.size(); ++second)
                if (physics::circlesOverlap(bodies[first].body.center(),
                    bodies[first].body.radius, bodies[second].body.center(),
                    bodies[second].body.radius))
                {
                    bodies[first].shape.setFillColor(sf::Color::Red);
                    bodies[second].shape.setFillColor(sf::Color::Red);
                }
    }
    if (selectedBody && *selectedBody < bodies.size())
        bodies[*selectedBody].shape.setFillColor(sf::Color(255, 215, 0));
}

void drawSpringConnections(sf::RenderTarget& target,
    const std::vector<CircleView>& bodies,
    const std::vector<physics::Spring>& springs, unsigned char alpha,
    int buildingLayer, int buildingGroup)
{
    for (const auto& spring : springs)
    {
        if (spring.first >= bodies.size() || spring.second >= bodies.size())
            continue;
        const bool buildingConnection = isBuildingBody(bodies, spring.first) ||
            isBuildingBody(bodies, spring.second);
        if ((buildingLayer < 0 && buildingConnection) ||
            (buildingLayer > 0 && !buildingConnection)) continue;
        if (buildingLayer > 0 && buildingGroup >= 60)
        {
            const int connectionGroup = isBuildingBody(bodies, spring.first)
                ? bodies[spring.first].groupId : bodies[spring.second].groupId;
            if (connectionGroup != buildingGroup) continue;
        }
        const sf::Vector2f start = bodies[spring.first].body.center();
        const sf::Vector2f end = bodies[spring.second].body.center();
        const sf::Vector2f difference = end - start;
        const float length = std::sqrt(difference.x * difference.x +
                                       difference.y * difference.y);
        const float extension = length - spring.restLength;
        sf::Color color = extension > 2.f ? sf::Color(255, 120, 100)
            : (extension < -2.f ? sf::Color(90, 180, 255)
                                : sf::Color(210, 220, 235));
        color.a = alpha;
        drawLine(target, start, end, color);
    }
}

void drawNonBuildingBodies(sf::RenderTarget& target,
                           const std::vector<CircleView>& bodies)
{
    for (std::size_t index = 0; index < bodies.size(); ++index)
        if (!isBuildingBody(bodies, index)) target.draw(bodies[index].shape);
}

void drawBuildingsBackToFront(sf::RenderTarget& target,
    const std::vector<CircleView>& bodies,
    const std::vector<physics::Spring>& springs)
{
    std::vector<std::pair<float, int>> buildingOrder;
    for (int group = 60; group <= 71; ++group)
    {
        float groundDepth = -std::numeric_limits<float>::infinity();
        bool found = false;
        for (const auto& view : bodies)
            if (view.groupId == group)
            {
                groundDepth = std::max(groundDepth, view.body.center().y);
                found = true;
            }
        if (found) buildingOrder.emplace_back(groundDepth, group);
    }
    std::sort(buildingOrder.begin(), buildingOrder.end());
    for (const auto [depth, group] : buildingOrder)
    {
        static_cast<void>(depth);
        drawBuildingOccluder(target, bodies, group);
        drawSpringConnections(target, bodies, springs, 255, 1, group);
        for (const auto& view : bodies)
            if (view.groupId == group) target.draw(view.shape);
    }
}
}
