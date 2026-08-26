#include "mesh.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace aa {
namespace {

constexpr std::uint32_t kMaxVertices = 500000;
constexpr std::uint32_t kMaxIndices = 1500000;
constexpr std::uint32_t kMaxBones = 16;

std::uint32_t readLE32(const unsigned char* p) {
    return std::uint32_t(p[0]) |
           (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

float readLEFloat(const unsigned char* p) {
    const std::uint32_t u = readLE32(p);
    float f{};
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

bool readWholeFile(const char* path, std::vector<unsigned char>& bytes) {
    if (!path || !*path) return false;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        std::fclose(f);
        return false;
    }
    bytes.resize(static_cast<std::size_t>(len));
    const std::size_t read = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return read == bytes.size();
}

} // namespace

bool Mesh::loadFromFile(const char* path) {
    clear();
    std::vector<unsigned char> bytes;
    if (!readWholeFile(path, bytes)) return false;
    if (!loadFromMemory(bytes.data(), bytes.size())) return false;
    sourcePath_ = path;
    return true;
}

bool Mesh::loadFromMemory(const void* data, std::size_t size) {
    clear();
    if (!data || size < 16) return false;

    const auto* p = static_cast<const unsigned char*>(data);
    if (std::memcmp(p, "AAM1", 4) != 0) return false;

    const std::uint32_t version = readLE32(p + 4);
    const std::uint32_t vertexCount = readLE32(p + 8);
    const std::uint32_t indexCount = readLE32(p + 12);
    if (version != 1 || vertexCount == 0 || indexCount < 3 ||
        vertexCount > kMaxVertices || indexCount > kMaxIndices ||
        (indexCount % 3) != 0) return false;

    constexpr std::size_t vertexBytes = 8 * sizeof(float);
    const std::size_t expected = 16 + std::size_t(vertexCount) * vertexBytes +
                                 std::size_t(indexCount) * sizeof(std::uint32_t);
    if (expected != size) return false;

    BoundsAccumulator bounds;
    vertices_.resize(vertexCount);
    std::size_t off = 16;
    for (std::uint32_t i=0; i<vertexCount; ++i) {
        MeshVertex v{};
        v.position = {readLEFloat(p+off+0), readLEFloat(p+off+4), readLEFloat(p+off+8)};
        v.normal   = {readLEFloat(p+off+12),readLEFloat(p+off+16),readLEFloat(p+off+20)};
        v.uv       = {readLEFloat(p+off+24),readLEFloat(p+off+28)};
        v.normal = normalize(v.normal);
        vertices_[i] = v;
        bounds.add(v.position);
        off += vertexBytes;
    }
    bounds_ = bounds.finish();

    indices_.resize(indexCount);
    for (std::uint32_t i=0; i<indexCount; ++i) {
        const std::uint32_t idx = readLE32(p+off);
        if (idx >= vertexCount) {
            clear();
            return false;
        }
        indices_[i] = idx;
        off += 4;
    }
    return true;
}

void Mesh::clear() {
    vertices_.clear();
    indices_.clear();
    sourcePath_.clear();
    bounds_ = {};
}

bool SkinnedMesh::loadFromFile(const char* path) {
    clear();
    std::vector<unsigned char> bytes;
    if (!readWholeFile(path, bytes)) return false;
    if (!loadFromMemory(bytes.data(), bytes.size())) return false;
    sourcePath_ = path;
    return true;
}

bool SkinnedMesh::loadFromMemory(const void* data, std::size_t size) {
    clear();
    if (!data || size < 20) return false;
    const auto* p = static_cast<const unsigned char*>(data);
    if (std::memcmp(p, "AAM2", 4) != 0) return false;

    const std::uint32_t version = readLE32(p + 4);
    const std::uint32_t vertexCount = readLE32(p + 8);
    const std::uint32_t indexCount = readLE32(p + 12);
    const std::uint32_t boneCount = readLE32(p + 16);
    if (version != 2 || vertexCount == 0 || indexCount < 3 ||
        vertexCount > kMaxVertices || indexCount > kMaxIndices ||
        boneCount == 0 || boneCount > kMaxBones || (indexCount % 3) != 0) return false;

    constexpr std::size_t vertexBytes = 64;
    const std::size_t expected = 20 + std::size_t(vertexCount) * vertexBytes +
                                 std::size_t(indexCount) * 4;
    if (expected != size) return false;

    BoundsAccumulator bounds;
    vertices_.resize(vertexCount);
    std::size_t off = 20;
    for (std::uint32_t i=0; i<vertexCount; ++i) {
        SkinnedMeshVertex v{};
        v.position = {readLEFloat(p+off+0), readLEFloat(p+off+4), readLEFloat(p+off+8)};
        v.normal   = {readLEFloat(p+off+12),readLEFloat(p+off+16),readLEFloat(p+off+20)};
        v.uv       = {readLEFloat(p+off+24),readLEFloat(p+off+28)};
        for (int k=0;k<4;++k) v.bone[k] = readLE32(p+off+32+k*4);
        for (int k=0;k<4;++k) v.weight[k] = readLEFloat(p+off+48+k*4);
        float sum = 0.0f;
        for (int k=0;k<4;++k) {
            if (v.bone[k] >= boneCount) { clear(); return false; }
            v.weight[k] = std::max(0.0f, v.weight[k]);
            sum += v.weight[k];
        }
        if (sum < 0.00001f) {
            v.bone[0] = 0;
            v.weight[0] = 1.0f;
            sum = 1.0f;
        }
        for (int k=0;k<4;++k) v.weight[k] /= sum;
        v.normal = normalize(v.normal);
        vertices_[i] = v;
        bounds.add(v.position);
        off += vertexBytes;
    }
    bounds_ = bounds.finish();

    indices_.resize(indexCount);
    for (std::uint32_t i=0; i<indexCount; ++i) {
        const std::uint32_t idx = readLE32(p+off);
        if (idx >= vertexCount) { clear(); return false; }
        indices_[i] = idx;
        off += 4;
    }
    boneCount_ = boneCount;
    return true;
}

void SkinnedMesh::clear() {
    vertices_.clear();
    indices_.clear();
    boneCount_ = 0;
    sourcePath_.clear();
    bounds_ = {};
}

} // namespace aa
