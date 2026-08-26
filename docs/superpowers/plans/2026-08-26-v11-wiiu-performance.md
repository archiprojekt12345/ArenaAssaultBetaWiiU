# V11 Wii U Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the V11 corridor, crates and AAM2 enemy while restoring smooth Wii U gameplay and removing hit/death frame spikes.

**Architecture:** Move expensive reusable work out of the per-triangle/per-frame path. Skin each AAM2 source vertex once using precomputed bone affine transforms, reject/LOD enemies before skinning, cache static world geometry in a dedicated GX2R buffer, and render particles as billboards rather than cubes. Preserve the current shader format and keep an AAM1 low-detail fallback so the first hardware pass minimizes GX2 risk.

**Tech Stack:** C++17, devkitPro/WUT, GX2/GX2R, GitHub Actions, host-side g++ tests.

**Spec:** `docs/superpowers/specs/2026-08-26-v11-wiiu-performance-design.md`

## Global Constraints

- Stay on `feature/v11-renderer-optimization`; do not merge to `main` before hardware verification.
- Keep AAM1/AAM2 binary formats unchanged.
- Keep offline `.gsh` shaders; no runtime CafeGLSL dependency.
- First hardware target: six visible enemies at >=30 FPS, 60 FPS stretch target.
- Do not aggressively decimate source assets in this pass.

---

### Task 1: Host-testable actor policy and unique skinning helpers

**Files:**
- Modify: `include/render_policy.hpp`
- Modify: `include/animation.hpp`
- Create: `include/skinning.hpp`
- Modify: `tests/render_policy_tests.cpp`
- Create: `tests/skinning_tests.cpp`

**Interfaces:**
- Produces: `EnemyRenderTier`, `selectEnemyRenderTier(distance, visible)`, `shouldRefreshMediumPose(frame, actorIndex)`, `BoneAffine`, `buildBoneAffines(const SkeletonPose&)`, `skinUniqueVertices(...)`.

- [ ] **Step 1: Write failing policy tests** for culled/high/medium/low tiers and alternating medium refresh cadence.
- [ ] **Step 2: Write failing skinning test** with four source vertices and six indices; assert the result has four skinned vertices and `evaluatedVertices == 4`, not six.
- [ ] **Step 3: Run host tests before implementation** and confirm the new names are missing.
- [ ] **Step 4: Implement policy and affine helpers** so Euler trig is performed once per bone when affines are built, not per weighted vertex.
- [ ] **Step 5: Implement unique-vertex skinning** into a reusable output vector; weights are normalized exactly as the loaded mesh supplies them.
- [ ] **Step 6: Run** `g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude tests/render_policy_tests.cpp -o /tmp/rp && /tmp/rp` and `g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude tests/skinning_tests.cpp -o /tmp/skin && /tmp/skin` and confirm PASS.

### Task 2: Actor cache, early culling and low-detail fallback

**Files:**
- Modify: `include/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `include/game.hpp`
- Modify: `src/game.cpp`

**Interfaces:**
- Consumes: Task 1 helpers.
- Produces: per-enemy skinned cache slots; renderer stats `visibleAam2Actors`, `culledAam2Actors`, `skinnedVertices`, `actorTriangles`.

- [ ] **Step 1: Add cache accounting assertions** through host-testable helper behavior before changing the renderer.
- [ ] **Step 2: Change the AAM2 renderer path** to skin the mesh vertex array once into a cache slot, then expand indices from that cached result into the existing safe dynamic GX2 batch. Do not skin inside the index loop.
- [ ] **Step 3: Add medium-tier cache reuse**: high refreshes each frame; medium refreshes on alternating frames per actor; low never enters AAM2 skinning.
- [ ] **Step 4: In `renderTV` determine visibility and tier before `renderEnemy`** using the current camera and a conservative enemy sphere. Culled actors do no render work. Low-tier actors use `enemy_body.aam` when available, otherwise procedural geometry.
- [ ] **Step 5: Preserve Scout/Heavy/elite visual add-ons** and visor/muzzle effects while making the base body tier-dependent.
- [ ] **Step 6: Re-run all host tests.**

### Task 3: Static world GPU cache

**Files:**
- Modify: `include/renderer.hpp`
- Modify: `src/renderer.cpp`

**Interfaces:**
- Produces: one dedicated static-world `GX2RBuffer`, a list of `StaticWorldRange { firstVertex, vertexCount, bounds }`, and stats `staticWorldBuilds`, `staticWorldDrawBatches`.

- [ ] **Step 1: Add a host-testable range-count helper or deterministic assertion** proving two corridor groups plus eight crate groups are built exactly once.
- [ ] **Step 2: During renderer initialization**, transform corridor layers, hazard stripes, crate meshes and crate panels into a temporary `Vertex3D` array once.
- [ ] **Step 3: Upload the complete static array once** into a dedicated GX2R vertex buffer and release the CPU temporary after upload.
- [ ] **Step 4: At `begin3D`, cull each static group by a conservative sphere and draw only visible ranges directly from the static buffer. Do not call `submitMesh` for world assets every frame.
- [ ] **Step 5: Destroy the static buffer during shutdown and keep missing optional assets non-fatal.**

### Task 4: Cheap particles and hit/death spike reduction

**Files:**
- Modify: `include/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `src/particle.cpp`

**Interfaces:**
- Produces: `submitBillboardQuad(center, halfSize, material)` and stats `activeParticles`, `particleTriangles`.

- [ ] **Step 1: Add a host-level geometry-count invariant**: one particle must submit two triangles rather than twelve.
- [ ] **Step 2: Implement camera-facing billboard quad submission** using the current camera right/up vectors.
- [ ] **Step 3: Replace particle `submitBox` calls with billboard submission** while keeping the existing preallocated particle ring and burst counts unchanged for the first hardware comparison.
- [ ] **Step 4: Re-run host tests.**

### Task 5: CI verification and Wii U artifact

**Files:**
- Modify: `.github/workflows/build-v11-core.yml` only if host tests are not already executed by CI.

**Interfaces:**
- Produces: `ArenaAssault-V11-Aroma-SD` artifact containing `ArenaAssault.wuhb` and `ArenaAssault_Aroma_SD_V11.zip`.

- [ ] **Step 1: Add host test commands before the cross-compile** for render policy, skinning and world-layout tests.
- [ ] **Step 2: Run asset validation** before configuring the Wii U build.
- [ ] **Step 3: Let the feature-branch push trigger GitHub Actions.**
- [ ] **Step 4: Inspect the full job log; fix compile/link/package failures without merging to main.**
- [ ] **Step 5: On success download the artifact and verify the ZIP contains `wiiu/apps/ArenaAssault/ArenaAssault.wuhb`.**
- [ ] **Step 6: Hardware test against the smooth no-AAM2 diagnostic build: six-enemy start, repeated Scout/Soldier/Heavy hits and kills, corridor-facing/away views, and defend phase near ten enemies.**
