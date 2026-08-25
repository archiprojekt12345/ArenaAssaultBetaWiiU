#include "map.hpp"

#include <algorithm>
#include <cmath>

namespace aa {

bool ArenaMap::circleHitsBox(const Vec3& p, float radius, const BoxObstacle& b) const {
    const float minX = b.center.x - b.half.x;
    const float maxX = b.center.x + b.half.x;
    const float minZ = b.center.z - b.half.z;
    const float maxZ = b.center.z + b.half.z;
    const float cx = clampf(p.x, minX, maxX);
    const float cz = clampf(p.z, minZ, maxZ);
    const float dx = p.x - cx;
    const float dz = p.z - cz;
    return dx*dx + dz*dz < radius*radius;
}

bool ArenaMap::validPosition(const Vec3& p, float radius) const {
    if (p.x < -ARENA_HALF + radius || p.x > ARENA_HALF - radius ||
        p.z < -ARENA_HALF + radius || p.z > ARENA_HALF - radius) return false;

    for (std::size_t i = 4; i < obstacles_.size(); ++i) {
        if (circleHitsBox(p, radius, obstacles_[i])) return false;
    }
    return true;
}

float ArenaMap::rayAabbDistance(const Vec3& origin, const Vec3& dir,
                                const BoxObstacle& b) const {
    const Vec3 mn{b.center.x - b.half.x, 0.0f, b.center.z - b.half.z};
    const Vec3 mx{b.center.x + b.half.x, b.height, b.center.z + b.half.z};

    float tmin = 0.0f;
    float tmax = 1000.0f;

    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float lo[3] = {mn.x, mn.y, mn.z};
    const float hi[3] = {mx.x, mx.y, mx.z};

    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 0.00001f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return -1.0f;
        } else {
            const float inv = 1.0f / d[i];
            float t1 = (lo[i] - o[i]) * inv;
            float t2 = (hi[i] - o[i]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return -1.0f;
        }
    }
    return tmin >= 0.0f ? tmin : tmax;
}

float ArenaMap::nearestObstacleDistance(const Vec3& origin, const Vec3& dir) const {
    float best = 1000.0f;
    for (const auto& b : obstacles_) {
        const float t = rayAabbDistance(origin, dir, b);
        if (t >= 0.0f && t < best) best = t;
    }
    return best;
}

bool ArenaMap::hasLineOfSight(const Vec3& a, const Vec3& b) const {
    const Vec3 delta = b - a;
    const float dist = length(delta);
    if (dist < 0.001f) return true;
    const Vec3 dir = delta / dist;
    return nearestObstacleDistance(a, dir) > dist - 0.15f;
}

float raySphereDistance(const Vec3& origin, const Vec3& dir,
                        const Vec3& center, float radius) {
    const Vec3 oc = origin - center;
    const float b = dot(oc, dir);
    const float c = dot(oc, oc) - radius*radius;
    float h = b*b - c;
    if (h < 0.0f) return -1.0f;
    h = std::sqrt(h);
    float t = -b - h;
    if (t < 0.0f) t = -b + h;
    return t >= 0.0f ? t : -1.0f;
}

} // namespace aa
