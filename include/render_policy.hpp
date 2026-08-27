#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "camera.hpp"

namespace aa {

enum class MeshLod {
    High,
    Medium,
    Low
};

inline MeshLod selectLod(float distance, float highToMedium, float mediumToLow) {
    if (highToMedium > mediumToLow) std::swap(highToMedium, mediumToLow);
    if (distance < highToMedium) return MeshLod::High;
    if (distance < mediumToLow) return MeshLod::Medium;
    return MeshLod::Low;
}

enum class EnemyRenderTier {
    Culled,
    High,
    Medium,
    Low
};

// Hardware-tuned for Wii U. Keep full-rate AAM2 only in close combat,
// amortize medium-range animation over three frames, and switch to AAM1
// before the detailed mesh becomes a sustained frame-time cost.
inline EnemyRenderTier selectEnemyRenderTier(float distance, bool visible,
                                             float highToMedium = 7.0f,
                                             float mediumToLow = 14.0f) {
    if (!visible) return EnemyRenderTier::Culled;
    switch (selectLod(distance, highToMedium, mediumToLow)) {
        case MeshLod::High: return EnemyRenderTier::High;
        case MeshLod::Medium: return EnemyRenderTier::Medium;
        case MeshLod::Low: return EnemyRenderTier::Low;
    }
    return EnemyRenderTier::Low;
}

inline bool shouldRefreshMediumPose(std::uint64_t frameIndex,
                                    std::size_t actorIndex) {
    return ((frameIndex + static_cast<std::uint64_t>(actorIndex)) % 3u) == 0u;
}

inline bool batchWouldOverflow(std::size_t current, std::size_t incoming,
                               std::size_t capacity) {
    if (incoming > capacity) return true;
    return current > capacity - incoming;
}

inline bool sphereVisible(const Camera& camera, const Vec3& center, float radius) {
    radius = std::max(0.0f, radius);
    const Vec3 toCenter = center - camera.pos;
    const float distance = length(toCenter);

    if (distance - radius > camera.farZ) return false;
    if (distance + radius < camera.nearZ) return false;
    if (distance <= radius || distance < 0.00001f) return true;

    const Vec3 forward = cameraForward(camera);
    const Vec3 right = cameraRight(camera);
    const Vec3 up = cameraUp(camera);

    const float forwardDistance = dot(toCenter, forward);
    if (forwardDistance + radius <= 0.0f) return false;

    const float halfVertical = camera.fov * 0.5f;
    const float halfHorizontal = std::atan(std::tan(halfVertical) * camera.aspect);
    const float horizontalLimit = std::tan(halfHorizontal) *
                                  std::max(forwardDistance, 0.0f) + radius;
    const float verticalLimit = std::tan(halfVertical) *
                                std::max(forwardDistance, 0.0f) + radius;

    return std::fabs(dot(toCenter, right)) <= horizontalLimit &&
           std::fabs(dot(toCenter, up)) <= verticalLimit;
}

} // namespace aa
