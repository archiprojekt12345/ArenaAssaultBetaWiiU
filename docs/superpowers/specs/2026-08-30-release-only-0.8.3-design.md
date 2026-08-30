# Arena Assault Wii U — release-only repository design

Date: 2026-08-30
Target version: ALPHA 0.8.3
Target branch for preparation: `release-only-0.8.3`

## Goal

Convert the repository from a source-code repository into a distribution-only repository for Arena Assault Wii U ALPHA 0.8.3.

The `main` branch should ultimately contain only the ready-to-run Aroma/WUHB build and user/developer-facing documentation. Source code is intentionally excluded because it is retained separately by the project owner.

## Release artifact

The supplied archive contains:

```text
ARENA_ASSAULT_INSTALL.txt
wiiu/apps/ArenaAssault/ArenaAssault.wuhb
```

`ArenaAssault.wuhb` size: 1,537,124 bytes

SHA-256:

```text
fe634edf6c5520977dbb0e6db1d115bc14d3bc3448cc7b27325ca3b322a4101b
```

## Final repository layout

```text
ArenaAssaultBetaWiiU/
├── README.md
├── LICENSE
├── ARENA_ASSAULT_INSTALL.txt
├── CHANGELOG.md
├── docs/
│   └── RELEASE_0.8.3.md
└── wiiu/
    └── apps/
        └── ArenaAssault/
            └── ArenaAssault.wuhb
```

The existing source/build directories and files are to be removed from the release-only branch, including CMake/build scripts, `.github` build workflow, `src`, `include`, `content`, `shaders`, `tools`, and source-oriented documentation that no longer describes the distributed repository.

## README

`README.md` will be rewritten for ALPHA 0.8.3 and will clearly state that this repository distributes a prebuilt Wii U Aroma application rather than source code.

It will include:

- version and platform,
- installation path,
- installation steps,
- basic menu controls,
- note that models, textures and required runtime content are packaged in the WUHB,
- SHA-256 checksum for verifying the release file,
- repository scope and source-code policy.

## Installation documentation

The supplied `ARENA_ASSAULT_INSTALL.txt` will be retained as the minimal Polish installation guide.

`docs/RELEASE_0.8.3.md` will provide a cleaner release note and technical summary of the distributed package.

## Changelog

`CHANGELOG.md` will identify 0.8.3 as the current distribution release and record the repository transition from the older source-oriented layout to release-only distribution.

No gameplay features will be invented if they cannot be verified from the supplied release package or existing reliable project information.

## License

The existing `LICENSE` file will be retained unless the project owner explicitly requests a license change.

## Branch and protection strategy

Changes will be prepared on `release-only-0.8.3` before replacing `main`.

The GitHub ruleset for the default branch should be configured as:

- name: `Protect main branch`,
- target: default branch (`main`),
- `Restrict deletions`: enabled,
- `Block force pushes`: enabled,
- other merge/status restrictions left disabled unless later needed,
- enforcement kept disabled during repository replacement and enabled after the new `main` is verified.

## Verification

Before considering the migration complete:

1. Verify the WUHB byte size and SHA-256 against the supplied archive.
2. Verify that the final repository tree contains no old source/build files.
3. Verify README installation path is exactly `wiiu/apps/ArenaAssault/ArenaAssault.wuhb`.
4. Verify documentation consistently names the release `ALPHA 0.8.3`.
5. Verify `LICENSE` remains present.
6. Review the final `main` branch after replacement.

## Known constraint

The supplied artifact is a binary WUHB. Repository editing must preserve the binary exactly; it must not be converted, re-encoded, or regenerated during the repository migration.
