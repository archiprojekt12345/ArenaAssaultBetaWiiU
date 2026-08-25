# Easy Launch / Aroma distribution

Primary distribution format: `ArenaAssault.wuhb`.

At build time CafeGLSL compiles the two GLSL pipelines into GFD `.gsh` files. Those files plus all meshes/textures become the WUHB content tree. At runtime `main.cpp` uses `/vol/content`; no shader compiler RPL is loaded on the console.

End-user install path:

`SD:/wiiu/apps/ArenaAssault/ArenaAssault.wuhb`

For old/debug RPX launch, the application falls back to `SD:/wiiu/apps/ArenaAssault/content`.
