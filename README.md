# Arena Assault Wii U — v0.5 Mission Build

Ta wersja zachowuje prosty model dystrybucji **Aroma + WUHB**, a jednocześnie przebudowuje gameplay w pełną krótką misję z trzema klasami AI i minibossem.

## Dla gracza

Gotowe wydanie ma postać `ArenaAssault_Aroma_SD.zip`. Użytkownik tylko rozpakowuje je do katalogu głównego karty SD. Efektem jest:

```text
SD:/
└── wiiu/
    └── apps/
        └── ArenaAssault/
            └── ArenaAssault.wuhb
```

Po uruchomieniu Aroma gra jest dostępna z menu Wii U. Modele, tekstury i skompilowane shadery są osadzone wewnątrz WUHB jako `content`.

Skrócona instrukcja po polsku znajduje się w `README_USER_PL.txt`.


## Co nowego w v0.5

- trzy odrębne klasy AI: **Scout / Soldier / Heavy**,
- różne HP, szybkość, zasięg walki, celność, obrażenia i styl poruszania,
- wizualnie różne gabaryty, kolory, markery GamePada i dodatkowe elementy modeli,
- elitarny **Heavy miniboss** z osobnym paskiem HP,
- nowa struktura misji: **infiltracja → terminal → obrona → miniboss → ewakuacja**,
- terminal aktywowany przytrzymaniem **A**,
- podczas obrony czas celu płynie tylko wewnątrz strefy obronnej,
- mieszane składy przeciwników zamiast zwykłego zwiększania numeru fali,
- aktualny cel i strefa obronna są zaznaczone na ekranie GamePada,
- TV HUD pokazuje postęp pięciu etapów misji.

Szczegóły: `docs/MISSION_SYSTEM.md`.

## Co zmieniło się względem v3

- finalny target to **WUHB**, nie luźny RPX + assety,
- runtime używa `/vol/content`,
- shadery GLSL są kompilowane **offline do `.gsh`**,
- usunięto runtime dependency na `glslcompiler.rpl`,
- WUHB zawiera `content/assets` i `content/shaders`,
- dodane są icon + splash TV + splash GamePada,
- build tworzy gotowy układ `build/sd/wiiu/apps/ArenaAssault/`,
- dołączony workflow GitHub Actions buduje shader compiler, shadery, RPX, WUHB i ZIP do rozpakowania na SD.

## Automatyczny build przez GitHub Actions

Workflow: `.github/workflows/build-wiiu.yml`.

Po umieszczeniu projektu w repozytorium można uruchomić workflow **Build Wii U release**. Jego artefakt `ArenaAssault-Aroma-SD` zawiera:

- `ArenaAssault_Aroma_SD.zip` — paczka dla zwykłego użytkownika,
- `ArenaAssault.wuhb` — pojedyncza aplikacja Aroma.

Workflow używa CafeGLSL tylko **na komputerze budującym**, aby wygenerować `scene3d.gsh` i `ui2d.gsh`. Na Wii U CafeGLSL nie jest potrzebny.

## Build lokalny

Wymagania developerskie:

1. devkitPro z grupą `wiiu-dev`,
2. hostowy CafeGLSL `glslcompiler.elf` 0.2.x,
3. ustawione `CAFEGLSL_COMPILER=/sciezka/do/glslcompiler.elf`.

Następnie:

```bash
./build_release.sh
```

Skrypt generuje:

```text
ArenaAssault_Aroma_SD.zip
```

## Ręczny build

```bash
./tools/compile_shaders.sh
mkdir -p build
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
python3 tools/package_sd.py build/ArenaAssault.wuhb -o ArenaAssault_Aroma_SD.zip
```

## Struktura content wewnątrz WUHB

```text
content/
├── assets/
│   ├── meshes/
│   │   ├── enemy_body.aam
│   │   └── enemy_body.aam2
│   └── textures/
│       ├── arena_atlas.tga
│       └── atlas_layout.json
└── shaders/
    ├── scene3d.gsh
    └── ui2d.gsh
```

`.gsh` są generowane przy budowaniu i dlatego nie są przechowywane jako ręcznie edytowane źródła. Źródła GLSL pozostają w `shaders/`.

## Legacy / developer RPX

Kod zachowuje fallback do:

```text
SD:/wiiu/apps/ArenaAssault/
├── ArenaAssault.rpx
├── meta.xml
├── icon.png
└── content/
```

Target CMake `legacy_package` przygotowuje taki układ. Do normalnej dystrybucji zalecany jest jednak WUHB.

## Walidacja w tym środowisku

Kod C++17 v0.5 przeszedł kontrolę składni z `-Wall -Wextra -Wpedantic -Werror` na warstwie testowych stubów WUT. Nie wykonano finalnego linkowania PowerPC ani testu na fizycznym Wii U, ponieważ lokalny toolchain devkitPro/WUT nie jest dostępny w tym środowisku.
