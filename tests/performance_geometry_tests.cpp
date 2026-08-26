#include <cassert>
#include <iostream>

#include "particle_policy.hpp"
#include "world_asset_layout.hpp"

using namespace aa;

int main() {
    assert(staticWorldGroupCount() == 10u);
    assert(kCorridorPortalPlacements.size() == 2u);
    assert(kSupplyCratePlacements.size() == 8u);

    // V10 used a cube = 12 triangles for every particle. V11 must use a
    // camera-facing quad = exactly two triangles.
    assert(particleBillboardTriangleCount() == 2u);
    assert(particleBillboardTriangleCount() < 12u);

    std::cout << "performance_geometry_tests: PASS\n";
    return 0;
}
