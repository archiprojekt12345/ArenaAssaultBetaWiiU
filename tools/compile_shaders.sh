#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# The shader compiler does not require world assets. If a complete optional
# V11 archive is present, expand it for packaging; if the repository copy is
# truncated, skip it and let the core build continue. The final hardware-test
# WUHB injects the verified full assets separately.
ASSET_PACK="$ROOT/tools/v11_assets/v11_meshes.tar.xz"
if [[ -f "$ASSET_PACK" ]]; then
  if xz -t "$ASSET_PACK" >/dev/null 2>&1; then
    mkdir -p "$ROOT/content/assets/meshes"
    tar -xJf "$ASSET_PACK" -C "$ROOT/content/assets/meshes"
  else
    echo "Warning: optional V11 asset archive is truncated; skipping expansion" >&2
  fi
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
