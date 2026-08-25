#include "texture.hpp"
#include <cstdint>

#include <cstdio>
#include <cstring>
#include <vector>

#include <coreinit/memdefaultheap.h>
#include <gx2/mem.h>
#include <gx2/surface.h>
#include <whb/log.h>

namespace aa {
namespace {

struct TgaHeader {
    std::uint8_t idSize;
    std::uint8_t colorMapType;
    std::uint8_t imageType;
    std::uint8_t colorMapStart[2];
    std::uint8_t colorMapLength[2];
    std::uint8_t colorMapBits;
    std::uint16_t xStart;
    std::uint16_t yStart;
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t bits;
    std::uint8_t descriptor;
};
static_assert(sizeof(TgaHeader) == 18, "TGA header layout changed");

void destroyTexture(GX2Texture*& texture) {
    if (!texture) return;
    if (texture->surface.image) MEMFreeToDefaultHeap(texture->surface.image);
    MEMFreeToDefaultHeap(texture);
    texture = nullptr;
}

GX2Texture* allocateTexture(std::uint32_t width, std::uint32_t height) {
    auto* texture = static_cast<GX2Texture*>(MEMAllocFromDefaultHeap(sizeof(GX2Texture)));
    if (!texture) return nullptr;
    *texture = {};

    texture->surface.width = width;
    texture->surface.height = height;
    texture->surface.depth = 1;
    texture->surface.mipLevels = 1;
    texture->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    texture->surface.aa = GX2_AA_MODE1X;
    texture->surface.use = GX2_SURFACE_USE_TEXTURE;
    texture->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    texture->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    texture->surface.swizzle = 0;
    texture->viewFirstMip = 0;
    texture->viewNumMips = 1;
    texture->viewFirstSlice = 0;
    texture->viewNumSlices = 1;
    texture->compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&texture->surface);
    GX2InitTextureRegs(texture);

    if (!texture->surface.imageSize) {
        destroyTexture(texture);
        return nullptr;
    }
    texture->surface.image = MEMAllocFromDefaultHeapEx(texture->surface.imageSize,
                                                       texture->surface.alignment);
    if (!texture->surface.image) {
        destroyTexture(texture);
        return nullptr;
    }
    std::memset(texture->surface.image, 0xFF, texture->surface.imageSize);
    return texture;
}

} // namespace

TextureAtlas::~TextureAtlas() {
    shutdown();
}

bool TextureAtlas::init(const char* tgaPath) {
    shutdown();
    GX2InitSampler(&sampler_, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    if (tgaPath && *tgaPath && loadTga32(tgaPath)) {
        loadedExternal_ = true;
        return true;
    }
    loadedExternal_ = false;
    return createFallbackWhite();
}

void TextureAtlas::shutdown() {
    destroyTexture(texture_);
    loadedExternal_ = false;
}

bool TextureAtlas::createFallbackWhite() {
    texture_ = allocateTexture(1,1);
    if (!texture_) return false;
    auto* out = static_cast<std::uint32_t*>(texture_->surface.image);
    out[0] = 0xFFFFFFFFu;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE,
                  texture_->surface.image, texture_->surface.imageSize);
    return true;
}

bool TextureAtlas::loadTga32(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f,0,SEEK_END);
    const long len = std::ftell(f);
    std::fseek(f,0,SEEK_SET);
    if (len < 18) { std::fclose(f); return false; }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(len));
    if (std::fread(data.data(),1,data.size(),f) != data.size()) {
        std::fclose(f); return false;
    }
    std::fclose(f);

    const TgaHeader* h = reinterpret_cast<const TgaHeader*>(data.data());
    const std::uint32_t width = std::uint32_t(data[12]) | (std::uint32_t(data[13]) << 8);
    const std::uint32_t height = std::uint32_t(data[14]) | (std::uint32_t(data[15]) << 8);
    if (!width || !height || h->bits != 32 || h->colorMapType != 0 || h->imageType != 2) {
        WHBLogPrintf("ArenaAssault: atlas TGA must be uncompressed 32-bit RGBA/BGRA");
        return false;
    }

    const std::size_t pixelOffset = 18u + h->idSize;
    const std::size_t required = pixelOffset + std::size_t(width)*height*4u;
    if (required > data.size()) return false;

    texture_ = allocateTexture(width,height);
    if (!texture_) return false;

    const bool topOrigin = (h->descriptor & 0x20u) != 0;
    for (std::uint32_t y=0; y<height; ++y) {
        const std::uint32_t srcY = topOrigin ? y : (height-y-1);
        const std::uint8_t* src = data.data() + pixelOffset + std::size_t(srcY)*width*4u;
        auto* dst = static_cast<std::uint32_t*>(texture_->surface.image) +
                    std::size_t(y)*texture_->surface.pitch;
        for (std::uint32_t x=0; x<width; ++x) {
            const std::uint8_t b = src[x*4+0];
            const std::uint8_t g = src[x*4+1];
            const std::uint8_t r = src[x*4+2];
            const std::uint8_t a = src[x*4+3];
            dst[x] = (std::uint32_t(r) << 24) | (std::uint32_t(g) << 16) |
                     (std::uint32_t(b) << 8) | std::uint32_t(a);
        }
    }

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE,
                  texture_->surface.image, texture_->surface.imageSize);
    return true;
}

} // namespace aa
