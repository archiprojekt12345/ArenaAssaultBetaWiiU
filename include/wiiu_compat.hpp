#pragma once

#include <cstdint>
#include <cstring>

#include <gx2r/buffer.h>

namespace aa {

// Current WUT exposes GX2RResourceFlags as a strongly typed enum. Older Arena
// Assault code passed literal 0 to the Ex helpers, so keep the call sites
// readable while converting that legacy argument explicitly at the boundary.
inline void GX2RDestroyBufferEx(GX2RBuffer* buffer, int flags) {
    ::GX2RDestroyBufferEx(buffer, static_cast<GX2RResourceFlags>(flags));
}

inline void* GX2RLockBufferEx(GX2RBuffer* buffer, int flags) {
    return ::GX2RLockBufferEx(buffer, static_cast<GX2RResourceFlags>(flags));
}

inline void GX2RUnlockBufferEx(GX2RBuffer* buffer, int flags) {
    ::GX2RUnlockBufferEx(buffer, static_cast<GX2RResourceFlags>(flags));
}

// CafeGLSL uniform blocks are consumed in little-endian byte order while the
// Wii U CPU is big-endian. Preserve the float's bits, byte-swap them, then put
// those bits back into a float slot without aliasing UB.
inline float _swapF32(float value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    bits = __builtin_bswap32(bits);
    float swapped{};
    std::memcpy(&swapped, &bits, sizeof(swapped));
    return swapped;
}

} // namespace aa
