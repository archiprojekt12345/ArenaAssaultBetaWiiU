#pragma once

#include <gx2/sampler.h>
#include <gx2/texture.h>

namespace aa {

class TextureAtlas {
public:
    TextureAtlas() = default;
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    bool init(const char* tgaPath);
    void shutdown();

    GX2Texture* texture() const { return texture_; }
    GX2Sampler* sampler() { return &sampler_; }
    bool loadedExternal() const { return loadedExternal_; }

private:
    bool loadTga32(const char* path);
    bool createFallbackWhite();

    GX2Texture* texture_{nullptr};
    GX2Sampler sampler_{};
    bool loadedExternal_{false};
};

} // namespace aa
