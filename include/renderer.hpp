#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gx2/sampler.h>
#include <gx2/texture.h>
#include <gx2r/buffer.h>
#include <whb/gfx.h>

#include "animation.hpp"
#include "camera.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "render_policy.hpp"
#include "skinning.hpp"
#include "texture.hpp"
#include "wiiu_compat.hpp"
#include "world_asset_layout.hpp"

namespace aa {

class Renderer {
public:
    static constexpr std::uint32_t kMaxVertices3D = 65536;
    static constexpr std::uint32_t kMaxVertices2D = 32768;
    static constexpr std::size_t kActorCacheSlots = 16;

    enum class UITarget {
        TV,
        DRC
    };

    struct RenderStats {
        std::uint32_t batches3D{};
        std::uint32_t submittedTriangles{};
        std::uint32_t culledMeshes{};
        std::uint32_t visibleAam2Actors{};
        std::uint32_t culledAam2Actors{};
        std::uint32_t lowDetailActors{};
        std::uint32_t skinnedVertices{};
        std::uint32_t actorTriangles{};
        std::uint32_t indexedActorDraws{};
        std::uint32_t actorVertexUploads{};
        std::uint32_t staticWorldBuilds{};
        std::uint32_t staticWorldDrawBatches{};
        std::uint32_t activeParticles{};
        std::uint32_t particleTriangles{};
    };

    bool init(const char* contentRoot);
    void shutdown();

    void begin3D(const Camera& camera);
    void submitBox(const Vec3& center, const Vec3& half, float yaw,
                   const Material& material);
    void submitBillboardQuad(const Vec3& center, float halfSize,
                             const Material& material);
    void submitMesh(const Mesh& mesh, const Transform& transform,
                    const Material& material);
    void submitSkinnedMesh(const SkinnedMesh& mesh, const SkeletonPose& pose,
                           const Transform& transform, const Material& material,
                           std::size_t actorSlot);
    void flush3D();

    void setUITarget(UITarget target) { uiTarget_ = target; }
    void begin2D();
    void tri2D(const Vec4& a, const Vec4& b, const Vec4& c,
               const Color& color);
    void rect2D(float x0, float y0, float x1, float y1,
                const Color& color, float z = 0.0f);
    void flush2D();

    bool usingExternalAtlas() const { return atlas_.loadedExternal(); }
    const RenderStats& stats() const { return stats_; }

private:
    struct Vertex3D {
        Vec3 position;
        Vec3 normal;
        Vec2 uv;
        Color diffuse;
        Color emissive;
        Vec4 surface; // specular, roughness, textureMix, emissiveStrength
    };

    struct Vertex2D {
        Vec4 position;
        Color color;
    };

    struct ActorSkinCache {
        std::vector<SkinnedVertexOutput> localVertices;
        GX2RBuffer gpuVertexBuffer{};
        std::uint32_t gpuVertexCapacity{};
        Material lastMaterial{};
        bool materialValid{};
        bool valid{};
        alignas(256) float uniformBlock[64]{};
    };

    struct StaticWorldRange {
        std::uint32_t firstVertex{};
        std::uint32_t vertexCount{};
        MeshBounds bounds{};
    };

    bool initScenePipeline();
    bool initActorPipeline();
    bool initUIPipeline();
    bool initBuffer(GX2RBuffer& buffer, std::size_t elemSize, std::uint32_t elemCount);
    bool ensureActorIndexBuffer(const SkinnedMesh& mesh);
    bool ensureActorVertexBuffer(ActorSkinCache& cache, std::uint32_t vertexCount);
    bool uploadActorVertices(ActorSkinCache& cache, const Material& material);
    void clearActorGpuResources();
    void destroyShaderGroup(WHBGfxShaderGroup& group);
    void bindScenePipeline();
    void bindActorPipeline(ActorSkinCache& cache, const Transform& transform);
    void pushTri3D(const Vertex3D& a, const Vertex3D& b, const Vertex3D& c);
    void flush3DBatch();
    bool meshVisible(const MeshBounds& bounds, const Transform& transform) const;
    Vertex3D makeVertex(const Vec3& position, const Vec3& normal,
                        const Vec2& uv, const Material& material) const;
    Vertex3D makePreparedVertex(const Vec3& position, const Vec3& normal,
                                const Vec2& uv, const Material& material) const;
    Vec2 atlasUV(const Vec2& uv, const Material& material) const;
    void uploadSceneUniforms(const Camera& camera);
    void uploadActorUniforms(ActorSkinCache& cache, const Transform& transform);

    void loadWorldAssets();
    void clearWorldAssets();
    bool buildStaticWorldCache();
    void clearStaticWorldCache();
    void drawStaticWorld();

    WHBGfxShaderGroup sceneGroup_{};
    WHBGfxShaderGroup actorGroup_{};
    WHBGfxShaderGroup uiGroup_{};
    GX2RBuffer sceneVertexBuffer_{};
    GX2RBuffer actorIndexBuffer_{};
    GX2RBuffer uiTvVertexBuffer_{};
    GX2RBuffer uiDrcVertexBuffer_{};
    GX2RBuffer staticWorldBuffer_{};
    std::vector<Vertex3D> sceneVertices_;
    std::vector<Vertex2D> uiVertices_;
    std::vector<StaticWorldRange> staticWorldRanges_;
    TextureAtlas atlas_{};
    std::string contentRoot_{};

    Camera currentCamera_{};
    Vec3 currentCameraForward_{0,0,-1};
    Vec3 currentCameraRight_{1,0,0};
    Vec3 currentCameraUp_{0,1,0};
    bool cameraValid_{};
    UITarget uiTarget_{UITarget::TV};
    RenderStats stats_{};
    std::uint32_t staticWorldBuildCount_{};
    std::uint64_t frameIndex_{};
    std::array<ActorSkinCache,kActorCacheSlots> actorSkinCache_{};
    const SkinnedMesh* actorIndexMesh_{};
    std::uint32_t actorIndexCount_{};

    Mesh corridorLightMesh_{};
    Mesh corridorWhiteMesh_{};
    Mesh corridorGrayMesh_{};
    Mesh corridorBlackMesh_{};
    Mesh corridorBlueMesh_{};
    Mesh corridorYellowMesh_{};
    Mesh corridorGlassMesh_{};
    Mesh corridorDetailMesh_{};
    Mesh supplyCrateMesh_{};
    Mesh enemyLowDetailMesh_{};

    alignas(256) float sceneUniformBlock_[48]{};
};

} // namespace aa
