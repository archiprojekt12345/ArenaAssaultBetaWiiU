# Arena Assault Wii U ALPHA 0.8.3 Release-Only Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the source-oriented repository contents with a clean distribution-only layout containing the prebuilt Arena Assault ALPHA 0.8.3 WUHB and documentation.

**Architecture:** Build a new Git tree from scratch on the preparation branch so no legacy source/build files survive accidentally. Preserve the supplied WUHB byte-for-byte, retain the existing MIT license, add installation/release documentation, verify the resulting tree, then fast-forward `main` to the verified release commit.

**Tech Stack:** GitHub Git Data API, Markdown/plain-text documentation, Wii U Aroma WUHB binary

**Spec:** `docs/superpowers/specs/2026-08-30-release-only-0.8.3-design.md`

## Global Constraints

- Target version is exactly `ALPHA 0.8.3`.
- Final runtime path is exactly `wiiu/apps/ArenaAssault/ArenaAssault.wuhb`.
- Source code and old build infrastructure must not remain in the final tree.
- Preserve the supplied WUHB byte-for-byte.
- Expected WUHB size: `1,537,124` bytes.
- Expected WUHB SHA-256: `fe634edf6c5520977dbb0e6db1d115bc14d3bc3448cc7b27325ca3b322a4101b`.
- Retain the existing `LICENSE` file unchanged.
- Do not invent gameplay claims that are not established by the supplied release package or reliable existing project information.

---

### Task 1: Verify release payload

**Files:** supplied ZIP and its two entries.

- [ ] Verify entries: `wiiu/apps/ArenaAssault/ArenaAssault.wuhb` and `ARENA_ASSAULT_INSTALL.txt`.
- [ ] Verify WUHB size `1537124` and SHA-256 `fe634edf6c5520977dbb0e6db1d115bc14d3bc3448cc7b27325ca3b322a4101b`.
- [ ] Preserve the exact Polish installer text from the archive.

### Task 2: Prepare release documentation

**Files:** `README.md`, `CHANGELOG.md`, `docs/RELEASE_0.8.3.md`, `ARENA_ASSAULT_INSTALL.txt`, preserved `LICENSE`.

- [ ] Rewrite `README.md` for `ALPHA 0.8.3` with installation, controls, package contents, verification checksum, repository scope, and license.
- [ ] Create `CHANGELOG.md` describing 0.8.3 as current and the repository transition to distribution-only form without inventing gameplay claims.
- [ ] Create `docs/RELEASE_0.8.3.md` with package type, path, size, SHA-256, included files, and runtime-content note.
- [ ] Preserve `LICENSE` unchanged.

### Task 3: Create clean release tree

**Final tree only:**

```text
README.md
LICENSE
ARENA_ASSAULT_INSTALL.txt
CHANGELOG.md
docs/RELEASE_0.8.3.md
wiiu/apps/ArenaAssault/ArenaAssault.wuhb
```

- [ ] Create Git blobs for all final files; WUHB must use base64 input of the exact verified bytes.
- [ ] Create a Git tree from scratch, with no base tree, so no old source/build files survive.
- [ ] Create commit `release: publish Arena Assault ALPHA 0.8.3 WUHB` using current `release-only-0.8.3` head as parent.
- [ ] Move `release-only-0.8.3` to that commit without force.

### Task 4: Verify prepared branch

- [ ] Verify the final tree contains exactly the six files listed above and no legacy `.github`, `src`, `include`, `content`, `shaders`, `tools`, `CMakeLists.txt`, build scripts, or old docs.
- [ ] Re-fetch WUHB blob and verify size `1537124` and SHA-256 `fe634edf6c5520977dbb0e6db1d115bc14d3bc3448cc7b27325ca3b322a4101b`.
- [ ] Verify docs consistently use `ALPHA 0.8.3` and exact path `wiiu/apps/ArenaAssault/ArenaAssault.wuhb`.

### Task 5: Publish to `main`

- [ ] Fast-forward `main` to the verified release commit with `force=false`.
- [ ] Verify the public `main` tree again.
- [ ] After verification, enable the `Protect main branch` ruleset manually with default branch target, `Restrict deletions`, and `Block force pushes` enabled; leave other restrictions disabled unless deliberately added later.
