#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# V11 stores the converted binary models as one compressed repository asset so
# GitHub can carry them reliably. Expand them into the WUHB content tree before
# compiling or packaging.
ASSET_PACK="$ROOT/tools/v11_assets/v11_meshes.tar.xz"
if [[ -f "$ASSET_PACK" ]]; then
  mkdir -p "$ROOT/content/assets/meshes"
  tar -xJf "$ASSET_PACK" -C "$ROOT/content/assets/meshes"
fi

COMPILER="${CAFEGLSL_COMPILER:-}"
if [[ -z "$COMPILER" ]]; then
  for c in glslcompiler.elf glslcompiler shader_compiler; do
    if command -v "$c" >/dev/null 2>&1; then COMPILER="$(command -v "$c")"; break; fi
  done
fi
if [[ -z "$COMPILER" || ! -x "$COMPILER" ]]; then
  echo "CafeGLSL host compiler not found. Set CAFEGLSL_COMPILER=/path/to/glslcompiler.elf" >&2
  exit 2
fi
mkdir -p "$ROOT/content/shaders"
"$COMPILER" -vs "$ROOT/shaders/scene3d.vert" -ps "$ROOT/shaders/scene3d.frag" -o "$ROOT/content/shaders/scene3d.gsh"
"$COMPILER" -vs "$ROOT/shaders/ui2d.vert"    -ps "$ROOT/shaders/ui2d.frag"    -o "$ROOT/content/shaders/ui2d.gsh"
echo "Shaders ready in content/shaders/"
