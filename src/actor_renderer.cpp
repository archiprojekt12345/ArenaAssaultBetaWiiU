#include "renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/shaders.h>
#include <gx2r/draw.h>
#include <whb/log.h>

namespace aa {
namespace {

bool readBinaryFileActor(const char* path, std::vector<std::uint8_t>& out) {
    out.clear();
    if (!path || !*path) return false;
    FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;
    if (std::fseek(fp, 0, SEEK_END) != 0) { std::fclose(fp); return false; }
    const long size = std::ftell(fp);
    if (size <= 0) { std::fclose(fp); return false; }
    std::rewind(fp);
    out.resize(static_cast<std::size_t>(size));
    const std::size_t got = std::fread(out.data(), 1, out.size(), fp);
    std::fclose(fp);
    if (got != out.size()) { out.clear(); return false; }
    return true;
}

bool loadActorGsh(WHBGfxShaderGroup& group, const char* path) {
    std::vector<std::uint8_t> file;
    if (!readBinaryFileActor(path, file)) {
        WHBLogPrintf("ArenaAssault: actor shader file missing: %s", path ? path : "(null)");
        return false;
    }
    group = {};
    if (!WHBGfxLoadGFDShaderGroup(&group, 0, file.data())) {
        WHBLogPrintf("ArenaAssault: invalid actor GSH: %s", path);
        group = {};
        return false;
    }
    return true;
}

std::string actorPathJoin(const std::string& root, const char* rel) {
    std::string s = root;
    if (!s.empty() && s.back() != '/') s.push_back('/');
    s += rel;
    return s;
}

bool sameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool sameMaterial(const Material& a, const Material& b) {
    return sameColor(a.diffuse,b.diffuse) &&
           sameColor(a.emissive,b.emissive) &&
           a.specular == b.specular &&
           a.roughness == b.roughness &&
           a.textureMix == b.textureMix &&
           a.emissiveStrength == b.emissiveStrength &&
           a.atlas.u0 == b.atlas.u0 && a.atlas.v0 == b.atlas.v0 &&
           a.atlas.u1 == b.atlas.u1 && a.atlas.v1 == b.atlas.v1;
}

Mat4 actorModelMatrix(const Transform& t) {
    Mat4 m = Mat4::identity();
    const float c = std::cos(t.yaw);
    const float s = std::sin(t.yaw);

    m.at(0,0) = c * t.scale.x;
    m.at(1,0) = 0.0f;
    m.at(2,0) = s * t.scale.x;

    m.at(0,1) = 0.0f;
    m.at(1,1) = t.scale.y;
    m.at(2,1) = 0.0f;

    m.at(0,2) = -s * t.scale.z;
    m.at(1,2) = 0.0f;
    m.at(2,2) = c * t.scale.z;

    m.at(0,3) = t.position.x;
    m.at(1,3) = t.position.y;
    m.at(2,3) = t.position.z;
    return m;
}

} // namespace

bool Renderer::initActorPipeline() {
    if (actorPipelineReady_) return true;

    const std::string path = actorPathJoin(contentRoot_, "shaders/actor3d.gsh");
    if (!loadActorGsh(actorGroup_, path.c_str())) return false;

    const bool ok =
        WHBGfxInitShaderAttribute(&actorGroup_, "in_position", 0,
            offsetof(Vertex3D, position), GX2_ATTRIB_FORMAT_FLOAT_32_32_32) &&
        WHBGfxInitShaderAttribute(&actorGroup_, "in_normal", 0,
            offsetof(Vertex3D, normal), GX2_ATTRIB_FORMAT_FLOAT_32_32_32) &&
        WHBGfxInitShaderAttribute(&actorGroup_, "in_uv", 0,
            offsetof(Vertex3D, uv), GX2_ATTRIB_FORMAT_FLOAT_32_32) &&
        WHBGfxInitShaderAttribute(&actorGroup_, "in_diffuse", 0,
            offsetof(Vertex3D, diffuse), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitShaderAttribute(&actorGroup_, "in_emissive", 0,
            offsetof(Vertex3D, emissive), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitShaderAttribute(&actorGroup_, "in_surface", 0,
            offsetof(Vertex3D, surface), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitFetchShader(&actorGroup_);

    if (!ok) {
        WHBLogPrintf("ArenaAssault: actor fetch shader init failed");
        destroyShaderGroup(actorGroup_);
        return false;
    }

    actorPipelineReady_ = true;
    WHBLogPrintf("ArenaAssault: indexed actor pipeline ready");
    return true;
}

bool Renderer::ensureActorIndexBuffer(const SkinnedMesh& mesh) {
    const auto& indices = mesh.indices();
    if (indices.empty() || indices.size() > 0xFFFFFFFFu) return false;

    if (actorIndexBuffer_.buffer && actorIndexMesh_ == &mesh &&
        actorIndexCount_ == static_cast<std::uint32_t>(indices.size())) {
        return true;
    }

    if (actorIndexBuffer_.buffer) GX2RDestroyBufferEx(&actorIndexBuffer_, 0);
    actorIndexBuffer_ = {};
    actorIndexMesh_ = nullptr;
    actorIndexCount_ = 0;

    actorIndexBuffer_.flags = GX2R_RESOURCE_BIND_INDEX_BUFFER |
                              GX2R_RESOURCE_USAGE_CPU_WRITE |
                              GX2R_RESOURCE_USAGE_GPU_READ;
    actorIndexBuffer_.elemSize = sizeof(std::uint32_t);
    actorIndexBuffer_.elemCount = static_cast<std::uint32_t>(indices.size());
    if (GX2RCreateBuffer(&actorIndexBuffer_) != TRUE) {
        WHBLogPrintf("ArenaAssault: actor index buffer allocation failed");
        actorIndexBuffer_ = {};
        return false;
    }

    void* dst = GX2RLockBufferEx(&actorIndexBuffer_, 0);
    if (!dst) {
        GX2RDestroyBufferEx(&actorIndexBuffer_, 0);
        actorIndexBuffer_ = {};
        return false;
    }
    std::memcpy(dst, indices.data(), indices.size() * sizeof(std::uint32_t));
    GX2RUnlockBufferEx(&actorIndexBuffer_, 0);

    actorIndexMesh_ = &mesh;
    actorIndexCount_ = static_cast<std::uint32_t>(indices.size());
    WHBLogPrintf("ArenaAssault: actor index buffer uploaded indices=%u",
                 static_cast<unsigned>(actorIndexCount_));
    return true;
}

bool Renderer::ensureActorVertexBuffer(ActorSkinCache& cache, std::uint32_t vertexCount) {
    if (vertexCount == 0) return false;
    if (cache.gpuVertexBuffer.buffer && cache.gpuVertexCapacity >= vertexCount) return true;

    if (cache.gpuVertexBuffer.buffer) GX2RDestroyBufferEx(&cache.gpuVertexBuffer, 0);
    cache.gpuVertexBuffer = {};
    cache.gpuVertexCapacity = 0;
    cache.materialValid = false;

    if (!initBuffer(cache.gpuVertexBuffer, sizeof(Vertex3D), vertexCount)) {
        WHBLogPrintf("ArenaAssault: actor vertex buffer allocation failed vertices=%u",
                     static_cast<unsigned>(vertexCount));
        cache.gpuVertexBuffer = {};
        return false;
    }
    cache.gpuVertexCapacity = vertexCount;
    return true;
}

bool Renderer::uploadActorVertices(ActorSkinCache& cache, const Material& material) {
    const std::size_t count = cache.localVertices.size();
    if (!cache.gpuVertexBuffer.buffer || count == 0 || count > cache.gpuVertexCapacity) return false;

    void* raw = GX2RLockBufferEx(&cache.gpuVertexBuffer, 0);
    if (!raw) return false;
    Vertex3D* dst = static_cast<Vertex3D*>(raw);
    for (std::size_t i=0;i<count;++i) {
        const auto& v = cache.localVertices[i];
        dst[i] = makePreparedVertex(v.position, v.normal, v.uv, material);
    }
    GX2RUnlockBufferEx(&cache.gpuVertexBuffer, 0);

    cache.lastMaterial = material;
    cache.materialValid = true;
    ++stats_.actorVertexUploads;
    return true;
}

void Renderer::uploadActorUniforms(ActorSkinCache& cache, const Transform& transform) {
    // First 48 floats are exactly the already byte-swapped scene block.
    std::memcpy(cache.uniformBlock, sceneUniformBlock_, sizeof(sceneUniformBlock_));

    const Mat4 model = actorModelMatrix(transform);
    for (int i=0;i<16;++i) cache.uniformBlock[48+i] = _swapF32(model.m[i]);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK,
                  cache.uniformBlock, sizeof(cache.uniformBlock));
}

void Renderer::bindActorPipeline(ActorSkinCache& cache, const Transform& transform) {
    uploadActorUniforms(cache, transform);

    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    GX2SetFetchShader(&actorGroup_.fetchShader);
    GX2SetVertexShader(actorGroup_.vertexShader);
    GX2SetPixelShader(actorGroup_.pixelShader);
    GX2SetVertexUniformBlock(0, sizeof(cache.uniformBlock), cache.uniformBlock);
    GX2SetPixelUniformBlock(0, sizeof(cache.uniformBlock), cache.uniformBlock);

    if (atlas_.texture() && actorGroup_.pixelShader->samplerVarCount > 0) {
        const std::uint32_t loc = actorGroup_.pixelShader->samplerVars[0].location;
        GX2SetPixelTexture(atlas_.texture(), loc);
        GX2SetPixelSampler(atlas_.sampler(), loc);
    }
}

void Renderer::submitSkinnedMeshIndexed(const SkinnedMesh& mesh, const SkeletonPose& pose,
                                        const Transform& transform, const Material& material) {
    if (!mesh.valid() || pose.count == 0) return;

    const std::size_t slot = actorSubmitCursor_++ % kActorCacheSlots;
    MeshBounds animatedBounds = mesh.bounds();
    animatedBounds.radius *= 1.20f;

    const float maxScale = std::max({
        std::fabs(transform.scale.x), std::fabs(transform.scale.y), std::fabs(transform.scale.z)
    });
    const Vec3 worldCenter = transformPoint(transform, animatedBounds.center);
    const float worldRadius = animatedBounds.radius * maxScale;
    const bool visible = !cameraValid_ || sphereVisible(currentCamera_, worldCenter, worldRadius);
    const float distance = cameraValid_ ? length(worldCenter-currentCamera_.pos) : 0.0f;
    const EnemyRenderTier tier = selectEnemyRenderTier(distance, visible);

    if (tier == EnemyRenderTier::Culled) {
        ++stats_.culledMeshes;
        ++stats_.culledAam2Actors;
        return;
    }

    auto lowFallback = [&]() {
        ++stats_.lowDetailActors;
        if (enemyLowDetailMesh_.valid()) submitMesh(enemyLowDetailMesh_, transform, material);
    };

    if (tier == EnemyRenderTier::Low) {
        lowFallback();
        return;
    }

    if (!initActorPipeline() || !ensureActorIndexBuffer(mesh)) {
        lowFallback();
        return;
    }

    ActorSkinCache& cache = actorSkinCache_[slot];
    const bool cacheShapeMismatch = cache.localVertices.size() != mesh.vertices().size();
    const bool actorChanged = !cache.valid ||
        lengthSq(transform.position-cache.lastWorldPosition) > (0.25f*0.25f);
    const bool refreshPose = tier == EnemyRenderTier::High ||
                             cacheShapeMismatch || actorChanged ||
                             shouldRefreshMediumPose(frameIndex_, slot);
    const bool materialChanged = !cache.materialValid ||
                                 !sameMaterial(cache.lastMaterial, material);

    if (refreshPose) {
        const SkinningStats skinStats = skinUniqueVertices(mesh.vertices(), pose, cache.localVertices);
        stats_.skinnedVertices += static_cast<std::uint32_t>(skinStats.evaluatedVertices);
        cache.valid = true;
    }

    if (cache.localVertices.empty() ||
        cache.localVertices.size() > 0xFFFFFFFFu ||
        !ensureActorVertexBuffer(cache, static_cast<std::uint32_t>(cache.localVertices.size()))) {
        lowFallback();
        cache.lastWorldPosition = transform.position;
        return;
    }

    if (refreshPose || materialChanged || !cache.materialValid) {
        if (!uploadActorVertices(cache, material)) {
            lowFallback();
            cache.lastWorldPosition = transform.position;
            return;
        }
    }

    // The generic dynamic batch and direct indexed actor draw must not overlap
    // in ordering/state. Flush pending scene triangles before the actor draw.
    flush3DBatch();
    bindActorPipeline(cache, transform);
    GX2RSetAttributeBuffer(&cache.gpuVertexBuffer, 0, sizeof(Vertex3D), 0);
    GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES,
                     actorIndexCount_,
                     GX2_INDEX_TYPE_U32,
                     actorIndexBuffer_.buffer,
                     0,
                     1);

    ++stats_.visibleAam2Actors;
    ++stats_.indexedActorDraws;
    stats_.actorTriangles += actorIndexCount_ / 3u;
    cache.lastWorldPosition = transform.position;
    cache.valid = true;
}

void Renderer::clearActorGpuResources() {
    if (actorIndexBuffer_.buffer) GX2RDestroyBufferEx(&actorIndexBuffer_, 0);
    actorIndexBuffer_ = {};
    actorIndexMesh_ = nullptr;
    actorIndexCount_ = 0;

    for (auto& cache : actorSkinCache_) {
        if (cache.gpuVertexBuffer.buffer) GX2RDestroyBufferEx(&cache.gpuVertexBuffer, 0);
        cache.gpuVertexBuffer = {};
        cache.gpuVertexCapacity = 0;
        cache.materialValid = false;
        cache.valid = false;
    }

    if (actorPipelineReady_) destroyShaderGroup(actorGroup_);
    actorGroup_ = {};
    actorPipelineReady_ = false;
}

void Renderer::shutdownActorGpu() {
    clearActorGpuResources();
}

} // namespace aa
