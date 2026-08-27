from pathlib import Path

root = Path(__file__).resolve().parents[1]
renderer_h = (root / "include" / "renderer.hpp").read_text(encoding="utf-8")
actor_cpp = (root / "src" / "actor_renderer.cpp").read_text(encoding="utf-8")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
shader_script = (root / "tools" / "compile_shaders.sh").read_text(encoding="utf-8")
main_cpp = (root / "src" / "main.cpp").read_text(encoding="utf-8")

# Detailed AAM2 actors own a dedicated indexed GPU path.
assert "actorIndexBuffer_" in renderer_h
assert "gpuVertexBuffer" in renderer_h
assert "actorGroup_" in renderer_h
assert "submitSkinnedMeshIndexed" in renderer_h
assert "GX2DrawIndexedEx" in actor_cpp
assert "actor3d.gsh" in actor_cpp

# The indexed path never expands triangle indices into the generic scene stream.
actor_start = actor_cpp.index("void Renderer::submitSkinnedMeshIndexed")
actor_end = actor_cpp.index("void Renderer::clearActorGpuResources", actor_start)
actor_body = actor_cpp[actor_start:actor_end]
assert "pushTri3D(" not in actor_body
assert "sceneVertices_" not in actor_body
assert "cache.worldVertices" not in actor_body

# Each actor slot owns a separate persistent GPU vertex/uniform buffer.
assert "static constexpr std::size_t kActorCacheSlots = 16" in renderer_h
assert "GX2RBuffer gpuVertexBuffer" in renderer_h
assert "float uniformBlock[64]" in renderer_h

# Normal gameplay calls are preprocessor-routed away from the legacy hot path.
assert "#define submitSkinnedMesh submitSkinnedMeshIndexed" in renderer_h
assert "AA_LEGACY_RENDERER_SOURCE" in cmake

# Actor shader is a required build/package input and resources are explicitly freed.
assert "actor3d.gsh" in cmake
assert "actor3d.gsh" in shader_script
assert "renderer.shutdownActorGpu();" in main_cpp

print("indexed_actor_renderer_test: PASS")
