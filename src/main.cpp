#include <cstdio>
#include <cstdint>
#include <cstring>

#include <gx2/registers.h>
#include <vpad/input.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#include "game.hpp"
#include "renderer.hpp"

namespace {

bool fileExists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    WHBLogUdpInit();
    WHBProcInit();
    if (!WHBGfxInit()) {
        WHBProcShutdown();
        WHBLogUdpDeinit();
        return -1;
    }

    // Aroma/WUHB: all runtime data is embedded in the bundle and appears
    // read-only at /vol/content.  This is the normal end-user path.
    const char* contentRoot = "/vol/content";
    char sdContentRoot[640]{};
    bool sdMounted = false;

    if (!fileExists("/vol/content/shaders/scene3d.gsh")) {
        // Developer/legacy RPX fallback.  This keeps the same executable useful
        // outside WUHB without making ordinary Aroma users install dependencies.
        sdMounted = WHBMountSdCard();
        if (sdMounted) {
            const char* sd = WHBGetSdCardMountPath();
            std::snprintf(sdContentRoot, sizeof(sdContentRoot),
                          "%s/wiiu/apps/ArenaAssault/content", sd);
            contentRoot = sdContentRoot;
        }
    }

    aa::Renderer renderer;
    if (!renderer.init(contentRoot)) {
        WHBLogPrintf("ArenaAssault: renderer init failed; content root=%s", contentRoot);
        renderer.shutdown();
        if (sdMounted) WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        return -2;
    }

    aa::Game game;
    game.init(contentRoot);

    VPADStatus pad{};
    VPADReadError error = VPAD_READ_SUCCESS;
    constexpr float dt = 1.0f / 60.0f;

    while (WHBProcIsRunning()) {
        const std::int32_t read = VPADRead(VPAD_CHAN_0, &pad, 1, &error);
        if (read <= 0) pad = {};

        game.update(pad, dt);
        WHBGfxBeginRender();

        WHBGfxBeginRenderTV();
        renderer.setUITarget(aa::Renderer::UITarget::TV);
        WHBGfxClearColor(0.025f, 0.035f, 0.055f, 1.0f);
        GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_LESS);
        game.renderTV(renderer);
        WHBGfxFinishRenderTV();

        WHBGfxBeginRenderDRC();
        renderer.setUITarget(aa::Renderer::UITarget::DRC);
        WHBGfxClearColor(0.015f, 0.025f, 0.035f, 1.0f);
        game.renderMap(renderer);
        WHBGfxFinishRenderDRC();

        WHBGfxFinishRender();
    }

    game.shutdown();
    renderer.shutdownActorGpu();
    renderer.shutdown();
    if (sdMounted) WHBUnmountSdCard();
    WHBGfxShutdown();
    WHBProcShutdown();
    WHBLogUdpDeinit();
    return 0;
}
