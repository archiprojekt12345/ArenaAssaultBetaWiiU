#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
"$ROOT/tools/compile_shaders.sh"
rm -rf "$ROOT/build"
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
python3 "$ROOT/tools/package_sd.py" "$ROOT/build/ArenaAssault.wuhb" -o "$ROOT/ArenaAssault_Aroma_SD.zip"
echo "READY: $ROOT/ArenaAssault_Aroma_SD.zip"
