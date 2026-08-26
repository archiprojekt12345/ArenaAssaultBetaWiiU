#pragma once

#include <array>
#include <cstddef>

#include "math.hpp"

namespace aa {

struct WorldAssetPlacement {
    Vec3 position{};
    Vec3 scale{1.0f,1.0f,1.0f};
    float yaw{};
};

// Two industrial corridor modules frame the terminal and extraction ends of
// the arena. The source corridor is a 3 m repeatable slice centered at Z=0.
inline constexpr std::array<WorldAssetPlacement, 2> kCorridorPortalPlacements{{
    {{0.0f, 0.0f, -17.0f}, {1.0f, 1.0f, 1.0f}, 0.0f},
    {{0.0f, 0.0f,  18.4f}, {1.0f, 1.0f, 1.0f}, PI}
}};

// Small industrial supply crates placed around existing cover. These are
// visual props only for now, so they intentionally sit next to the existing
// collision obstacles rather than creating new collision volumes.
inline constexpr std::array<WorldAssetPlacement, 8> kSupplyCratePlacements{{
    {{-12.8f,0.02f,-10.8f},{1.05f,1.05f,1.05f}, 0.18f},
    {{ 12.9f,0.02f,-10.2f},{1.15f,1.15f,1.15f},-0.24f},
    {{-13.6f,0.02f, 10.4f},{1.10f,1.10f,1.10f},-0.16f},
    {{ 13.2f,0.02f, 10.8f},{1.00f,1.00f,1.00f}, 0.22f},
    {{ -6.0f,0.02f, 13.2f},{0.95f,0.95f,0.95f}, 0.05f},
    {{  6.2f,0.02f,-13.0f},{0.95f,0.95f,0.95f},-0.08f},
    {{-16.4f,0.02f,  1.8f},{1.05f,1.05f,1.05f}, PI*0.5f},
    {{ 16.4f,0.02f, -1.8f},{1.05f,1.05f,1.05f},-PI*0.5f}
}};

inline constexpr std::size_t staticWorldGroupCount() {
    return kCorridorPortalPlacements.size() + kSupplyCratePlacements.size();
}

} // namespace aa
