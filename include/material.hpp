#pragma once

#include "math.hpp"

namespace aa {

struct AtlasRect {
    float u0{0.0f};
    float v0{0.0f};
    float u1{1.0f};
    float v1{1.0f};
};

struct Material {
    Color diffuse{1,1,1,1};
    Color emissive{0,0,0,1};
    float specular{0.25f};
    float roughness{0.65f};
    float textureMix{0.0f};
    float emissiveStrength{0.0f};
    AtlasRect atlas{};
};

namespace materials {
inline Material floor() {
    return {{0.13f,0.15f,0.18f,1},{0,0,0,1},0.18f,0.82f,0.70f,0.0f,{0.0f,0.5f,0.5f,1.0f}};
}
inline Material wall(const Color& c) {
    return {c,{0,0,0,1},0.28f,0.70f,0.60f,0.0f,{0.0f,0.0f,0.5f,0.5f}};
}
inline Material armor(const Color& c) {
    return {c,{0,0,0,1},0.52f,0.38f,0.72f,0.0f,{0.5f,0.0f,1.0f,0.5f}};
}
inline Material darkMetal() {
    return {{0.09f,0.11f,0.13f,1},{0,0,0,1},0.50f,0.30f,0.62f,0.0f,{0.0f,0.0f,0.5f,0.5f}};
}
inline Material emissive(const Color& c, float strength=2.0f) {
    return {{0.09f,0.12f,0.14f,1},c,0.15f,0.55f,0.25f,strength,{0.5f,0.5f,1.0f,1.0f}};
}
} // namespace materials

} // namespace aa
