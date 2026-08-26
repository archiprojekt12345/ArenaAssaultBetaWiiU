#pragma once

#include <algorithm>

#include "math.hpp"

namespace aa {

struct MeshBounds {
    Vec3 center{};
    float radius{};
    bool valid{};
};

class BoundsAccumulator {
public:
    void add(const Vec3& p) {
        if (!hasPoint_) {
            min_ = max_ = p;
            hasPoint_ = true;
            return;
        }
        min_.x = std::min(min_.x, p.x);
        min_.y = std::min(min_.y, p.y);
        min_.z = std::min(min_.z, p.z);
        max_.x = std::max(max_.x, p.x);
        max_.y = std::max(max_.y, p.y);
        max_.z = std::max(max_.z, p.z);
    }

    MeshBounds finish() const {
        if (!hasPoint_) return {};
        const Vec3 center{
            (min_.x + max_.x) * 0.5f,
            (min_.y + max_.y) * 0.5f,
            (min_.z + max_.z) * 0.5f
        };
        const Vec3 half{
            (max_.x - min_.x) * 0.5f,
            (max_.y - min_.y) * 0.5f,
            (max_.z - min_.z) * 0.5f
        };
        return {center, length(half), true};
    }

private:
    Vec3 min_{};
    Vec3 max_{};
    bool hasPoint_{};
};

} // namespace aa
