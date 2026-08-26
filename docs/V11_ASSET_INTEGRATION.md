# V11 asset integration

The V11 renderer optionally loads the industrial environment pack from `content/assets/meshes/`.

Expected runtime files:

- `corridor_light.aam`
- `corridor_white.aam`
- `corridor_gray.aam`
- `corridor_black.aam`
- `corridor_blue.aam`
- `corridor_yellow.aam`
- `corridor_glass.aam`
- `corridor_detail.aam`
- `supply_crate.aam`

The optimized uploaded Stormtrooper conversion uses the existing runtime name:

- `enemy_body.aam2`

so `Game::init()` continues to load it through the normal AAM2 path without a gameplay-code special case.

The corridor is a repeatable 3 m slice of the uploaded sci-fi corridor, kept as separate material groups. The renderer assigns metal, yellow industrial, glass-like dark material, white light, and blue emissive material at runtime. Two copies frame the terminal and extraction sides of the arena. Eight small-box props are placed around the existing cover layout and get blue/orange/green emissive status panels.

Asset files are optional: if one is missing, renderer initialization continues and logs the missing asset. This keeps old content packages usable while V11 content is being iterated.

A final Aroma `.wuhb` must be rebuilt after copying these files into `content/assets/meshes/`, because WUHB embeds `/vol/content` at build time.
