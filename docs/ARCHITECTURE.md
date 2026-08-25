# Arena Assault Wii U v3 — architecture

## Platform loop

`main.cpp` owns only Wii U lifecycle, VPAD polling and the separate TV/DRC
render passes. Game logic does not call WHBGfx directly except for the explicit
2D depth-state transition already used by the HUD/map.

## Game layer

- `game.*` — waves, update order, hit tests, AI orchestration.
- `player.hpp` / `weapon.hpp` — player state and weapon state.
- `enemy.hpp` — AI state and per-enemy animation phase.
- `map.*` — static arena colliders, ray/AABB queries and LOS.
- `particle.*` — fixed-size particle pool for muzzle, impact and death effects.
- `audio.*` — semantic gameplay sounds backed by sndcore2/AX.

## Rendering

`renderer.*` keeps two pipelines:

1. scene 3D — material data, atlas, lighting, point lights, fog/specular,
2. UI 2D — HUD and independent GamePad tactical map.

Static boxes, AAM1 meshes and AAM2 skinned meshes all end up in the same CPU
triangle batch. This is deliberate for Wii U: the sample robot has a tiny bone
count and CPU skinning avoids a second heavy vertex-shader path while the
vertex budget remains comfortably below the 65k-frame batch limit.

## Animation

`animation.hpp` contains:

- generic looping scalar keyframes,
- the 8-bone robot pose,
- bind-space bone pivots,
- CPU point/normal transforms,
- walk/attack/hit-react pose generation.

AAM2 supports four bone weights per vertex. The bundled robot uses mostly rigid
weights per mechanical segment, which fits a robot and keeps deformation
stable. Blender may export blended weights when desired.

## Particles

`ParticleSystem` has a fixed 192-element pool with no per-frame heap allocation.
Effects currently include:

- player and AI muzzle flashes,
- wall/body impact sparks,
- enemy death burst.

Particles are submitted as tiny emissive boxes into the normal 3D batch, so no
extra GX2 shader is required.

## Audio

`AudioSystem` initializes AX at 32 kHz when no other owner has initialized it.
It acquires 12 reusable voices, generates compact PCM16 effects at startup and
plays them on both TV and DRC. World sounds get distance attenuation and
left/right pan relative to the listener yaw. If AX cannot initialize or no
voice can be acquired, gameplay continues silently.

## Asset fallbacks

Load order for enemy visuals:

1. `assets/meshes/enemy_body.aam2` — full skeletal robot,
2. `assets/meshes/enemy_body.aam` — static mesh + procedural limbs,
3. procedural boxes only.

Texture load order remains external atlas TGA -> white 1x1 fallback.
