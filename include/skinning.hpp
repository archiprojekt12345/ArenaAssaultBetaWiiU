#pragma once

#include <cstddef>
#include <vector>

#include "animation.hpp"
#include "mesh.hpp"

namespace aa {

struct SkinnedVertexOutput {
    Vec3 position{};
    Vec3 normal{0,1,0};
    Vec2 uv{};
};

struct SkinningStats {
    std::size_t evaluatedVertices{};
    std::size_t weightedTransforms{};
};

inline SkinningStats skinUniqueVertices(const std::vector<SkinnedMeshVertex>& source,
                                        const SkeletonPose& pose,
                                        std::vector<SkinnedVertexOutput>& out) {
    SkinningStats stats{};
    out.resize(source.size());
    if (source.empty() || pose.count == 0) return stats;

    const auto affines = buildBoneAffines(pose);
    const std::size_t boneCount = pose.count < kMaxSkinBones ? pose.count : kMaxSkinBones;

    for (std::size_t i=0;i<source.size();++i) {
        const SkinnedMeshVertex& v = source[i];
        Vec3 p{};
        Vec3 n{};
        float total = 0.0f;

        for (int k=0;k<4;++k) {
            const float w = v.weight[k];
            if (w <= 0.00001f) continue;
            const std::size_t bi = static_cast<std::size_t>(v.bone[k]);
            if (bi >= boneCount) continue;
            p += applyBonePoint(affines[bi],v.position) * w;
            n += applyBoneNormal(affines[bi],v.normal) * w;
            total += w;
            ++stats.weightedTransforms;
        }

        if (total <= 0.00001f) {
            p = v.position;
            n = v.normal;
        } else if (std::fabs(total - 1.0f) > 0.0001f) {
            p = p / total;
            n = n / total;
        }

        out[i] = {p,normalize(n),v.uv};
        ++stats.evaluatedVertices;
    }

    return stats;
}

} // namespace aa
