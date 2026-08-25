#pragma once

#include "math.hpp"
#include "weapon.hpp"

namespace aa {

struct Player {
    Vec3 pos{0,0,14};
    float yaw{0};
    float pitch{0};
    float hp{100};
    float damageFlash{0};
    bool aiming{false};
    Weapon weapon{};
};

} // namespace aa
