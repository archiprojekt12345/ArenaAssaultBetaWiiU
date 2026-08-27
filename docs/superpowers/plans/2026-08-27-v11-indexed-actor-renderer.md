# V11 Indexed Actor Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the expensive visible AAM2 triangle-expansion path with a dedicated indexed GPU actor renderer that keeps detailed Troopers smooth on Wii U.

**Architecture:** Each detailed actor cache slot owns a persistent GX2R vertex buffer containing unique local-space skinned vertices. One shared GX2R index buffer contains the AAM2 mesh index list, and a dedicated actor shader applies model/world transforms on the GPU before `GX2DrawIndexedEx`. Low and culled tiers bypass this path entirely.

**Tech Stack:** C++17, devkitPro WUT, GX2/GX2R, CafeGLSL, GitHub Actions, Aroma WUHB.

**Spec:** `docs/superpowers/specs/2026-08-27-v11-indexed-actor-renderer-design.md`

## Global Constraints

- Work only on `feature/v11-renderer-optimization`.
- Do not merge to `main` before Wii U hardware approval.
- Full AAM2: High < 7 m; Medium 7–14 m; Low >= 14 m.
- Medium pose refresh cadence: every 3 frames, staggered by actor slot.
- Culled and Low tiers perform no detailed AAM2 upload/draw.
- Each actor slot must have its own GPU vertex buffer; no shared overwrite hazard.
- The detailed AAM2 body must not expand indices into `sceneVertices_`.
- If indexed actor GPU resources fail, fall back to AAM1 rather than the old expanded AAM2 path.

---

### Task 1: Lock the actor policy with failing tests

**Files:**
- Modify: `tests/render_policy_tests.cpp`
- Create: `tests/indexed_actor_renderer_test.py`
- Modify: `.github/workflows/build-v11-core.yml`

**Interfaces:**
- Consumes: `selectEnemyRenderTier`, `shouldRefreshMediumPose`.
- Produces: regression checks for 7/14 m thresholds, 3-frame cadence, dedicated actor buffers/indexed draw/shader packaging.

- [ ] **Step 1: Write the failing policy assertions**

Add assertions equivalent to:

```cpp
assert(selectEnemyRenderTier(6.9f, true) == EnemyRenderTier::High);
assert(selectEnemyRenderTier(7.0f, true) == EnemyRenderTier::Medium);
assert(selectEnemyRenderTier(13.9f, true) == EnemyRenderTier::Medium);
assert(selectEnemyRenderTier(14.0f, true) == EnemyRenderTier::Low);
assert(shouldRefreshMediumPose(0,0));
assert(!shouldRefreshMediumPose(1,0));
assert(!shouldRefreshMediumPose(2,0));
assert(shouldRefreshMediumPose(3,0));
assert(!shouldRefreshMediumPose(0,1));
assert(!shouldRefreshMediumPose(1,1));
assert(shouldRefreshMediumPose(2,1));
```

- [ ] **Step 2: Add a source-structure regression test**

`tests/indexed_actor_renderer_test.py` must assert that renderer source/header contain:

```python
assert "actorIndexBuffer_" in renderer_h
assert "gpuVertexBuffer" in renderer_h
assert "GX2DrawIndexedEx" in renderer_cpp
assert "actorGroup_" in renderer_h
assert "actor3d.gsh" in renderer_cpp
assert "sceneVertices_[indices" not in renderer_cpp
assert "actor3d.gsh" in cmake
assert "actor3d.gsh" in shader_script
```

Also assert at least 16 actor GPU cache slots are available and each cache owns its own `GX2RBuffer`.

- [ ] **Step 3: Add the Python test to CI and run RED**

Add:

```bash
python3 tests/indexed_actor_renderer_test.py
```

Expected result on the current branch: FAIL because dedicated actor GPU resources/shader do not exist yet.

- [ ] **Step 4: Commit the failing tests**

Commit message:

```text
test: define indexed AAM2 actor renderer contract
```

