#pragma once

#include <array>
#include <cstddef>

#include "math.hpp"

namespace aa {

constexpr float ARENA_HALF = 21.0f;

struct BoxObstacle {
    Vec3 center;
    Vec3 half;
    float height;
    Color color;
};

class ArenaMap {
public:
    static constexpr std::size_t kObstacleCount = 18;

    const std::array<BoxObstacle, kObstacleCount>& obstacles() const { return obstacles_; }

    bool circleHitsBox(const Vec3& p, float radius, const BoxObstacle& b) const;
    bool validPosition(const Vec3& p, float radius) const;
    float rayAabbDistance(const Vec3& origin, const Vec3& dir, const BoxObstacle& b) const;
    float nearestObstacleDistance(const Vec3& origin, const Vec3& dir) const;
    bool hasLineOfSight(const Vec3& a, const Vec3& b) const;

private:
    std::array<BoxObstacle, kObstacleCount> obstacles_{{
        {{  0.0f, 0, -ARENA_HALF}, {ARENA_HALF, 0, 0.45f}, 2.6f, {0.18f,0.23f,0.30f,1}},
        {{  0.0f, 0,  ARENA_HALF}, {ARENA_HALF, 0, 0.45f}, 2.6f, {0.18f,0.23f,0.30f,1}},
        {{-ARENA_HALF,0, 0.0f}, {0.45f,0,ARENA_HALF}, 2.6f, {0.18f,0.23f,0.30f,1}},
        {{ ARENA_HALF,0, 0.0f}, {0.45f,0,ARENA_HALF}, 2.6f, {0.18f,0.23f,0.30f,1}},

        {{-8.0f,0,-8.0f}, {2.8f,0,0.6f}, 2.0f, {0.26f,0.31f,0.38f,1}},
        {{ 8.0f,0,-8.0f}, {2.8f,0,0.6f}, 2.0f, {0.26f,0.31f,0.38f,1}},
        {{-8.0f,0, 8.0f}, {2.8f,0,0.6f}, 2.0f, {0.26f,0.31f,0.38f,1}},
        {{ 8.0f,0, 8.0f}, {2.8f,0,0.6f}, 2.0f, {0.26f,0.31f,0.38f,1}},

        {{-10.5f,0, 0.0f}, {0.65f,0,3.0f}, 1.65f, {0.34f,0.29f,0.24f,1}},
        {{ 10.5f,0, 0.0f}, {0.65f,0,3.0f}, 1.65f, {0.34f,0.29f,0.24f,1}},
        {{  0.0f,0,-10.5f}, {3.0f,0,0.65f}, 1.65f, {0.34f,0.29f,0.24f,1}},
        {{  0.0f,0, 10.5f}, {3.0f,0,0.65f}, 1.65f, {0.34f,0.29f,0.24f,1}},

        {{-4.5f,0,-1.5f}, {1.0f,0,1.0f}, 3.4f, {0.20f,0.28f,0.35f,1}},
        {{ 4.5f,0, 1.5f}, {1.0f,0,1.0f}, 3.4f, {0.20f,0.28f,0.35f,1}},
        {{-1.5f,0, 4.5f}, {1.0f,0,1.0f}, 3.4f, {0.20f,0.28f,0.35f,1}},
        {{ 1.5f,0,-4.5f}, {1.0f,0,1.0f}, 3.4f, {0.20f,0.28f,0.35f,1}},

        {{-14.0f,0,-13.5f}, {1.2f,0,1.2f}, 2.8f, {0.21f,0.35f,0.31f,1}},
        {{ 14.0f,0, 13.5f}, {1.2f,0,1.2f}, 2.8f, {0.21f,0.35f,0.31f,1}}
    }};
};

float raySphereDistance(const Vec3& origin, const Vec3& dir,
                        const Vec3& center, float radius);

} // namespace aa
