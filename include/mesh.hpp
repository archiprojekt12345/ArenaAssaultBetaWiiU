#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "math.hpp"
#include "mesh_bounds.hpp"

namespace aa {

// Arena Assault Mesh v1 (.aam)
// Header: magic AAM1, version, vertexCount, indexCount.
// Vertices: pos.xyz + normal.xyz + uv.xy, all IEEE754 float32.
// Indices: uint32 triangle list.
struct MeshVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

static_assert(sizeof(MeshVertex) == 32, "AAM vertex layout must remain 32 bytes");

class Mesh {
public:
    bool loadFromFile(const char* path);
    bool loadFromMemory(const void* data, std::size_t size);
    void clear();

    bool valid() const { return !vertices_.empty() && indices_.size() >= 3; }
    const std::vector<MeshVertex>& vertices() const { return vertices_; }
    const std::vector<std::uint32_t>& indices() const { return indices_; }
    const std::string& sourcePath() const { return sourcePath_; }
    const MeshBounds& bounds() const { return bounds_; }

private:
    std::vector<MeshVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::string sourcePath_;
    MeshBounds bounds_{};
};

// Arena Assault Mesh v2 (.aam2)
// Header: "AAM2", version=2, vertexCount, indexCount, boneCount.
// Per vertex: AAM1 payload + 4 x u32 bone indices + 4 x f32 weights.
// The skeleton definition is game-side; bone IDs are documented in AAM_FORMAT.md.
struct SkinnedMeshVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    std::uint32_t bone[4]{};
    float weight[4]{};
};

class SkinnedMesh {
public:
    bool loadFromFile(const char* path);
    bool loadFromMemory(const void* data, std::size_t size);
    void clear();

    bool valid() const { return !vertices_.empty() && indices_.size() >= 3 && boneCount_ > 0; }
    std::uint32_t boneCount() const { return boneCount_; }
    const std::vector<SkinnedMeshVertex>& vertices() const { return vertices_; }
    const std::vector<std::uint32_t>& indices() const { return indices_; }
    const std::string& sourcePath() const { return sourcePath_; }
    const MeshBounds& bounds() const { return bounds_; }

private:
    std::vector<SkinnedMeshVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::uint32_t boneCount_{};
    std::string sourcePath_;
    MeshBounds bounds_{};
};

} // namespace aa
