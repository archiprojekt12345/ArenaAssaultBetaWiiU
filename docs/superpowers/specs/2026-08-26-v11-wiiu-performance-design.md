# Arena Assault V11 — Wii U performance architecture

## Goal

Keep the V11 visual assets (corridor, crates and AAM2 enemy) while restoring smooth gameplay on real Wii U hardware and removing short hit/death frame spikes.

Hardware success target:
- six enemies visible at mission start: at least 30 FPS, with 60 FPS as the stretch target;
- no multi-frame stalls when a projectile hits or kills an enemy;
- corridor and crates remain visible and do not consume per-frame CPU time proportional to triangle count;
- off-screen actors/world groups are rejected before expensive work.

## Root causes already demonstrated

1. The current AAM2 path skins vertices inside the indexed triangle loop. Shared vertices are therefore skinned repeatedly. The 5,496-triangle enemy can trigger roughly 16k vertex skin evaluations per enemy per frame instead of 2,571 unique-vertex evaluations.
2. `applyBonePoint` / `applyBoneNormal` execute Euler rotations for every weight of every evaluated vertex, so trigonometry is repeated far too often.
3. Static world meshes are transformed and expanded into the dynamic CPU triangle stream every frame.
4. Impact/death particles are rendered as boxes. A hit/death burst causes a sudden CPU geometry burst, which matches the observed short stutter.
5. Frustum culling exists for mesh submissions on the V11 branch, but it happens too late to solve the repeated-skinning and static-world costs by itself.

## Architecture

### 1. Actor skin cache + indexed dynamic draw

For each visible AAM2 enemy, compute its pose once, precompute one affine transform per active bone, then skin each unique source vertex exactly once into a temporary/cache vertex array.

The renderer will draw the skinned unique-vertex array with the existing mesh index list rather than expanding every indexed triangle into three duplicate vertices. One shared/static index buffer may be reused by all instances of the same AAM2 mesh.

If the indexed GX2 path is unavailable or unstable on hardware, the fallback is still to skin unique vertices once and only then expand indices into the existing triangle batch. This preserves the largest CPU win even before the indexed draw path is enabled.

### 2. Enemy distance policy

Apply culling before skinning. Use three distance tiers:
- High: full AAM2, full animation.
- Medium: same AAM2 geometry initially, but animation pose may update at half rate and reuse the last skinned cache on alternating frames.
- Low: use the lightweight existing AAM1/procedural representation instead of AAM2.

This avoids requiring new binary LOD assets for the first performance pass. Dedicated simplified AAM2 LOD files can be added later if the hardware measurements justify them.

### 3. Static world cache

Corridor layers and crate geometry are static. Build their world-transformed `Vertex3D` data once during renderer initialization, upload it to dedicated GX2R vertex buffers, and draw those buffers directly every frame.

Use coarse world-group bounds (one per corridor portal, one per crate placement or crate cluster) so an entire static group can be skipped when outside the camera frustum. Do not re-run `transformPoint`, `transformNormal`, material expansion, or triangle expansion for static world assets every frame.

This first version intentionally keeps the existing shader/material vertex format to minimize shader risk. A future fully indexed static world path can reduce memory further, but it is not required for the first hardware-performance fix.

### 4. Cheap hit/death effects

Replace box particles in the 3D particle renderer with camera-facing quads (two triangles each) or a minimal crossed-quad fallback. Reduce burst counts only if hardware timing still spikes after the geometry change.

No heap allocation is introduced during hit/death events. Particle storage remains preallocated/ring-based.

### 5. Audio spike isolation

Keep prebuilt PCM samples. Avoid unnecessary voice setup work when possible and add timing counters around hit/death audio calls. Audio is treated as a secondary suspected source; geometry is fixed first so only one major variable changes at a time.

### 6. Performance instrumentation

Extend render/runtime stats with counters for:
- visible/culled AAM2 actors;
- unique vertices skinned;
- actor triangles/indices submitted;
- static-world draw batches;
- active particle count and particle triangles;
- 3D dynamic batches.

Add optional low-overhead frame timing/log output suitable for a hardware diagnostic build. It must be possible to disable verbose logging for normal release builds.

## Implementation boundaries

Expected source changes:
- `include/renderer.hpp`, `src/renderer.cpp` — skin cache, indexed/fallback actor path, static world buffers, particle-quad helper, stats.
- `include/animation.hpp` — precomputed bone transform representation/helpers.
- `src/game.cpp` — enemy LOD/culling policy before expensive skin submission; pose-cache cadence.
- `src/particle.cpp` / `include/particle.hpp` — quad particle submission and instrumentation.
- `include/render_policy.hpp` — explicit enemy LOD thresholds/cadence policy.
- tests under `tests/` — policy, unique-skinning accounting and static-cache behavior that can be verified host-side.

No merge to `main` is part of this work. Changes stay on `feature/v11-renderer-optimization` and PR #1 until hardware verification is complete.

## Verification

Host-side:
- existing render policy/world layout tests continue to pass;
- new tests prove an indexed AAM2 mesh skins no more than `vertexCount` source vertices per actor update;
- LOD/culling tests prove off-screen and low-tier actors skip AAM2 work;
- static-world cache build counts are deterministic and do not increase per frame;
- asset binary validation remains green.

Wii U hardware:
1. start mission with six enemies and compare FPS/frame pacing to the smooth no-AAM2 diagnostic build;
2. repeatedly shoot Scout/Soldier/Heavy and kill each class while watching for stutters;
3. stand where both corridor portals/crates are visible, then turn away to verify culling and frame pacing;
4. run defend phase near the 10-enemy cap;
5. only after these checks package the final V11 WUHB.

## Non-goals for this pass

- new gameplay modes/checkpoint system;
- texture/material overhaul;
- fully GPU-based skeletal animation;
- aggressive mesh decimation that changes the source assets before we have hardware timing data.
