#include "renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2r/draw.h>
#include <whb/log.h>

namespace aa {
namespace {

bool readBinaryFile(const char* path, std::vector<std::uint8_t>& out) {
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

bool loadGshGroup(WHBGfxShaderGroup& group, const char* path) {
    std::vector<std::uint8_t> file;
    if (!readBinaryFile(path, file)) {
        WHBLogPrintf("ArenaAssault: shader file missing: %s", path ? path : "(null)");
        return false;
    }
    group = {};
    if (!WHBGfxLoadGFDShaderGroup(&group, 0, file.data())) {
        WHBLogPrintf("ArenaAssault: invalid GSH: %s", path);
        group = {};
        return false;
    }
    return true;
}

std::string pathJoin(const char* root, const char* rel) {
    std::string s = root ? root : "";
    if (!s.empty() && s.back() != '/') s.push_back('/');
    s += rel;
    return s;
}

} // namespace

bool Renderer::init(const char* contentRoot) {
    contentRoot_ = contentRoot ? contentRoot : "";
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);

    if (!initScenePipeline() || !initUIPipeline()) {
        shutdown();
        return false;
    }

    if (!initBuffer(sceneVertexBuffer_, sizeof(Vertex3D), kMaxVertices3D) ||
        !initBuffer(uiVertexBuffer_, sizeof(Vertex2D), kMaxVertices2D)) {
        WHBLogPrintf("ArenaAssault: vertex buffer allocation failed");
        shutdown();
        return false;
    }

    const std::string atlasPath = pathJoin(contentRoot, "assets/textures/arena_atlas.tga");
    if (!atlas_.init(atlasPath.c_str())) {
        WHBLogPrintf("ArenaAssault: could not create texture atlas or fallback");
        shutdown();
        return false;
    }

    loadWorldAssets();
    if (!buildStaticWorldCache()) {
        WHBLogPrintf("ArenaAssault: static V11 world cache unavailable; world props disabled");
        clearStaticWorldCache();
    }

    sceneVertices_.reserve(kMaxVertices3D);
    uiVertices_.reserve(kMaxVertices2D);
    for (auto& cache : actorSkinCache_) {
        cache.localVertices.reserve(4096);
        cache.worldVertices.reserve(4096);
    }

