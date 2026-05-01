# maxivmac v1.0 Release Roadmap

This document captures every decision made during the v1.0 release
planning session (May 2026). It is the single source of truth for what
ships, what doesn't, and why.

See [ROADMAP_PLAN.md](ROADMAP_PLAN.md) for the phased execution plan.
See [../GLOSSARY.md](../GLOSSARY.md) for terminology.

---

## Product Identity

- **Name:** maxivmac (final).
- **Positioning:** The natural successor to Mini vMac for normal use.
  All models in one binary, runtime configuration, transparent
  host integration.
- **License:** GPL v2 (inherited from Mini vMac).
- **Attribution:** Placeholder "Built on Mini vMac (GPL v2)" for v1.0.
  Full credits (Paul C. Pratt, vMac, UAE CPU lineage) to be
  researched and expanded post-release.
- **Versioning:** 1.0.0 = first public release. 1.0.x = bug fixes.
  1.1.0 = new features. Version numbers serve marketing — "check
  out 1.1, there are new shiny things."

---

## Guest Scope

### Models

| Model | Status | Notes |
|-------|--------|-------|
| Macintosh Plus | Full support | Hero model |
| Macintosh II | Beta | No sound (ASC issue, not a simple fix) |
| All others | Hidden in v1.0 | May work via CLI, unsupported |

### Guest OS

| OS Version | Status | Legal |
|------------|--------|-------|
| System 6.0.8 | Shipped on bundled boot disk | Freely distributed by Apple |
| System 7.0.1 | Shipped on bundled boot disk | Freely distributed by Apple |
| System 7.5.x | Not bundled | Legally murky; deferred |

- English (US) only.
- Bundled boot disks ship with the INIT pre-installed.
- ROM validated by MD5 at startup. User must supply ROMs.

### INIT

- **Single merged INIT:** clipboard sync + shared drive in one code
  resource.
- **Port to Plus:** Currently tested on Mac II + System 6.0.8.
  Must be verified on Plus + 6.0.8 and Plus + 7.0.1. Time-boxed
  at 8 hours.
- **Built manually:** Compiled in THINK C inside the emulator,
  committed as a binary HFS image. No CI/CD for the INIT in v1.0.

### Sony Driver

- Migrate the sony driver from the legacy interface to the
  register-based extension API before release. This locks the
  extension interface — after release, changing it would break
  users' INITs.

---

## Host Integration Features

### Shared Drive

- **Status:** Feature-complete. Read/write, multiple drives,
  resource forks, automatic text transcoding, drag-drop of host
  folders.
- **v1.0 TODO:** Test on Plus + System 6.0.8 and 7.0.1.
- **Deferred:** Watch-for-changes (live sync when host modifies files).

### Clipboard

- **Status:** Working. Transparent bidirectional sync.
- **Known issues:**
  - Trailing \0 on some pastes (minor, noted).
  - Apps with private scraps need MultiFinder context switch
    (correct behavior, documented limitation).
- **Deferred:** Faster polling (v1.2), snappier sync.

### Debugger

- Ship as-is. Major differentiator, but no v1.0 work needed.

### SLIP Networking

- Ship as-is. Works ("on my machine"). Needs:
  - Testing / stabilization pass.
  - Demo material (Netscape 2.0 browsing the blog).
  - External dependency: blog site must generate early-HTML pages.

### Screenshot

- Ships. Ctrl+S → clipboard. Already implemented.

---

## User Experience

### First Launch Flow

```
Launch → Scan for ROMs
       → "Choose a Macintosh" launcher
       → Cards for each bootable Macintosh (greyed-out with reason if not)
       → Click → Boot
```

- **No config panel.** No tabs. No RAM dropdown. Each card is a
  fixed, pre-defined Macintosh.
- **v1.0 Macintoshes:** Plus+6.0.8, Plus+7.0.1, MacII+6.0.8 (beta).
  Hardcoded. Users cannot create new ones.
- **CLI escape hatch:** `maxivmac --model=Plus --disk=myweird.hfs`
  bypasses the launcher. Power users use command line.
- **No model → error.** If disk is specified but no model, fail with
  a clear message. Never guess.

