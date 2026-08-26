# V11 Renderer Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Arena Assault able to render heavier modular sci-fi environment assets and higher-detail enemies without silently dropping geometry.

**Architecture:** Keep the existing dynamic GX2 renderer but make it batch safely when the streaming vertex buffer fills. Add conservative mesh bounding spheres and camera-frustum culling so off-screen models are skipped before transformation/skin work. Add a small host-testable render policy layer for culling, LOD selection, and batch-capacity decisions.

**Tech Stack:** C++17, devkitPro/WUT, GX2/GX2R, host-side g++ tests.

**Spec:** Approved in-chat design: dynamic batching + frustum/distance culling + LOD policy as the first V11 renderer foundation.

## Global Constraints

- Preserve the current AAM1/AAM2 formats and existing game behavior.
- Do not require runtime CafeGLSL or new Wii U dependencies.
- Keep the existing 65,536-vertex streaming buffer; remove its silent geometry-loss behavior instead of merely increasing memory.
- Culling must be conservative: visible objects may be drawn unnecessarily, but potentially visible objects must not disappear.

---

### Task 1: Host-testable render policy

**Files:**
- Create: `include/render_policy.hpp`
- Create: `include/mesh_bounds.hpp`
- Create: `tests/render_policy_tests.cpp`

**Interfaces:**
- Produces: `sphereVisible(const Camera&, const Vec3&, float)`, `selectLod(float,float,float)`, `batchWouldOverflow(size_t,size_t,size_t)`, `MeshBounds`, `BoundsAccumulator`.

- [ ] Write tests for front/behind/off-axis/far-plane sphere visibility, LOD thresholds, buffer overflow decisions, and mesh bounds.
- [ ] Run the host test before implementation and confirm compilation fails because the new headers do not exist.
- [ ] Implement the minimum policy and bounds helpers.
- [ ] Run `g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude tests/render_policy_tests.cpp -o /tmp/render_policy_test && /tmp/render_policy_test` and confirm PASS.

### Task 2: Persist mesh bounds

**Files:**
- Modify: `include/mesh.hpp`
- Modify: `src/mesh.cpp`

**Interfaces:**
- Consumes: `MeshBounds`, `BoundsAccumulator`.
- Produces: `Mesh::bounds()` and `SkinnedMesh::bounds()`.

- [ ] Add a conservative bounding sphere to both loaded mesh types.
- [ ] Build bounds while reading vertex positions and clear them on `clear()`.
- [ ] Preserve all current AAM validation rules.

### Task 3: Safe dynamic batching and culling

**Files:**
- Modify: `include/renderer.hpp`
- Modify: `src/renderer.cpp`

**Interfaces:**
- Consumes: `sphereVisible`, `batchWouldOverflow`, mesh bounds.
- Produces: automatic 3D batch flushes and renderer statistics.

- [ ] Store the current camera at `begin3D()` and reset per-frame statistics.
- [ ] Before static/skinned mesh transformation, conservatively cull its transformed bounding sphere.
- [ ] Replace silent triangle dropping at 65,536 vertices with an automatic batch flush and continue submission.
- [ ] Make every batch clear its CPU staging vector after drawing so later geometry is not redrawn.
- [ ] Keep public `flush3D()` behavior as the final end-of-scene flush.

### Task 4: Verification

**Files:**
- Verify modified source and tests.

- [ ] Re-run host policy tests with warnings enabled.
- [ ] Inspect changed source for buffer-overflow, stale-batch, and invalid-camera edge cases.
- [ ] Run the Wii U build workflow if the toolchain is available; otherwise explicitly report that platform compilation remains CI/device verification work.
