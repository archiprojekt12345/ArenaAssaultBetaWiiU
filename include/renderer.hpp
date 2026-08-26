#pragma once

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
#include "texture.hpp"
#include "wiiu_compat.hpp"
#include "world_asset_layout.hpp"

namespace aa {

class Renderer {
public:
    static constexpr std::uint32_t kMaxVertices3D = 65536;
    static constexpr std::uint32_t kMaxVertices2D = 32768;

    struct RenderStats {
        std::uint32_t batches3D{};
        std::uint32_t submittedTriangles{};
        std::uint32_t culledMeshes{};
    };

    bool init(const char* contentRoot);
    void shutdown();

    void begin3D(const Camera& camera);
    void submitBox(const Vec3& center, const Vec3& half, float yaw,
                   const Material& material);
    void submitMesh(const Mesh& mesh, const Transform& transform,
                    const Material& material);
    void submitSkinnedMesh(const SkinnedMesh& mesh, const SkeletonPose& pose,
                           const Transform& transform, const Material& material);
    void flush3D();

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

    bool initScenePipeline();
    bool initUIPipeline();
    bool initBuffer(GX2RBuffer& buffer, std::size_t elemSize, std::uint32_t elemCount);
    void destroyShaderGroup(WHBGfxShaderGroup& group);
    void pushTri3D(const Vertex3D& a, const Vertex3D& b, const Vertex3D& c);
    void flush3DBatch();
    bool meshVisible(const MeshBounds& bounds, const Transform& transform) const;
    Vertex3D makeVertex(const Vec3& position, const Vec3& normal,
                        const Vec2& uv, const Material& material) const;
    Vec2 atlasUV(const Vec2& uv, const Material& material) const;
    void uploadSceneUniforms(const Camera& camera);

    void loadWorldAssets();
    void clearWorldAssets();
    void submitWorldAssets();
    void submitCorridorPortal(const WorldAssetPlacement& placement);
    void submitSupplyCrate(const WorldAssetPlacement& placement, std::size_t index);

    WHBGfxShaderGroup sceneGroup_{};
    WHBGfxShaderGroup uiGroup_{};
    GX2RBuffer sceneVertexBuffer_{};
    GX2RBuffer uiVertexBuffer_{};
    std::vector<Vertex3D> sceneVertices_;
    std::vector<Vertex2D> uiVertices_;
    TextureAtlas atlas_{};
    std::string contentRoot_{};

    Camera currentCamera_{};
    bool cameraValid_{};
    RenderStats stats_{};

    Mesh corridorLightMesh_{};
    Mesh corridorWhiteMesh_{};
    Mesh corridorGrayMesh_{};
    Mesh corridorBlackMesh_{};
    Mesh corridorBlueMesh_{};
    Mesh corridorYellowMesh_{};
    Mesh corridorGlassMesh_{};
    Mesh corridorDetailMesh_{};
    Mesh supplyCrateMesh_{};

    alignas(256) float sceneUniformBlock_[48]{};
};

} // namespace aa
