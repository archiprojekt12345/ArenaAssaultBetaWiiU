# Validation report — v3

Completed in the build session:

- C++17 syntax check for all `src/*.cpp` with `-Wall -Wextra -Wpedantic -Werror` using API-compatible Wii U stubs.
- Native C++ test of `Mesh` and `SkinnedMesh` loaders against the bundled files.
- `tools/validate_assets.py` structural checks for AAM1, AAM2 and TGA.
- Python syntax compilation for all tools.
- Asset regeneration from `tools/generate_sample_assets.py`.
- AAM2 validation result: 712 vertices, 340 triangles, 8 bones.
- AAM1 fallback validation result: 256 vertices, 112 triangles.
- Atlas validation result: 256x256 RGBA TGA.

Not completed here:

- final PowerPC link to RPX (devkitPro/wiiu-dev toolchain unavailable in this environment),
- runtime test on physical Wii U hardware.