### Overlay UI

**Release blockers:**
- G8: Closing the window must quit the app.
- Toast notifications: user-visible errors (disk open failed, ROM
  wrong, etc.) instead of silent stderr.

**Polish:**
- Shortcut keys shown on overlay buttons.
- Terminology: pick either "Integer" or "Pixel Perfect," use it
  everywhere.

**Deferred:**
- Configurable overlay activation key (v1.1).
- Snap/zoom minor bugs B2, B3 (v1.1).
- macOS Ctrl+Click quirk (workaround: use sticky mode).

### Model → Macintosh Concept

The launcher shows **Macintoshes**, not Models. In v1.0, each Model
produces exactly one hardcoded Macintosh (no user customization).
The Macintosh concept is the foundation for v1.1 (user-creatable
machines with custom RAM, disks, shared drives).

---

## Naming and Architecture

### Three-Layer Naming

| Term | Meaning | Scope |
|------|---------|-------|
| **Model** | What Apple designed. Static, immutable, ships with the app. | System |
| **Macintosh** | What's on your desk. A saved, user-owned configuration. | User |
| **Rig** | The live runtime emulation engine. | Runtime |

### Rename: Machine → Rig

The existing `Machine` class (runtime emulation object) is renamed to
`Rig` to free "Machine" / "Macintosh" for the user-facing concept.
`g_machine` → `g_rig`. Mechanical refactor, ~200 occurrences.
Do it early to avoid conflicts with later work.

---

## Host Platforms

| Platform | Arch | Distribution | Priority |
|----------|------|-------------|----------|
| macOS | arm64 + x86_64 | Homebrew tap | v1.0 |
| Windows | x86_64 + arm64 | ZIP download | v1.0 |
| Linux | x86_64 | git clone + cmake | v1.0 |

- No notarization. No signed .app bundles. Homebrew builds from
  source and bypasses Gatekeeper.
- Dependencies vendored in-repo (SDL3, imgui, etc.) for
  self-contained builds.

---

## CI/CD

**On every push/PR:**
1. Build on macOS, Windows, Linux.
2. Run unit tests (260 doctest, no ROM needed).
3. Run 1 golden-file regression test (Plus ROM committed to repo).

**On tag:**
4. All of the above, plus:
5. macOS: universal binary, publish to Homebrew tap.
6. Windows: x86_64 + arm64 ZIPs with SDL3 DLL.
7. Linux: source tarball.
8. Create GitHub Release with all artifacts.

Golden test uses Mac Plus ROM committed to repo. If removal is
required, the test degrades to build-only — at least we know it
worked at some point.

---

## Marketing / Go-to-Market

### README (= landing page)

- Hero section: mp4/gif clips (10 seconds each) showing differentiators:
  1. Transparent clipboard (copy on host, paste in guest)
  2. Shared drive (drag folder, browse in Finder)
  3. All models in one binary (model selector)
  4. Dynamic Mac II video resolution
  5. SLIP networking (Netscape 2.0 browsing)
  6. Built-in debugger
- Getting Started section below the hero.
- One-line install per platform (brew, ZIP, git clone).

### Blog Post

- Reuse README visuals + background story.
- External dependency: blog site early-HTML generation for SLIP demo.

### Social

- Mastodon announcement with link to repo.
- Discord channel (new or reuse macflim's — TBD).

### App Icon

- 32x32 black-and-white Mac-inspired icon with a "maxi" twist.
- Separate creative task, not engineering.

---

## v1.1 Roadmap (Post-Release)

- **Macintosh concept:** User-creatable machines (RAM, disks, shared
  drive, per-machine settings). "+ New Macintosh" button in launcher.
- **Tools disk:** Auto-mounted read-only HFS with INIT. Host detects
  if INIT is installed; overlay button to mount tools disk if not.
  INIT build automated via maxivmac CI.
- **Configurable overlay key.**
- **Additional guest OS versions** (7.1.x, 7.5.x) based on INIT
  porting complexity.
- **Mac II sound** (needs dedicated debugging session).

## v1.2 Roadmap

- **Clipboard polling latency** — snappier host↔guest sync.
- **Watch-for-changes** on shared drives.
