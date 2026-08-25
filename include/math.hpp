#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace aa {

constexpr float PI = 3.14159265358979323846f;

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

struct Vec2 {
    float x{}, y{};
};

struct Vec3 {
    float x{}, y{}, z{};

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct Vec4 {
    float x{}, y{}, z{}, w{};
};

struct Color {
    float r{}, g{}, b{}, a{1.0f};
};

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

inline float lengthSq(const Vec3& v) { return dot(v, v); }
inline float length(const Vec3& v) { return std::sqrt(lengthSq(v)); }

inline Vec3 normalize(const Vec3& v) {
    const float l = length(v);
    if (l < 0.00001f) return {0, 0, 0};
    return v / l;
}

inline Vec3 rotateX(const Vec3& p, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {p.x, p.y*c - p.z*s, p.y*s + p.z*c};
}

inline Vec3 rotateY(const Vec3& p, float yaw) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return {p.x*c - p.z*s, p.y, p.x*s + p.z*c};
}

inline Vec3 rotateZ(const Vec3& p, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {p.x*c - p.y*s, p.x*s + p.y*c, p.z};
}

inline Vec3 rotateEulerXYZ(const Vec3& p, const Vec3& r) {
    return rotateZ(rotateY(rotateX(p, r.x), r.y), r.z);
}

inline Color mulColor(const Color& c, float k) {
    return {
        clampf(c.r*k, 0.0f, 1.0f),
        clampf(c.g*k, 0.0f, 1.0f),
        clampf(c.b*k, 0.0f, 1.0f),
        c.a
    };
}

inline Color mixColor(const Color& a, const Color& b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    return {
        a.r + (b.r-a.r)*t,
        a.g + (b.g-a.g)*t,
        a.b + (b.b-a.b)*t,
        a.a + (b.a-a.a)*t
    };
}

// Column-major matrix, matching GLSL mat4 memory order.
struct Mat4 {
    float m[16]{};

    float& at(int row, int col) { return m[col*4 + row]; }
    float at(int row, int col) const { return m[col*4 + row]; }

    static Mat4 identity() {
        Mat4 r{};
        r.at(0,0)=1; r.at(1,1)=1; r.at(2,2)=1; r.at(3,3)=1;
        return r;
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c=0;c<4;++c) {
        for (int row=0;row<4;++row) {
            float v = 0.0f;
            for (int k=0;k<4;++k) v += a.at(row,k) * b.at(k,c);
            r.at(row,c) = v;
        }
    }
    return r;
}

inline Mat4 perspective(float fovY, float aspect, float nearZ, float farZ) {
    Mat4 r{};
    const float f = 1.0f / std::tan(fovY * 0.5f);
    r.at(0,0) = f / aspect;
    r.at(1,1) = f;
    r.at(2,2) = (farZ + nearZ) / (nearZ - farZ);
    r.at(2,3) = (2.0f * farZ * nearZ) / (nearZ - farZ);
    r.at(3,2) = -1.0f;
    return r;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& upHint) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, upHint));
    const Vec3 u = cross(s, f);

    Mat4 out = Mat4::identity();
    out.at(0,0)=s.x; out.at(0,1)=s.y; out.at(0,2)=s.z; out.at(0,3)=-dot(s,eye);
    out.at(1,0)=u.x; out.at(1,1)=u.y; out.at(1,2)=u.z; out.at(1,3)=-dot(u,eye);
    out.at(2,0)=-f.x; out.at(2,1)=-f.y; out.at(2,2)=-f.z; out.at(2,3)=dot(f,eye);
    return out;
}

struct Transform {
    Vec3 position{0,0,0};
    Vec3 scale{1,1,1};
    float yaw{0};
};

inline Vec3 transformPoint(const Transform& t, const Vec3& p) {
    Vec3 scaled{p.x*t.scale.x, p.y*t.scale.y, p.z*t.scale.z};
    return t.position + rotateY(scaled, t.yaw);
}

inline Vec3 transformNormal(const Transform& t, const Vec3& n) {
    Vec3 invScaled{
        (std::fabs(t.scale.x) > 0.00001f) ? n.x/t.scale.x : n.x,
        (std::fabs(t.scale.y) > 0.00001f) ? n.y/t.scale.y : n.y,
        (std::fabs(t.scale.z) > 0.00001f) ? n.z/t.scale.z : n.z
    };
    return normalize(rotateY(invScaled, t.yaw));
}

} // namespace aa
