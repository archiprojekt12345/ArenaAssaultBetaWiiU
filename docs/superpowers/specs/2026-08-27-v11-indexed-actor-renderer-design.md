# Arena Assault V11 — indexed actor renderer rebuild

## Context and hardware evidence

The current V11 UI fix is correct, but real Wii U testing shows a remaining performance cliff: the game is smooth until a detailed AAM2 Trooper enters the player camera, then frame rate drops sharply. This means the remaining hot path is the visible AAM2 actor path, not the TV/DRC UI buffers or static corridor cache.

The current renderer already skins each unique AAM2 source vertex once, but then it still:

1. transforms every cached vertex into world space on the CPU every frame;
2. walks the full index list and expands each triangle into three `Vertex3D` copies;
3. appends those 16,488 expanded vertices for one 5,496-triangle Trooper into the generic dynamic scene batch;
4. uploads and draws that expanded stream through `sceneVertexBuffer_`.

For the real V11 Trooper this turns 2,571 unique vertices into 16,488 submitted vertices per visible actor. The visible-on-screen transition therefore still creates a large CPU and memory-upload burst.

## Chosen architecture

Use a dedicated indexed actor rendering path for AAM2 enemies.

### 1. Shared static index buffer

When the AAM2 mesh is loaded, create one persistent `GX2RBuffer` with `GX2R_RESOURCE_BIND_INDEX_BUFFER | GX2R_RESOURCE_USAGE_GPU_READ` and upload the mesh's `uint32_t` index list once.

The real Trooper uses 16,488 indices, so the index buffer is about 66 KiB and is shared by every AAM2 enemy instance.

Drawing uses `GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, indexCount, GX2_INDEX_TYPE_U32, indexBuffer.buffer, 0, 1)`.

The generic dynamic triangle batch is no longer used for the detailed AAM2 body.

### 2. Persistent actor vertex buffers

Each actor cache slot gets its own persistent GX2R vertex buffer sized for the unique AAM2 vertex count. This avoids the same GPU hazard that previously affected the UI: one actor must never overwrite vertex data that a prior actor draw may still be consuming.

`kActorCacheSlots` remains 16, matching the gameplay enemy ceiling.

The buffer stores one render vertex per unique skinned source vertex. The CPU no longer expands triangle indices to duplicate render vertices.

### 3. Local-space skinned cache + GPU model transform

The actor vertex buffer stores local-space skinned positions/normals. Player-relative actor movement, yaw and scale are moved to the actor vertex shader through a per-draw model transform uniform.

This removes the current per-frame CPU loop that converts all 2,571 cached local vertices to world space even when a Medium-tier pose is being reused.

A dedicated actor shader pipeline is used rather than changing the static-world/box scene shader semantics:

- `shaders/actor3d.vert`
- reuse the existing scene lighting fragment shader behavior through `actor3d.frag` or an equivalent compiled pair;
- `content/shaders/actor3d.gsh` is compiled and embedded in WUHB.

The actor vertex shader computes world position/normal and view-projection position from the local skinned vertex plus model matrix. Lighting remains visually compatible with the existing scene pipeline.

### 4. Pose update cadence

Culling happens before any AAM2 work.

Initial hardware-safe tiers:

- **High:** visible and nearer than 7 m — refresh skin pose every frame.
- **Medium:** visible from 7 m to 14 m — refresh local skin cache every 3 frames, staggered by actor slot.
- **Low:** visible at 14 m or farther — use existing lightweight AAM1 fallback.
- **Culled:** outside frustum — no AAM2 skinning and no actor buffer upload/draw.

The thresholds are intentionally more conservative than the current 10/22 m policy because hardware testing showed a visible AAM2 actor is still expensive.

### 5. Upload policy

A High/Medium actor buffer is uploaded only when its local skinned pose is refreshed or its material-dependent render vertex data changed.

Movement/yaw/scale alone do not force a vertex-buffer rewrite because they are applied by the actor model matrix on the GPU.

No heap allocation is permitted in the steady-state actor render path.

### 6. Material handling

For the first indexed pass, preserve the existing `Vertex3D` material fields to minimize pixel-shader risk. Diffuse/emissive/surface values are filled when the actor buffer is refreshed.

A later optimization may move actor material values to uniforms and shrink the actor vertex stride, but that is not required for this fix.

### 7. Draw ordering and generic scene batches

Before issuing a direct indexed AAM2 draw, flush any pending generic 3D batch so ordering is deterministic. Bind the actor pipeline, actor vertex buffer and shared actor index buffer, draw, then allow generic scene submission to continue.

The actor path must not overwrite `sceneVertexBuffer_` and must not depend on it.

### 8. Fallback behavior

If creation of the shared actor index buffer or any actor vertex buffer fails, the renderer falls back to AAM1 for that actor rather than returning to the expensive triangle-expansion AAM2 path.

This makes the failure mode safe for Wii U performance.

## Code changes

Expected files:

- `include/renderer.hpp`
  - actor GPU cache structure;
  - persistent per-slot vertex buffers;
  - shared actor index buffer;
  - actor shader group and uniform block;
  - indexed actor stats.
- `src/renderer.cpp`
  - actor pipeline init/shutdown;
  - index-buffer upload;
  - per-slot vertex-buffer creation/update;
  - direct `GX2DrawIndexedEx` path;
  - removal of AAM2 triangle expansion into `sceneVertices_`;
  - GPU model-transform upload.
- `include/render_policy.hpp`
  - 7 m / 14 m thresholds;
  - Medium refresh every 3 frames.
- `shaders/actor3d.vert`, `shaders/actor3d.frag`
  - local-space actor transform and existing lighting outputs.
- `tools/compile_shaders.sh`
  - compile `actor3d.gsh`.
- `CMakeLists.txt`
  - require/package `actor3d.gsh`.
- `.github/workflows/build-v11-core.yml`
  - verify actor shader and run new host-side tests.
- `tests/`
  - actor indexed-path policy/structure tests.

No merge to `main` is part of this work.

## TDD and verification

Before production code, add tests that fail against the current branch and prove the intended architecture:

1. the detailed actor path does not expand `indexCount` into the generic dynamic scene stream;
2. one source vertex maps to one actor GPU vertex;
3. Medium cadence refreshes once every 3 frames and is staggered by actor slot;
4. Low and Culled tiers perform zero detailed AAM2 buffer uploads;
5. the renderer source contains a dedicated index buffer and `GX2DrawIndexedEx` actor draw;
6. actor cache slots own separate GPU vertex buffers;
7. WUHB build requires and embeds `actor3d.gsh`.

CI must then pass:

- all existing host tests;
- the new actor-renderer tests;
- CafeGLSL compilation;
- WUT configure/build;
- RPX conversion;
- WUHB creation;
- Aroma ZIP verification.

## Hardware acceptance test

A hardware-test WUHB will be assembled with the full verified V11 assets and tested on Wii U.

Success criteria:

- entering view of one detailed Trooper no longer causes the obvious frame-rate collapse;
- two or more nearby visible Troopers remain substantially smoother than the current UIFIX build;
- distant Troopers switch to the lightweight representation without destabilizing gameplay;
- TV remains 3D + HUD and GamePad remains independent map;
- hit/death effects do not reintroduce long stalls;
- corridor/crates remain present.

Final LOD distances may be tuned only after this indexed path is verified on hardware.

## Non-goals

- full GPU skeletal skinning with bone matrices/weights in the vertex shader;
- changing the AAM2 file format;
- mesh decimation of the source Trooper in this pass;
- merging the feature branch to `main` before hardware approval.
