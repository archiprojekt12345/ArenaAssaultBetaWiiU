#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# The devkitPPC container intentionally ships only the compiler toolchain.
# Older CI for this project invokes this script before CMake, so install the
# Wii U SDK packages here when running under GitHub Actions. This keeps reruns
# of the existing workflow able to link coreinit/vpad/whb/gx2 without needing
# another manual workflow dispatch.
if [[ "${GITHUB_ACTIONS:-}" == "true" ]] && command -v dkp-pacman >/dev/null 2>&1; then
  for i in 1 2 3; do
    echo "Syncing devkitPro package databases (attempt $i/3)"
    if dkp-pacman -Syu --noconfirm; then
      break
    fi
    if [[ "$i" == "3" ]]; then
      exit 1
    fi
    sleep 5
  done
  dkp-pacman -S --needed --noconfirm wiiu-dev wut-tools
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
