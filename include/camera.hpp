#pragma once

#include "math.hpp"

namespace aa {

struct Camera {
    Vec3 pos{0, 1.65f, 6};
    float yaw{0};
    float pitch{0};
    float fov{72.0f * PI / 180.0f};
    float aspect{16.0f/9.0f};
    float nearZ{0.08f};
    float farZ{80.0f};
};

inline Vec3 cameraForward(const Camera& c) {
    const float cp = std::cos(c.pitch);
    return {
        std::sin(c.yaw) * cp,
        std::sin(c.pitch),
        -std::cos(c.yaw) * cp
    };
}

inline Vec3 cameraRight(const Camera& c) {
    return {std::cos(c.yaw), 0.0f, std::sin(c.yaw)};
}

inline Vec3 cameraUp(const Camera& c) {
    return normalize(cross(cameraRight(c), cameraForward(c)));
}

inline Mat4 cameraView(const Camera& c) {
    return lookAt(c.pos, c.pos + cameraForward(c), {0,1,0});
}

inline Mat4 cameraProjection(const Camera& c) {
    return perspective(c.fov, c.aspect, c.nearZ, c.farZ);
}

inline Mat4 cameraViewProjection(const Camera& c) {
    return cameraProjection(c) * cameraView(c);
}

} // namespace aa