---

### Task 2: Implement conservative LOD/cadence policy

**Files:**
- Modify: `include/render_policy.hpp`
- Test: `tests/render_policy_tests.cpp`

**Interfaces:**
- Produces: `selectEnemyRenderTier(distance, visible, 7.0f, 14.0f)` defaults and `shouldRefreshMediumPose(frameIndex, actorIndex)` using modulo 3 stagger.

- [ ] **Step 1: Change default thresholds to 7 m and 14 m**

Use:

```cpp
float highToMedium = 7.0f,
float mediumToLow = 14.0f
```

- [ ] **Step 2: Change Medium cadence to one refresh every three frames**

Use:

```cpp
return ((frameIndex + static_cast<std::uint64_t>(actorIndex)) % 3u) == 0u;
```

- [ ] **Step 3: Run host policy tests**

Expected: `render_policy_tests: PASS`.

- [ ] **Step 4: Commit**

Commit message:

```text
perf: tighten AAM2 LOD and medium cadence
```

---

### Task 3: Add the actor shader pipeline and GPU resource ownership

**Files:**
- Create: `shaders/actor3d.vert`
- Create: `shaders/actor3d.frag`
- Modify: `tools/compile_shaders.sh`
- Modify: `CMakeLists.txt`
- Modify: `include/renderer.hpp`
- Modify: `src/renderer.cpp`

**Interfaces:**
- Produces: `actorGroup_`, `actorIndexBuffer_`, one `gpuVertexBuffer` per `ActorSkinCache`, `initActorPipeline()`, `initActorIndexBuffer(const SkinnedMesh&)`, `bindActorPipeline(const Transform&)`.

- [ ] **Step 1: Add `actor3d.vert`**

The vertex shader accepts the same `Vertex3D` layout as scene rendering. It takes a uniform block containing the existing scene values followed by a model matrix. It computes:

```glsl
vec4 world = u_model * vec4(in_position, 1.0);
gl_Position = u_viewProj * world;
v_worldPos = world.xyz;
v_normal = normalize(mat3(u_model) * in_normal);
```

and forwards material/UV fields unchanged.

- [ ] **Step 2: Add `actor3d.frag`**

Use the same lighting/fog/material logic as `scene3d.frag`, with the same first 48-float scene uniform layout.

- [ ] **Step 3: Compile/package actor shader**

`tools/compile_shaders.sh` must create `content/shaders/actor3d.gsh`. `CMakeLists.txt` must fail configuration when it is absent and WUHB content packaging must include it through the content directory.

- [ ] **Step 4: Extend renderer resource structures**

`ActorSkinCache` must contain:

```cpp
GX2RBuffer gpuVertexBuffer{};
std::uint32_t gpuVertexCapacity{};
bool gpuReady{};
```

Renderer must contain:

```cpp
WHBGfxShaderGroup actorGroup_{};
GX2RBuffer actorIndexBuffer_{};
std::uint32_t actorIndexCount_{};
alignas(256) float actorUniformBlock_[64]{};
```

- [ ] **Step 5: Initialize/destroy actor pipeline and per-slot buffers safely**

Load `shaders/actor3d.gsh`, initialize attributes with the same offsets as `Vertex3D`, create the shared index buffer with `GX2R_RESOURCE_BIND_INDEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ`, and destroy every actor buffer/index buffer/shader group during shutdown.

- [ ] **Step 6: Run source-structure test until actor-resource checks pass**

Expected: remaining failures should only concern the direct indexed draw implementation if not yet completed.

- [ ] **Step 7: Commit**

Commit message:

```text
feat: add dedicated actor GPU pipeline
```

---

### Task 4: Replace AAM2 triangle expansion with indexed direct draws

**Files:**
- Modify: `include/renderer.hpp`
- Modify: `src/renderer.cpp`
- Test: `tests/indexed_actor_renderer_test.py`
- Test: `tests/skinning_tests.cpp`

