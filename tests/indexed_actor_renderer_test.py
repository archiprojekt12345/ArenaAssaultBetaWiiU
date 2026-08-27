from pathlib import Path

root = Path(__file__).resolve().parents[1]
renderer_h = (root / "include" / "renderer.hpp").read_text(encoding="utf-8")
renderer_cpp = (root / "src" / "renderer.cpp").read_text(encoding="utf-8")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
shader_script = (root / "tools" / "compile_shaders.sh").read_text(encoding="utf-8")

# Detailed AAM2 actors must own a dedicated indexed GPU path.
assert "actorIndexBuffer_" in renderer_h
assert "gpuVertexBuffer" in renderer_h
assert "actorGroup_" in renderer_h
assert "GX2DrawIndexedEx" in renderer_cpp
assert "actor3d.gsh" in renderer_cpp

# AAM2 must no longer expand triangle indices into the generic scene stream.
actor_start = renderer_cpp.index("void Renderer::submitSkinnedMesh")
actor_end = renderer_cpp.index("void Renderer::flush3DBatch", actor_start)
actor_body = renderer_cpp[actor_start:actor_end]
assert "pushTri3D(" not in actor_body
assert "sceneVertices_" not in actor_body

# Each actor slot owns a separate persistent GPU vertex buffer.
assert "static constexpr std::size_t kActorCacheSlots = 16" in renderer_h
assert "GX2RBuffer gpuVertexBuffer" in renderer_h

# Actor shader is a required build/package input.
assert "actor3d.gsh" in cmake
assert "actor3d.gsh" in shader_script

print("indexed_actor_renderer_test: PASS")
