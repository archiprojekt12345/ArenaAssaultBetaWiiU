#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "animation.hpp"
#include "mesh.hpp"
#include "skinning.hpp"

using namespace aa;

namespace {
bool near(float a, float b) { return std::fabs(a-b) < 0.0002f; }
}

int main() {
    SkeletonPose pose{};
    pose.count = 1;
    pose.bones[0].pivot = {0.5f, 0.0f, 0.0f};
    pose.bones[0].rotation = {0.0f, 0.0f, PI*0.5f};
    pose.bones[0].translation = {1.0f, 2.0f, 0.0f};

    const auto affines = buildBoneAffines(pose);
    const Vec3 probe{1.5f, 0.0f, 0.0f};
    const Vec3 expectedPoint = applyBonePoint(pose.bones[0], probe);
    const Vec3 affinePoint = applyBonePoint(affines[0], probe);
    assert(near(expectedPoint.x, affinePoint.x));
    assert(near(expectedPoint.y, affinePoint.y));
    assert(near(expectedPoint.z, affinePoint.z));

    std::vector<SkinnedMeshVertex> source(4);
    source[0].position = {0,0,0};
    source[1].position = {1,0,0};
    source[2].position = {1,1,0};
    source[3].position = {0,1,0};
    for (auto& v : source) {
        v.normal = {0,0,1};
        v.uv = {0,0};
        v.bone[0] = 0;
        v.weight[0] = 1.0f;
    }

    std::vector<SkinnedVertexOutput> out;
    const SkinningStats stats = skinUniqueVertices(source, pose, out);
    assert(out.size() == source.size());
    assert(stats.evaluatedVertices == source.size());
    assert(stats.weightedTransforms == source.size());

    const Vec3 expected0 = applyBonePoint(pose.bones[0], source[0].position);
    assert(near(out[0].position.x, expected0.x));
    assert(near(out[0].position.y, expected0.y));
    assert(near(out[0].position.z, expected0.z));
    assert(near(length(out[0].normal), 1.0f));

    // Six triangle-list indices may reference these four vertices, but the skin
    // stage itself must still evaluate only the four unique source vertices.
    const std::size_t indexCount = 6;
    assert(stats.evaluatedVertices < indexCount);

    std::cout << "skinning_tests: PASS\n";
    return 0;
}
