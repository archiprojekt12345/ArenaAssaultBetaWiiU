#pragma once

#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>

#include "math.hpp"

namespace aa {

struct FloatKeyframe {
    float time;
    float value;
};

inline float sampleLoop(const FloatKeyframe* keys, std::size_t count,
                        float duration, float time) {
    if (!keys || count == 0 || duration <= 0.0f) return 0.0f;
    if (count == 1) return keys[0].value;

    float t = std::fmod(time, duration);
    if (t < 0.0f) t += duration;

    std::size_t next = 1;
    while (next < count && keys[next].time < t) ++next;
    if (next >= count) {
        const FloatKeyframe& a = keys[count-1];
        const FloatKeyframe& b = keys[0];
        const float span = (duration - a.time) + b.time;
        const float local = (t >= a.time) ? (t-a.time) : (duration-a.time+t);
        const float f = span > 0.00001f ? local/span : 0.0f;
        return a.value + (b.value-a.value)*f;
    }

    const FloatKeyframe& a = keys[next-1];
    const FloatKeyframe& b = keys[next];
    const float span = b.time-a.time;
    const float f = span > 0.00001f ? (t-a.time)/span : 0.0f;
    return a.value + (b.value-a.value)*f;
}

// AAM2 robot skeleton. Skinning is CPU-side so the existing GX2 shader stays
// compact and the fallback AAM1/box pipeline remains usable.
enum class RobotBone : std::uint32_t {
    Root = 0,
    Torso,
    Head,
    ArmL,
    ArmR,
    LegL,
    LegR,
    Weapon,
    Count
};

constexpr std::size_t kRobotBoneCount = static_cast<std::size_t>(RobotBone::Count);
constexpr std::size_t kMaxSkinBones = 16;

struct BonePose {
    Vec3 pivot{};       // bind-space pivot
    Vec3 rotation{};    // radians, XYZ
    Vec3 translation{}; // bind-space offset
};

struct SkeletonPose {
    std::array<BonePose, kMaxSkinBones> bones{};
    std::size_t count{kRobotBoneCount};
};

inline Vec3 applyBonePoint(const BonePose& bone, const Vec3& p) {
    return bone.pivot + rotateEulerXYZ(p - bone.pivot, bone.rotation) + bone.translation;
}

inline Vec3 applyBoneNormal(const BonePose& bone, const Vec3& n) {
    return normalize(rotateEulerXYZ(n, bone.rotation));
}

// Row-major 3x3 rotation plus translation. Building this once per bone moves
// all sin/cos work out of the weighted-vertex loop.
struct BoneAffine {
    Vec3 row0{1,0,0};
    Vec3 row1{0,1,0};
    Vec3 row2{0,0,1};
    Vec3 translation{};
};

inline BoneAffine makeBoneAffine(const BonePose& bone) {
    const float sx = std::sin(bone.rotation.x);
    const float cx = std::cos(bone.rotation.x);
    const float sy = std::sin(bone.rotation.y);
    const float cy = std::cos(bone.rotation.y);
    const float sz = std::sin(bone.rotation.z);
    const float cz = std::cos(bone.rotation.z);

    BoneAffine a{};
    a.row0 = {
        cz*cy,
        -cz*sy*sx - sz*cx,
        -cz*sy*cx + sz*sx
    };
    a.row1 = {
        sz*cy,
        -sz*sy*sx + cz*cx,
        -sz*sy*cx - cz*sx
    };
    a.row2 = {
        sy,
        cy*sx,
        cy*cx
    };

    const Vec3 rp{
        dot(a.row0,bone.pivot),
        dot(a.row1,bone.pivot),
        dot(a.row2,bone.pivot)
    };
    a.translation = bone.pivot + bone.translation - rp;
    return a;
}

inline std::array<BoneAffine,kMaxSkinBones> buildBoneAffines(const SkeletonPose& pose) {
    std::array<BoneAffine,kMaxSkinBones> out{};
    const std::size_t count = pose.count < kMaxSkinBones ? pose.count : kMaxSkinBones;
    for (std::size_t i=0;i<count;++i) out[i] = makeBoneAffine(pose.bones[i]);
    return out;
}

inline Vec3 applyBonePoint(const BoneAffine& bone, const Vec3& p) {
    return {
        dot(bone.row0,p) + bone.translation.x,
        dot(bone.row1,p) + bone.translation.y,
        dot(bone.row2,p) + bone.translation.z
    };
}

inline Vec3 applyBoneNormal(const BoneAffine& bone, const Vec3& n) {
    return {
        dot(bone.row0,n),
        dot(bone.row1,n),
        dot(bone.row2,n)
    };
}

inline SkeletonPose makeRobotPose(float phase, bool attacking, float hitReaction = 0.0f) {
    SkeletonPose pose{};
    pose.count = kRobotBoneCount;

    auto& root   = pose.bones[static_cast<std::size_t>(RobotBone::Root)];
    auto& torso  = pose.bones[static_cast<std::size_t>(RobotBone::Torso)];
    auto& head   = pose.bones[static_cast<std::size_t>(RobotBone::Head)];
    auto& armL   = pose.bones[static_cast<std::size_t>(RobotBone::ArmL)];
    auto& armR   = pose.bones[static_cast<std::size_t>(RobotBone::ArmR)];
    auto& legL   = pose.bones[static_cast<std::size_t>(RobotBone::LegL)];
    auto& legR   = pose.bones[static_cast<std::size_t>(RobotBone::LegR)];
    auto& weapon = pose.bones[static_cast<std::size_t>(RobotBone::Weapon)];

    root.pivot = {0,0,0};
    torso.pivot = {0,1.25f,0};
    head.pivot = {0,1.79f,0};
    armL.pivot = {-0.52f,1.56f,0};
    armR.pivot = { 0.52f,1.56f,0};
    legL.pivot = {-0.22f,0.73f,0};
    legR.pivot = { 0.22f,0.73f,0};
    weapon.pivot = {0.50f,1.23f,-0.26f};

    const float walk = std::sin(phase);
    const float walk2 = std::sin(phase + PI);
    const float bob = std::fabs(std::sin(phase))*0.025f;

    root.translation.y = bob;
    torso.rotation.z = std::sin(phase*0.5f)*0.025f;
    torso.rotation.x = -hitReaction*0.12f;
    head.rotation.y = std::sin(phase*0.25f)*0.08f;

    legL.rotation.x = walk*0.42f;
    legR.rotation.x = walk2*0.42f;
    armL.rotation.x = walk2*0.28f;
    armR.rotation.x = walk*0.22f;

    if (attacking) {
        armR.rotation.x = -0.72f + walk*0.04f;
        armR.rotation.z = -0.08f;
        armL.rotation.x = -0.48f + walk2*0.03f;
        armL.rotation.z = 0.16f;
        weapon.rotation.x = -0.72f;
        head.rotation.y *= 0.3f;
    } else {
        weapon.rotation.x = armR.rotation.x;
        weapon.rotation.z = armR.rotation.z;
    }

    return pose;
}

} // namespace aa
