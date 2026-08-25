# Arena Assault Mesh formats

## AAM1 — static mesh

Little-endian binary format retained for compatibility and fallback rendering.

Header (16 bytes):

| Offset | Type | Meaning |
|---:|---|---|
| 0 | char[4] | `AAM1` |
| 4 | u32 | version = 1 |
| 8 | u32 | vertex count |
| 12 | u32 | index count |

Vertex (32 bytes): `float3 position`, `float3 normal`, `float2 uv`.
Indices are `u32` triangle-list indices.

## AAM2 — skinned mesh

AAM2 extends AAM1 with up to four bone influences per vertex. Skinning is
performed on the CPU immediately before the renderer batches the triangles.
This keeps the GX2 scene shader simple and leaves AAM1 fully usable.

Header (20 bytes):

| Offset | Type | Meaning |
|---:|---|---|
| 0 | char[4] | `AAM2` |
| 4 | u32 | version = 2 |
| 8 | u32 | vertex count |
| 12 | u32 | index count |
| 16 | u32 | bone count (1..16) |

Vertex (64 bytes):

| Bytes | Type | Meaning |
|---:|---|---|
| 0..11 | 3 x f32 | position xyz |
| 12..23 | 3 x f32 | normal xyz |
| 24..31 | 2 x f32 | uv |
| 32..47 | 4 x u32 | bone indices |
| 48..63 | 4 x f32 | normalized weights |

Indices follow immediately after the vertex array and are `u32` triangle-list
indices. Loader rejects invalid counts, out-of-range indices/bones and malformed
file lengths. Weights are normalized again at load time.

## Robot bone IDs

The v3 sample robot and Blender exporter use a fixed tiny skeleton:

0. `root`
1. `torso`
2. `head`
3. `arm_l`
4. `arm_r`
5. `leg_l`
6. `leg_r`
7. `weapon`

The runtime supports up to 16 bones, but the sample pose uses eight. The fixed
mapping intentionally avoids storing strings and hierarchy tables in every
mesh file.

## Blender export

`tools/export_aam_blender.py` writes AAM2 by default. Create vertex groups with
the bone names above and assign weights. The exporter keeps the four strongest
weights and normalizes them. Vertices without a known group become `root=1`.

For a static compatibility mesh:

```python
export_selected(r"C:\\temp\\enemy_static.aam", skinned=False)
```

For the skinned format:

```python
export_selected(r"C:\\temp\\enemy_body.aam2", skinned=True)
```

Coordinates are converted from Blender Z-up to Arena Assault Y-up.