**Interfaces:**
- Consumes: `skinUniqueVertices`, `ActorSkinCache::gpuVertexBuffer`, shared `actorIndexBuffer_`.
- Produces: direct AAM2 `GX2DrawIndexedEx` path with one uploaded render vertex per unique source vertex.

- [ ] **Step 1: Initialize the shared index buffer lazily from the first detailed mesh**

Copy `mesh.indices()` once into `actorIndexBuffer_`; if mesh topology changes, rebuild only when index count/topology identity differs. If creation fails, call AAM1 fallback.

- [ ] **Step 2: Create actor vertex buffer per cache slot**

Capacity must equal at least `mesh.vertices().size()`. Recreate only when capacity is insufficient.

- [ ] **Step 3: Build/upload one `Vertex3D` per unique skinned source vertex only on pose/material refresh**

For each `cache.localVertices[i]`, produce:

```cpp
makePreparedVertex(local.position, local.normal, local.uv, material)
```

No world-space CPU transform and no index expansion are allowed.

- [ ] **Step 4: Upload model transform uniform per draw**

Build a model matrix from `Transform::position`, `yaw`, `scale`, append it after the first 48 scene floats in `actorUniformBlock_`, byte-swap all floats, invalidate the uniform block, and bind it to the actor vertex shader. Pixel shader receives the first scene portion it needs.

- [ ] **Step 5: Flush generic 3D batch before direct actor draw**

Then bind actor vertex buffer and call:

```cpp
GX2DrawIndexedEx(
    GX2_PRIMITIVE_MODE_TRIANGLES,
    actorIndexCount_,
    GX2_INDEX_TYPE_U32,
    actorIndexBuffer_.buffer,
    0,
    1);
```

- [ ] **Step 6: Low/Culled fallback guarantees**

Low and Culled return before shared index/actor vertex creation. Any GPU actor resource failure uses `enemyLowDetailMesh_` through `submitMesh` and never calls the old expanded AAM2 loop.

- [ ] **Step 7: Run all host tests**

Expected: policy, skinning, world layout, performance geometry, UI isolation, indexed actor renderer all PASS.

- [ ] **Step 8: Commit**

Commit message:

```text
perf: draw skinned AAM2 actors with indexed GPU buffers
```

---

### Task 5: Cross-build Wii U, verify WUHB, and assemble hardware-test package

**Files:**
- Modify if needed: `.github/workflows/build-v11-core.yml`
- Output: GitHub Actions artifact and local final ZIP assembled from the verified full V11 Combined assets.

**Interfaces:**
- Produces: `ArenaAssault_Aroma_SD_V11_INDEXED_ACTOR.zip` containing `wiiu/apps/ArenaAssault/ArenaAssault.wuhb` with the new RPX, `actor3d.gsh`, full Corridor/crates, and real 5,496-triangle AAM2.

- [ ] **Step 1: Ensure CI verifies actor shader**

Add:

```bash
test -s content/shaders/actor3d.gsh
```

- [ ] **Step 2: Push and wait for full GitHub Actions build**

Required successful stages: host tests, asset validation, CafeGLSL compile, WUT configure, RPX/WUHB build, Aroma packaging, artifact upload.

- [ ] **Step 3: Download the core artifact**

Verify new `arena_assault.rpx`, `actor3d.gsh`, and core WUHB are present.

- [ ] **Step 4: Rebuild final WUHB with full verified V11 assets**

Use the previously hardware-working UIFIX/Combined full asset set, replacing only the core RPX and shader set with the newly built indexed-actor versions.

- [ ] **Step 5: Validate final ZIP/WUHB**

Confirm ZIP integrity, required WUHB path, real AAM2 byte size (230,516 bytes), Corridor/crate files, and actor shader presence.

- [ ] **Step 6: Hardware acceptance test**

Test one Trooper entering camera, two nearby Troopers, distant LOD transition, hit/death, corridor view, TV HUD, and GamePad map. Do not merge to `main` until hardware behavior is approved.
