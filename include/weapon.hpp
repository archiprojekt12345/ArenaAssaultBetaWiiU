#pragma once

namespace aa {

struct Weapon {
    int magazineSize{30};
    int ammo{30};
    float shotsPerSecond{9.52f};
    float reloadDuration{1.05f};
    float fireCooldown{0.0f};
    float reloadTimer{0.0f};
    float recoil{0.0f};

    bool reloading() const { return reloadTimer > 0.0f; }
};

} // namespace aa
