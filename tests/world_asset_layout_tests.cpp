#include <cassert>
#include <cmath>

#include "world_asset_layout.hpp"

int main() {
    using namespace aa;

    static_assert(kCorridorPortalPlacements.size() == 2,
                  "terminal and extraction need corridor framing");
    static_assert(kSupplyCratePlacements.size() >= 6,
                  "industrial arena needs several crate props");

    assert(std::fabs(kCorridorPortalPlacements[0].position.z + 17.0f) < 0.01f);
    assert(std::fabs(kCorridorPortalPlacements[1].position.z - 18.4f) < 0.01f);

    for (const auto& p : kCorridorPortalPlacements) {
        assert(p.scale.x > 0.0f && p.scale.y > 0.0f && p.scale.z > 0.0f);
    }

    for (const auto& p : kSupplyCratePlacements) {
        assert(std::fabs(p.position.x) < 20.0f);
        assert(std::fabs(p.position.z) < 20.0f);
        assert(p.scale.x > 0.0f && p.scale.y > 0.0f && p.scale.z > 0.0f);
    }

    return 0;
}
