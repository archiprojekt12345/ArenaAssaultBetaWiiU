#include <cassert>
#include <cmath>
#include <iostream>

#include "mesh_bounds.hpp"
#include "render_policy.hpp"

using namespace aa;

int main() {
    Camera camera;
    camera.pos = {0,0,0};
    camera.yaw = 0.0f;
    camera.pitch = 0.0f;
    camera.fov = 72.0f * PI / 180.0f;
    camera.aspect = 16.0f / 9.0f;
    camera.nearZ = 0.08f;
    camera.farZ = 80.0f;

    assert(sphereVisible(camera, {0,0,-10}, 1.0f));
    assert(!sphereVisible(camera, {0,0,10}, 1.0f));
    assert(!sphereVisible(camera, {100,0,-10}, 1.0f));
    assert(!sphereVisible(camera, {0,0,-100}, 1.0f));
    assert(sphereVisible(camera, {0,0,-79.5f}, 1.0f));

    assert(selectLod(4.0f, 8.0f, 18.0f) == MeshLod::High);
    assert(selectLod(10.0f, 8.0f, 18.0f) == MeshLod::Medium);
    assert(selectLod(24.0f, 8.0f, 18.0f) == MeshLod::Low);
    assert(selectLod(10.0f, 18.0f, 8.0f) == MeshLod::Medium);

    assert(selectEnemyRenderTier(4.0f, false) == EnemyRenderTier::Culled);
    assert(selectEnemyRenderTier(6.9f, true) == EnemyRenderTier::High);
    assert(selectEnemyRenderTier(7.0f, true) == EnemyRenderTier::Medium);
    assert(selectEnemyRenderTier(13.9f, true) == EnemyRenderTier::Medium);
    assert(selectEnemyRenderTier(14.0f, true) == EnemyRenderTier::Low);
    assert(selectEnemyRenderTier(28.0f, true) == EnemyRenderTier::Low);

    assert(shouldRefreshMediumPose(0, 0));
    assert(!shouldRefreshMediumPose(1, 0));
    assert(!shouldRefreshMediumPose(2, 0));
    assert(shouldRefreshMediumPose(3, 0));
    assert(!shouldRefreshMediumPose(0, 1));
    assert(!shouldRefreshMediumPose(1, 1));
    assert(shouldRefreshMediumPose(2, 1));

    assert(!batchWouldOverflow(100, 3, 128));
    assert(batchWouldOverflow(126, 3, 128));
    assert(batchWouldOverflow(128, 1, 128));

    BoundsAccumulator bounds;
    bounds.add({-1.0f,-2.0f,-3.0f});
    bounds.add({3.0f,2.0f,1.0f});
    const MeshBounds result = bounds.finish();
    assert(result.valid);
    assert(std::fabs(result.center.x - 1.0f) < 0.0001f);
    assert(std::fabs(result.center.y) < 0.0001f);
    assert(std::fabs(result.center.z + 1.0f) < 0.0001f);
    assert(std::fabs(result.radius - std::sqrt(12.0f)) < 0.0001f);

    std::cout << "render_policy_tests: PASS\n";
    return 0;
}
