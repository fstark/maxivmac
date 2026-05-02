# CI/CD

Continuous integration and release automation for maxivmac.
GitHub Actions builds on every push/PR, runs tests, and produces
release artifacts on tag.

See [GLOSSARY.md](../GLOSSARY.md) for terminology.

---

## Goals

1. Every push and PR is built on all supported platforms.
2. Unit tests run on every build — no ROM required.
3. One golden-file regression test per platform (needs Plus ROM).
4. Tagged builds produce downloadable release artifacts.
5. One manual step: review the draft release and click Publish.

---

## Platforms

| Platform | Arch | Runner |
|----------|------|--------|
| macOS | arm64 + x86_64 | `macos-latest` (arm64), `macos-13` (x86_64) |
| Windows | x86_64 + arm64 | `windows-latest` (x86_64), cross-compile arm64 |
| Linux | x86_64 | `ubuntu-latest` |

---

## On Every Push / PR

### Build

Build with CMake on all five platform×arch combinations.
Dependencies are vendored (SDL3, imgui, etc.) so no package manager
fetches are needed — the build is self-contained.

### Unit Tests

Run the doctest suite (~260 tests). These are pure host-side tests
that do not require a ROM or disk image. Gated via `ctest`.

### Golden-File Regression Test

Boot Mac Plus ROM (committed to repo), run a headless golden-file
test, compare output against a checked-in reference. One test per
platform. If the ROM is ever removed from the repo, degrade
gracefully to build + unit tests only.

---

## On Tag (Release)

Everything above, plus:

### macOS

Build a universal binary (arm64 + x86_64 fat binary). Publish to a
Homebrew tap so users can install with one command:

```
brew install maxivmac/tap/maxivmac
```

No notarization. No signed `.app` bundle. Homebrew builds from source
and bypasses Gatekeeper.

### Windows

Produce two ZIP archives:
- `maxivmac-win-x86_64.zip` — executable + SDL3 DLL.
- `maxivmac-win-arm64.zip` — executable + SDL3 DLL.

### Linux

Produce a source tarball (`maxivmac-<version>.tar.gz`). Linux users
build from source via `cmake`.

### GitHub Release

Create a **draft** GitHub Release attached to the tag. Upload all
artifacts. A human reviews the draft and clicks Publish.

The release body is built automatically from two sources:

1. **"What's New"** — extracted from the annotated tag message
   (`git tag -a v1.0.0 -m "First public release..."`). Write
   release notes in the tag, not in a separate file.
2. **"Installation"** — hardcoded template with per-platform
   install instructions, stamped with the tag version.

Example release body:

```
## What's New

First public release of maxivmac.

## Installation

### macOS
brew install fstark/maxivmac/maxivmac

### Windows
Download maxivmac-win-x86_64-v1.0.0.zip or
maxivmac-win-arm64-v1.0.0.zip. Includes all required DLLs
and bundled ROMs.

### Linux
Download maxivmac-v1.0.0.tar.gz and build from source:
  cmake --preset linux && cmake --build bld/linux
```

---

## Version Embedding

Already implemented. CMake runs `git describe --tags --match "v*"
--always` at configure time and compiles the result as
`-DMAXIVMAC_VERSION="..."`. Tagged builds get `v1.0.0`. Dev builds
get `v1.0.0-7-gabcdef` or `dev-abcdef` if no tags exist.
`--dirty` suffix appears for uncommitted changes.

---

## Homebrew Tap

Requires a separate repository: `fstark/homebrew-maxivmac`
containing `Formula/maxivmac.rb`.

The tap is updated by a separate workflow
(`.github/workflows/homebrew-update.yml`) triggered when a release
is **published** (not drafted). Steps:

1. Download the release source tarball, compute SHA256.
2. Check out `fstark/homebrew-maxivmac` using a
   `HOMEBREW_TAP_TOKEN` secret.
3. `sed`-replace the URL and SHA256 in the formula.
4. Commit and push as `github-actions[bot]`.
5. Smoke-test: install via `brew` on macOS arm64 + x86_64 runners,
   run `maxivmac --version` to verify.

---

## Workflow Structure

Three workflow files:

### `.github/workflows/ci.yml`

Single job with a platform matrix.

- **Trigger:** push, pull_request.
- **Matrix:** macOS-arm64, macOS-x86_64, Windows-x86_64,
  Windows-arm64, Linux-x86_64.
- **Steps:** checkout → configure (CMake preset) → build → unit
  tests → golden test (conditional on ROM presence).

### `.github/workflows/release.yml`

- **Trigger:** push of a version tag (`v*`).
- **Jobs:**
  1. `build-and-test` — reuses CI matrix, all platforms must pass.
  2. `create-release` — create draft GitHub Release with body
     from annotated tag message + install template.
  3. `package` — download build artifacts → create macOS universal
     binary → package Windows ZIPs with DLLs + ROMs → package
     Linux tarball → upload all to the draft release.

### `.github/workflows/homebrew-update.yml`

- **Trigger:** release published.
- **Jobs:**
  1. `update-formula` — download tarball, compute SHA256, push
     updated formula to `fstark/homebrew-maxivmac`.
  2. `test-install` — smoke-test `brew install` on macOS arm64
     + x86_64, verify `maxivmac --version`.

---

## ROM Handling

The Mac Plus ROM is committed to the repo for golden testing.
The workflow checks ROM presence before running golden tests.
If absent, golden tests are skipped — build and unit tests still
gate the PR.

Release artifacts bundle all ROMs from `data/roms/`. Users get a
working setup out of the box — no separate ROM downloads needed.

---

## Out of Scope (v1.0)

- INIT CI/CD (building the THINK C guest-side code). Deferred to
  v1.1 when the INIT build can be automated via maxivmac itself.
- Code signing / notarization.
- Nightly builds.
- Coverage reporting in CI.
- Linux binary packages (`.deb`, `.rpm`, Flatpak).
