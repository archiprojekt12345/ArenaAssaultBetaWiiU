from pathlib import Path

root = Path(__file__).resolve().parents[1]
renderer_h = (root / "include" / "renderer.hpp").read_text(encoding="utf-8")
renderer_cpp = (root / "src" / "renderer.cpp").read_text(encoding="utf-8")
main_cpp = (root / "src" / "main.cpp").read_text(encoding="utf-8")

# TV and DRC must never share the same GX2R 2D vertex buffer. WHB only waits
# for the GPU at WHBGfxFinishRender(), after both surfaces have been submitted.
assert "uiTvVertexBuffer_" in renderer_h
assert "uiDrcVertexBuffer_" in renderer_h
assert "GX2RBuffer uiVertexBuffer_" not in renderer_h

# The renderer must select the GPU buffer from an explicit surface target.
assert "enum class UITarget" in renderer_h
assert "setUITarget(UITarget target)" in renderer_h
assert "uiTarget_" in renderer_h
assert "uiTvVertexBuffer_" in renderer_cpp
assert "uiDrcVertexBuffer_" in renderer_cpp

# The frame orchestration owns the surface choice: TV before renderTV, DRC
# before renderMap. This keeps Game independent from WHB surface state.
assert "setUITarget(aa::Renderer::UITarget::TV)" in main_cpp
assert "setUITarget(aa::Renderer::UITarget::DRC)" in main_cpp

print("ui_buffer_isolation_test: PASS")