    WHBLogPrintf("ArenaAssault: atlas=%s staticGroups=%u",
                 atlas_.loadedExternal() ? "external TGA" : "1x1 fallback",
                 static_cast<unsigned>(staticWorldRanges_.size()));
    return true;
}

void Renderer::shutdown() {
    clearStaticWorldCache();
    if (sceneVertexBuffer_.buffer) GX2RDestroyBufferEx(&sceneVertexBuffer_, 0);
    if (uiVertexBuffer_.buffer) GX2RDestroyBufferEx(&uiVertexBuffer_, 0);
    sceneVertexBuffer_ = {};
    uiVertexBuffer_ = {};

    sceneVertices_.clear();
    uiVertices_.clear();
    for (auto& cache : actorSkinCache_) {
        cache.localVertices.clear();
        cache.worldVertices.clear();
        cache.valid = false;
    }
    actorSubmitCursor_ = 0;
    frameIndex_ = 0;
    cameraValid_ = false;
    stats_ = {};

    clearWorldAssets();
    atlas_.shutdown();
    destroyShaderGroup(sceneGroup_);
    destroyShaderGroup(uiGroup_);
}

void Renderer::loadWorldAssets() {
    auto load = [&](Mesh& mesh, const char* rel, const char* label) {
        const std::string path = pathJoin(contentRoot_.c_str(), rel);
        if (mesh.loadFromFile(path.c_str())) {
            WHBLogPrintf("ArenaAssault: loaded V11 asset %s", label);
        } else {
            WHBLogPrintf("ArenaAssault: optional V11 asset missing: %s", rel);
        }
    };

    load(corridorLightMesh_,  "assets/meshes/corridor_light.aam",  "corridor light");
    load(corridorWhiteMesh_,  "assets/meshes/corridor_white.aam",  "corridor white metal");
    load(corridorGrayMesh_,   "assets/meshes/corridor_gray.aam",   "corridor gray metal");
    load(corridorBlackMesh_,  "assets/meshes/corridor_black.aam",  "corridor black metal");
    load(corridorBlueMesh_,   "assets/meshes/corridor_blue.aam",   "corridor blue lamps");
    load(corridorYellowMesh_, "assets/meshes/corridor_yellow.aam", "corridor yellow structure");
    load(corridorGlassMesh_,  "assets/meshes/corridor_glass.aam",  "corridor glass");
    load(corridorDetailMesh_, "assets/meshes/corridor_detail.aam", "corridor details");
    load(supplyCrateMesh_,    "assets/meshes/supply_crate.aam",    "supply crate");
    load(enemyLowDetailMesh_, "assets/meshes/enemy_body.aam",      "enemy low-detail fallback");
}

void Renderer::clearWorldAssets() {
    corridorLightMesh_.clear();
    corridorWhiteMesh_.clear();
    corridorGrayMesh_.clear();
    corridorBlackMesh_.clear();
    corridorBlueMesh_.clear();
    corridorYellowMesh_.clear();
    corridorGlassMesh_.clear();
    corridorDetailMesh_.clear();
    supplyCrateMesh_.clear();
    enemyLowDetailMesh_.clear();
}

bool Renderer::initScenePipeline() {
    const char* root = contentRoot_.c_str();
    const std::string path = pathJoin(root, "shaders/scene3d.gsh");
    if (!loadGshGroup(sceneGroup_, path.c_str())) return false;

    const bool ok =
        WHBGfxInitShaderAttribute(&sceneGroup_, "in_position", 0,
            offsetof(Vertex3D, position), GX2_ATTRIB_FORMAT_FLOAT_32_32_32) &&
        WHBGfxInitShaderAttribute(&sceneGroup_, "in_normal", 0,
            offsetof(Vertex3D, normal), GX2_ATTRIB_FORMAT_FLOAT_32_32_32) &&
        WHBGfxInitShaderAttribute(&sceneGroup_, "in_uv", 0,
            offsetof(Vertex3D, uv), GX2_ATTRIB_FORMAT_FLOAT_32_32) &&
        WHBGfxInitShaderAttribute(&sceneGroup_, "in_diffuse", 0,
            offsetof(Vertex3D, diffuse), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitShaderAttribute(&sceneGroup_, "in_emissive", 0,
            offsetof(Vertex3D, emissive), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitShaderAttribute(&sceneGroup_, "in_surface", 0,
            offsetof(Vertex3D, surface), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitFetchShader(&sceneGroup_);

    if (!ok) WHBLogPrintf("ArenaAssault: 3D fetch shader init failed");
    return ok;
}

bool Renderer::initUIPipeline() {
    const char* root = contentRoot_.c_str();
    const std::string path = pathJoin(root, "shaders/ui2d.gsh");
    if (!loadGshGroup(uiGroup_, path.c_str())) return false;

    const bool ok =
        WHBGfxInitShaderAttribute(&uiGroup_, "in_position", 0,
            offsetof(Vertex2D, position), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitShaderAttribute(&uiGroup_, "in_color", 0,
            offsetof(Vertex2D, color), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) &&
        WHBGfxInitFetchShader(&uiGroup_);
    if (!ok) WHBLogPrintf("ArenaAssault: UI fetch shader init failed");
    return ok;
}

bool Renderer::initBuffer(GX2RBuffer& buffer, std::size_t elemSize,
                          std::uint32_t elemCount) {
    buffer = {};
    buffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER |
                   GX2R_RESOURCE_USAGE_CPU_READ |
                   GX2R_RESOURCE_USAGE_CPU_WRITE |
                   GX2R_RESOURCE_USAGE_GPU_READ;
    buffer.elemSize = static_cast<std::uint32_t>(elemSize);
    buffer.elemCount = elemCount;
    return GX2RCreateBuffer(&buffer) == TRUE;
}

void Renderer::destroyShaderGroup(WHBGfxShaderGroup& group) {
    WHBGfxFreeShaderGroup(&group);
    group = {};
}

void Renderer::bindScenePipeline() {
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    GX2SetFetchShader(&sceneGroup_.fetchShader);
    GX2SetVertexShader(sceneGroup_.vertexShader);
    GX2SetPixelShader(sceneGroup_.pixelShader);
    GX2SetVertexUniformBlock(0, sizeof(sceneUniformBlock_), sceneUniformBlock_);
    GX2SetPixelUniformBlock(0, sizeof(sceneUniformBlock_), sceneUniformBlock_);

    if (atlas_.texture() && sceneGroup_.pixelShader->samplerVarCount > 0) {
        const std::uint32_t loc = sceneGroup_.pixelShader->samplerVars[0].location;
        GX2SetPixelTexture(atlas_.texture(), loc);
        GX2SetPixelSampler(atlas_.sampler(), loc);
    }
}

void Renderer::begin3D(const Camera& camera) {
    sceneVertices_.clear();
    currentCamera_ = camera;
    currentCameraForward_ = cameraForward(camera);
    currentCameraRight_ = cameraRight(camera);
    currentCameraUp_ = cameraUp(camera);
    cameraValid_ = true;
    actorSubmitCursor_ = 0;
    stats_ = {};
    stats_.staticWorldBuilds = staticWorldBuildCount_;
    uploadSceneUniforms(camera);
    drawStaticWorld();
}

Vec2 Renderer::atlasUV(const Vec2& uv, const Material& material) const {
    return {
        material.atlas.u0 + (material.atlas.u1-material.atlas.u0)*uv.x,
        material.atlas.v0 + (material.atlas.v1-material.atlas.v0)*uv.y
    };
}

Renderer::Vertex3D Renderer::makeVertex(const Vec3& position, const Vec3& normal,
                                        const Vec2& uv,
                                        const Material& material) const {
    return makePreparedVertex(position,normalize(normal),uv,material);
}

Renderer::Vertex3D Renderer::makePreparedVertex(const Vec3& position, const Vec3& normal,
                                                const Vec2& uv,
                                                const Material& material) const {
    return {
        position,
        normal,
        atlasUV(uv, material),
        material.diffuse,
        material.emissive,
        {material.specular, material.roughness,
         material.textureMix, material.emissiveStrength}
    };
}

bool Renderer::buildStaticWorldCache() {
    clearStaticWorldCache();

    std::vector<Vertex3D> vertices;
    vertices.reserve(160000);
    staticWorldRanges_.reserve(staticWorldGroupCount());

    auto appendMesh = [&](const Mesh& mesh, const Transform& transform,
                          const Material& material, BoundsAccumulator& bounds) {
        if (!mesh.valid()) return;
        const auto& src = mesh.vertices();
        const auto& indices = mesh.indices();
        for (std::uint32_t index : indices) {
            const MeshVertex& v = src[index];
            const Vec3 p = transformPoint(transform,v.position);
            const Vec3 n = transformNormal(transform,v.normal);
            vertices.push_back(makePreparedVertex(p,n,v.uv,material));
            bounds.add(p);
        }
    };

    auto appendBox = [&](const Vec3& center, const Vec3& half, float yaw,
                         const Material& material, BoundsAccumulator& bounds) {
        const Vec3 local[8] = {
            {-half.x,-half.y,-half.z}, { half.x,-half.y,-half.z},
            { half.x, half.y,-half.z}, {-half.x, half.y,-half.z},
            {-half.x,-half.y, half.z}, { half.x,-half.y, half.z},
            { half.x, half.y, half.z}, {-half.x, half.y, half.z}
        };
        const int faces[6][4] = {
            {0,1,2,3}, {5,4,7,6}, {4,0,3,7},
            {1,5,6,2}, {3,2,6,7}, {4,5,1,0}
        };
        const Vec3 normals[6] = {
            {0,0,-1}, {0,0,1}, {-1,0,0},
            {1,0,0}, {0,1,0}, {0,-1,0}
        };
        const Vec2 uv[4] = {{0,0},{1,0},{1,1},{0,1}};
        Vec3 world[8];
        for (int i=0;i<8;++i) {
            world[i] = center + rotateY(local[i],yaw);
            bounds.add(world[i]);
        }
        for (int f=0;f<6;++f) {
            const Vec3 n = rotateY(normals[f],yaw);
            const int a=faces[f][0], b=faces[f][1], c=faces[f][2], d=faces[f][3];
            const Vertex3D va=makePreparedVertex(world[a],n,uv[0],material);
            const Vertex3D vb=makePreparedVertex(world[b],n,uv[1],material);
            const Vertex3D vc=makePreparedVertex(world[c],n,uv[2],material);
            const Vertex3D vd=makePreparedVertex(world[d],n,uv[3],material);
            vertices.push_back(va); vertices.push_back(vb); vertices.push_back(vc);
            vertices.push_back(va); vertices.push_back(vc); vertices.push_back(vd);
        }
    };

    auto finishGroup = [&](std::size_t first, const BoundsAccumulator& bounds) {
        if (vertices.size() <= first) return;
        const std::size_t count = vertices.size() - first;
        if (first > std::numeric_limits<std::uint32_t>::max() ||
            count > std::numeric_limits<std::uint32_t>::max()) return;
        staticWorldRanges_.push_back({
            static_cast<std::uint32_t>(first),
            static_cast<std::uint32_t>(count),
            bounds.finish()
        });
    };

    for (const auto& placement : kCorridorPortalPlacements) {
        const std::size_t first = vertices.size();
        BoundsAccumulator bounds;
        Transform t{};
        t.position = placement.position;
        t.scale = placement.scale;
        t.yaw = placement.yaw;

        Material white = materials::wall({0.48f,0.52f,0.56f,1.0f});
        Material gray = materials::wall({0.24f,0.28f,0.32f,1.0f});
        Material black = materials::darkMetal();
        Material yellow = materials::wall({0.72f,0.48f,0.08f,1.0f});
        Material glass = materials::wall({0.08f,0.18f,0.22f,1.0f});
        Material detail = materials::darkMetal();
        Material warmLight = materials::emissive({0.72f,0.86f,1.00f,1.0f},2.2f);
        Material blueLight = materials::emissive({0.04f,0.58f,0.98f,1.0f},3.0f);
        white.textureMix=gray.textureMix=black.textureMix=yellow.textureMix=0.0f;
        glass.textureMix=detail.textureMix=warmLight.textureMix=blueLight.textureMix=0.0f;

        appendMesh(corridorGrayMesh_,t,gray,bounds);
        appendMesh(corridorBlackMesh_,t,black,bounds);
        appendMesh(corridorWhiteMesh_,t,white,bounds);
        appendMesh(corridorYellowMesh_,t,yellow,bounds);
        appendMesh(corridorGlassMesh_,t,glass,bounds);
        appendMesh(corridorDetailMesh_,t,detail,bounds);
        appendMesh(corridorLightMesh_,t,warmLight,bounds);
        appendMesh(corridorBlueMesh_,t,blueLight,bounds);

        Material hazardYellow = materials::emissive({0.92f,0.58f,0.04f,1.0f},0.55f);
        hazardYellow.diffuse = {0.38f,0.24f,0.03f,1.0f};
        hazardYellow.textureMix = 0.0f;
        Material hazardBlack = materials::darkMetal();
        hazardBlack.diffuse = {0.025f,0.03f,0.035f,1.0f};
        hazardBlack.textureMix = 0.0f;
        for (int i=-4;i<=4;++i) {
            const Vec3 local{float(i)*0.62f,0.035f,1.34f};
            const Vec3 world = placement.position + rotateY(local,placement.yaw);
            appendBox(world,{0.27f,0.025f,0.11f},placement.yaw,
                      (i&1)?hazardBlack:hazardYellow,bounds);
        }
        finishGroup(first,bounds);
    }

    for (std::size_t i=0;i<kSupplyCratePlacements.size();++i) {
        const auto& placement = kSupplyCratePlacements[i];
        const std::size_t first = vertices.size();
        BoundsAccumulator bounds;
        Transform t{};
        t.position=placement.position;
        t.scale=placement.scale;
        t.yaw=placement.yaw;

        Material crate=materials::darkMetal();
        crate.diffuse={0.15f,0.18f,0.20f,1.0f};
        crate.textureMix=0.0f;
        appendMesh(supplyCrateMesh_,t,crate,bounds);

        Color panelColor{0.04f,0.65f,0.96f,1.0f};
        if ((i%3)==1) panelColor={0.92f,0.58f,0.06f,1.0f};
        if ((i%3)==2) panelColor={0.08f,0.90f,0.42f,1.0f};
        Material panel=materials::emissive(panelColor,2.1f);
        panel.textureMix=0.0f;
        const Vec3 localPanel{0.0f,0.36f*placement.scale.y,-0.505f*placement.scale.z};
        const Vec3 panelPos=placement.position+rotateY(localPanel,placement.yaw);
        appendBox(panelPos,
                  {0.22f*placement.scale.x,0.075f*placement.scale.y,0.025f*placement.scale.z},
                  placement.yaw,panel,bounds);
        finishGroup(first,bounds);
    }

    if (vertices.empty()) return true;
    if (vertices.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    if (!initBuffer(staticWorldBuffer_,sizeof(Vertex3D),
                    static_cast<std::uint32_t>(vertices.size()))) return false;

    void* dst=GX2RLockBufferEx(&staticWorldBuffer_,0);
    if (!dst) {
        clearStaticWorldCache();
        return false;
    }
    std::memcpy(dst,vertices.data(),vertices.size()*sizeof(Vertex3D));
    GX2RUnlockBufferEx(&staticWorldBuffer_,0);
    ++staticWorldBuildCount_;

    WHBLogPrintf("ArenaAssault: static world cached vertices=%u groups=%u",
                 static_cast<unsigned>(vertices.size()),
                 static_cast<unsigned>(staticWorldRanges_.size()));
    return true;
}

void Renderer::clearStaticWorldCache() {
    if (staticWorldBuffer_.buffer) GX2RDestroyBufferEx(&staticWorldBuffer_,0);
    staticWorldBuffer_={};
    staticWorldRanges_.clear();
}

void Renderer::drawStaticWorld() {
    if (!staticWorldBuffer_.buffer || staticWorldRanges_.empty()) return;
    bindScenePipeline();
    for (const auto& range : staticWorldRanges_) {
        if (cameraValid_ && range.bounds.valid &&
            !sphereVisible(currentCamera_,range.bounds.center,range.bounds.radius)) {
            continue;
        }
        GX2RSetAttributeBuffer(&staticWorldBuffer_,0,sizeof(Vertex3D),
                               range.firstVertex*sizeof(Vertex3D));
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES,range.vertexCount,0,1);
        ++stats_.staticWorldDrawBatches;
    }
}

bool Renderer::meshVisible(const MeshBounds& bounds, const Transform& transform) const {
    if (!cameraValid_ || !bounds.valid) return true;
    const Vec3 center = transformPoint(transform, bounds.center);
    const float maxScale = std::max({
        std::fabs(transform.scale.x),
        std::fabs(transform.scale.y),
        std::fabs(transform.scale.z)
    });
    return sphereVisible(currentCamera_, center, bounds.radius * maxScale);
}

void Renderer::pushTri3D(const Vertex3D& a, const Vertex3D& b, const Vertex3D& c) {
    if (batchWouldOverflow(sceneVertices_.size(), 3, kMaxVertices3D)) {
        flush3DBatch();
    }
    if (batchWouldOverflow(sceneVertices_.size(), 3, kMaxVertices3D)) {
        WHBLogPrintf("ArenaAssault: 3D triangle exceeds batch capacity");
        return;
    }
    sceneVertices_.push_back(a);
    sceneVertices_.push_back(b);
    sceneVertices_.push_back(c);
    ++stats_.submittedTriangles;
}

void Renderer::submitBox(const Vec3& center, const Vec3& half, float yaw,
                         const Material& material) {
    const Vec3 local[8] = {
        {-half.x,-half.y,-half.z}, { half.x,-half.y,-half.z},
        { half.x, half.y,-half.z}, {-half.x, half.y,-half.z},
        {-half.x,-half.y, half.z}, { half.x,-half.y, half.z},
        { half.x, half.y, half.z}, {-half.x, half.y, half.z}
    };
    const int faces[6][4] = {
        {0,1,2,3}, {5,4,7,6}, {4,0,3,7},
        {1,5,6,2}, {3,2,6,7}, {4,5,1,0}
    };
    const Vec3 normals[6] = {
        {0,0,-1}, {0,0,1}, {-1,0,0},
        {1,0,0}, {0,1,0}, {0,-1,0}
    };
    const Vec2 uv[4] = {{0,0},{1,0},{1,1},{0,1}};

    Vec3 world[8];
    for (int i=0;i<8;++i) world[i] = center + rotateY(local[i], yaw);

    for (int f=0;f<6;++f) {
        const Vec3 n = rotateY(normals[f], yaw);
        const int a=faces[f][0], b=faces[f][1], c=faces[f][2], d=faces[f][3];
        const Vertex3D va=makePreparedVertex(world[a],n,uv[0],material);
        const Vertex3D vb=makePreparedVertex(world[b],n,uv[1],material);
        const Vertex3D vc=makePreparedVertex(world[c],n,uv[2],material);
        const Vertex3D vd=makePreparedVertex(world[d],n,uv[3],material);
        pushTri3D(va,vb,vc);
        pushTri3D(va,vc,vd);
    }
}

void Renderer::submitBillboardQuad(const Vec3& center, float halfSize,
                                   const Material& material) {
    halfSize=std::max(0.0001f,halfSize);
    const Vec3 right=currentCameraRight_*halfSize;
    const Vec3 up=currentCameraUp_*halfSize;
    const Vec3 n=currentCameraForward_*-1.0f;
    const Vec3 a=center-right-up;
    const Vec3 b=center+right-up;
    const Vec3 c=center+right+up;
    const Vec3 d=center-right+up;
    const Vertex3D va=makePreparedVertex(a,n,{0,1},material);
    const Vertex3D vb=makePreparedVertex(b,n,{1,1},material);
    const Vertex3D vc=makePreparedVertex(c,n,{1,0},material);
    const Vertex3D vd=makePreparedVertex(d,n,{0,0},material);
    pushTri3D(va,vb,vc);
    pushTri3D(va,vc,vd);
    ++stats_.activeParticles;
    stats_.particleTriangles+=2;
}

void Renderer::submitMesh(const Mesh& mesh, const Transform& transform,
                          const Material& material) {
    if (!mesh.valid()) return;
    if (!meshVisible(mesh.bounds(), transform)) {
        ++stats_.culledMeshes;
        return;
    }

    const auto& vertices = mesh.vertices();
    const auto& indices = mesh.indices();
    for (std::size_t i=0;i+2<indices.size();i+=3) {
        const MeshVertex& a = vertices[indices[i+0]];
        const MeshVertex& b = vertices[indices[i+1]];
        const MeshVertex& c = vertices[indices[i+2]];
        pushTri3D(
            makePreparedVertex(transformPoint(transform,a.position), transformNormal(transform,a.normal), a.uv, material),
            makePreparedVertex(transformPoint(transform,b.position), transformNormal(transform,b.normal), b.uv, material),
            makePreparedVertex(transformPoint(transform,c.position), transformNormal(transform,c.normal), c.uv, material)
        );
    }
}

void Renderer::submitSkinnedMesh(const SkinnedMesh& mesh, const SkeletonPose& pose,
                                 const Transform& transform, const Material& material) {
    if (!mesh.valid() || pose.count == 0) return;

    const std::size_t slot=actorSubmitCursor_++ % kActorCacheSlots;
    MeshBounds animatedBounds=mesh.bounds();
    animatedBounds.radius*=1.20f;

    const float maxScale=std::max({
        std::fabs(transform.scale.x),std::fabs(transform.scale.y),std::fabs(transform.scale.z)
    });
    const Vec3 worldCenter=transformPoint(transform,animatedBounds.center);
    const float worldRadius=animatedBounds.radius*maxScale;
    const bool visible=!cameraValid_ || sphereVisible(currentCamera_,worldCenter,worldRadius);
    const float distance=cameraValid_ ? length(worldCenter-currentCamera_.pos) : 0.0f;
    const EnemyRenderTier tier=selectEnemyRenderTier(distance,visible);

    if (tier==EnemyRenderTier::Culled) {
        ++stats_.culledMeshes;
        ++stats_.culledAam2Actors;
        return;
    }
    if (tier==EnemyRenderTier::Low) {
        ++stats_.lowDetailActors;
        if (enemyLowDetailMesh_.valid()) submitMesh(enemyLowDetailMesh_,transform,material);
        return;
    }

    ++stats_.visibleAam2Actors;
    ActorSkinCache& cache=actorSkinCache_[slot];
    const bool cacheShapeMismatch=cache.localVertices.size()!=mesh.vertices().size();
    const bool actorChanged=!cache.valid ||
                            lengthSq(transform.position-cache.lastWorldPosition)>4.0f;
    const bool refresh=tier==EnemyRenderTier::High || cacheShapeMismatch || actorChanged ||
                       shouldRefreshMediumPose(frameIndex_,slot);

    if (refresh) {
        const SkinningStats skinStats=skinUniqueVertices(mesh.vertices(),pose,cache.localVertices);
        stats_.skinnedVertices+=static_cast<std::uint32_t>(skinStats.evaluatedVertices);
        cache.valid=true;
        cache.lastWorldPosition=transform.position;
    }

    cache.worldVertices.resize(cache.localVertices.size());
    const float cy=std::cos(transform.yaw);
    const float sy=std::sin(transform.yaw);
    const float invX=std::fabs(transform.scale.x)>0.00001f ? 1.0f/transform.scale.x : 1.0f;
    const float invY=std::fabs(transform.scale.y)>0.00001f ? 1.0f/transform.scale.y : 1.0f;
    const float invZ=std::fabs(transform.scale.z)>0.00001f ? 1.0f/transform.scale.z : 1.0f;

    for (std::size_t i=0;i<cache.localVertices.size();++i) {
        const SkinnedVertexOutput& local=cache.localVertices[i];
        const float sx=local.position.x*transform.scale.x;
        const float py=local.position.y*transform.scale.y;
        const float sz=local.position.z*transform.scale.z;
        const Vec3 p{
            transform.position.x + sx*cy - sz*sy,
            transform.position.y + py,
            transform.position.z + sx*sy + sz*cy
        };
        const Vec3 ns{local.normal.x*invX,local.normal.y*invY,local.normal.z*invZ};
        const Vec3 n=normalize({ns.x*cy-ns.z*sy,ns.y,ns.x*sy+ns.z*cy});
        cache.worldVertices[i]={p,n,local.uv};
    }

    const auto& indices=mesh.indices();
    for (std::size_t i=0;i+2<indices.size();i+=3) {
        const auto& a=cache.worldVertices[indices[i+0]];
        const auto& b=cache.worldVertices[indices[i+1]];
        const auto& c=cache.worldVertices[indices[i+2]];
        pushTri3D(
            makePreparedVertex(a.position,a.normal,a.uv,material),
            makePreparedVertex(b.position,b.normal,b.uv,material),
            makePreparedVertex(c.position,c.normal,c.uv,material)
        );
    }
    stats_.actorTriangles+=static_cast<std::uint32_t>(indices.size()/3);
}

void Renderer::flush3DBatch() {
    if (sceneVertices_.empty()) return;
    void* dst = GX2RLockBufferEx(&sceneVertexBuffer_, 0);
    if (!dst) {
        WHBLogPrintf("ArenaAssault: failed to lock 3D batch buffer");
        sceneVertices_.clear();
        return;
    }
    std::memcpy(dst, sceneVertices_.data(), sceneVertices_.size()*sizeof(Vertex3D));
    GX2RUnlockBufferEx(&sceneVertexBuffer_, 0);

    bindScenePipeline();
    GX2RSetAttributeBuffer(&sceneVertexBuffer_, 0, sizeof(Vertex3D), 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES,
              static_cast<std::uint32_t>(sceneVertices_.size()),0,1);
    ++stats_.batches3D;
    sceneVertices_.clear();
}

void Renderer::flush3D() {
    flush3DBatch();
    ++frameIndex_;
}

void Renderer::begin2D() {
    uiVertices_.clear();
}

void Renderer::tri2D(const Vec4& a, const Vec4& b, const Vec4& c,
                     const Color& color) {
    if (uiVertices_.size()+3 > kMaxVertices2D) return;
    uiVertices_.push_back({a,color});
    uiVertices_.push_back({b,color});
    uiVertices_.push_back({c,color});
}

void Renderer::rect2D(float x0, float y0, float x1, float y1,
                      const Color& color, float z) {
    const Vec4 a{x0,y0,z,1}, b{x1,y0,z,1}, c{x1,y1,z,1}, d{x0,y1,z,1};
    tri2D(a,b,c,color);
    tri2D(a,c,d,color);
}

void Renderer::flush2D() {
    if (uiVertices_.empty()) return;
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    void* dst = GX2RLockBufferEx(&uiVertexBuffer_, 0);
    if (!dst) return;
    std::memcpy(dst, uiVertices_.data(), uiVertices_.size()*sizeof(Vertex2D));
    GX2RUnlockBufferEx(&uiVertexBuffer_, 0);

    GX2SetFetchShader(&uiGroup_.fetchShader);
    GX2SetVertexShader(uiGroup_.vertexShader);
    GX2SetPixelShader(uiGroup_.pixelShader);
    GX2RSetAttributeBuffer(&uiVertexBuffer_, 0, sizeof(Vertex2D), 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES,
              static_cast<std::uint32_t>(uiVertices_.size()),0,1);
}

void Renderer::uploadSceneUniforms(const Camera& camera) {
    float host[48]{};
    const Mat4 vp = cameraViewProjection(camera);
    for (int i=0;i<16;++i) host[i] = vp.m[i];

    int o = 16;
    auto put4 = [&](float x,float y,float z,float w) {
        host[o++]=x; host[o++]=y; host[o++]=z; host[o++]=w;
    };

    put4(camera.pos.x,camera.pos.y,camera.pos.z,1.0f);
    put4(0.45f,-0.85f,0.35f,0.24f);
    put4(0.025f,0.040f,0.065f,0.028f);
    put4(-8.0f,3.4f,-8.0f,11.0f);
    put4(0.06f,0.62f,0.95f,0.75f);
    put4(8.0f,3.0f,8.0f,10.0f);
    put4(1.00f,0.28f,0.06f,0.46f);
    put4(1.0f,0.0f,0.0f,0.0f);

    for (int i=0;i<48;++i) sceneUniformBlock_[i] = _swapF32(host[i]);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK,
                  sceneUniformBlock_, sizeof(sceneUniformBlock_));
}

} // namespace aa
