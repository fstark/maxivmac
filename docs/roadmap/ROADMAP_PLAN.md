# maxivmac v1.0 Release Plan

Phased execution plan for the v1.0 release.
See [ROADMAP.md](ROADMAP.md) for the full decision record.
See [../GLOSSARY.md](../GLOSSARY.md) for terminology.

Each phase is independently buildable and testable. Phases within a
track can be parallelized across tracks. Dependencies between tracks
are marked explicitly.

---

## Track A — Foundation

These are mechanical prerequisites that unblock everything else.

### A1. Rename Machine → Rig

Rename the `Machine` class to `Rig`, `g_machine` to `g_rig`, update
all ~200 call sites. Mechanical find-and-replace + compile + test.

**Gate:** builds, 260 unit tests pass, 4 headless golden tests pass.

### A2. Model definition data

Extract the per-model facts from `MachineConfigForModel()` into a
declarative data structure: name, ROM filename, ROM MD5, compatible
OS list, default RAM, screen geometry. The launcher and ROM
validation will consume this.

**Gate:** builds, existing behavior unchanged, data accessible via API.

### A3. CI/CD — build + unit tests

GitHub Actions workflows for macOS (arm64 + x86_64), Windows
(x86_64 + arm64), Linux (x86_64). Build + run 260 unit tests on all
platforms. No golden tests yet (ROM not committed yet).

**Gate:** green CI on all 3 platforms.

### A4. CI/CD — golden test + release artifacts

Commit Plus ROM. Add 1 golden-file regression test per platform.
Add release job: macOS universal binary → Homebrew tap, Windows ZIPs,
Linux source tarball. GitHub Release on tag.

**Gate:** tagged test release produces downloadable artifacts on all
platforms.

**Depends on:** A3.

---

## Track B — Guest-Side

The INIT and extension interface work. Largely independent of host
code changes.

### B1. Sony driver API migration

Migrate the sony disk driver from the legacy interface to the
register-based extension API. This is the last chance to change the
extension interface before it is locked at release.

**Gate:** all disk operations work on Mac II + System 6.0.8 (existing
test environment).

### B2. Merge clipboard + shared drive INITs

Combine the two INITs into a single code resource. Build in THINK C.
Decision: one codebase with shared glue, or two independent code
segments in one resource.

**Gate:** clipboard and shared drive both work on Mac II + 6.0.8 from
the merged INIT.

**Depends on:** B1 (extension interface must be final).

### B3. Port INIT to Plus

Test merged INIT on Plus + System 6.0.8 and Plus + System 7.0.1.
Fix any trap dispatch, memory map, or ROM compatibility issues.
Time-boxed at 8 hours. If issues are severe, ship Plus without
shared drive and document it.

**Gate:** INIT works on Plus with both OS versions, or a clear
decision on what to ship without.

**Depends on:** B2.

### B4. Build bundled boot disks

Create clean HFS boot disk images:
- Plus + System 6.0.8 + INIT installed
- Plus + System 7.0.1 + INIT installed
- Mac II + System 6.0.8 + INIT installed

Committed as binary assets in the repo.

**Gate:** each disk boots in the emulator with working clipboard and
shared drive.

**Depends on:** B3.

---

## Track C — Launcher and UX

### C1. Launcher: "Choose a Macintosh"

Replace the current model selector with a card-based launcher. Each
card represents one hardcoded Macintosh (from the bundled boot disks).
Cards show: icon / model name / OS version / status. Greyed-out
cards show why they can't boot (ROM missing, disk missing).

Click a card → boot. No config panel. No tabs.

**Gate:** launcher displays correct cards, boots the selected
Macintosh, greyed-out cards show correct reasons.

**Depends on:** A2 (model data), B4 (boot disks exist).

### C2. CLI behavior cleanup

- `maxivmac --model=Plus --disk=foo.hfs` → bypass launcher, boot
  directly.
- `maxivmac foo.hfs` (no --model) → error with clear message.
- `maxivmac` (no args) → show launcher.

**Gate:** all three cases behave correctly.

### C3. UI fix: close window = quit

Fix G8 — clicking the window close button must quit the application.

**Gate:** closing the window quits on macOS, Windows, Linux.

### C4. Toast notifications

Lightweight overlay toasts for user-visible errors: disk open failed,
ROM wrong/missing, too many disk images, etc. Replace silent stderr
logging.

**Gate:** error conditions produce visible toasts instead of silent
failures.

### C5. UI polish

- Shortcut keys displayed on overlay buttons.
- Terminology unified (pick "Integer" or "Pixel Perfect").

**Gate:** visual inspection.

---

## Track D — Release Prep

### D1. README

Hero section with mp4/gif clips (one per differentiator):
1. Transparent clipboard
2. Shared drive
3. All models in one binary
4. Dynamic Mac II video
5. SLIP networking / Netscape 2.0
6. Debugger

Getting Started section. One-line install per platform.
Attribution line.

**Depends on:** B4 (need working product to record clips), D2 (icon).
External dependency: blog site early-HTML for SLIP demo clip.

### D2. App icon

32x32 black-and-white Mac-inspired icon. Creative task, not
engineering. Needed for README hero, launcher cards, platform
packaging.

### D3. Blog post

Reuse README visuals. Add background story, motivation, feature
deep-dives. Publish on personal blog.

**Depends on:** D1.

### D4. Social / community

- Mastodon announcement with repo link.
- Discord channel decision (new vs reuse macflim's).

**Depends on:** D1, D3.

---

## Execution Order

Tracks A and B can start immediately in parallel. Track C depends on
parts of A and B. Track D is last.

```
Week 1-2    A1 ──→ A2 ──→ A3 ──→ A4
            B1 ──→ B2 ──→ B3 ──→ B4
                                   ↓
Week 3-4              C1 (needs A2+B4)
                      C2, C3, C4, C5 (independent)
                                   ↓
Week 5               D2 (icon, parallel)
                     D1 (README, needs B4+D2)
                     D3, D4 (after D1)
                                   ↓
                    Tag 1.0.0 → Release
```

---

## Out of Scope for v1.0

- User-creatable Macintoshes (v1.1)
- Tools disk with auto-detection (v1.1)
- INIT CI/CD build (v1.1)
- Configurable overlay key (v1.1)
- Additional guest OS versions beyond 6.0.8/7.0.1 (v1.1)
- Mac II sound (v1.1, needs debugging session)
- Clipboard polling latency (v1.2)
- Shared drive watch-for-changes (v1.2)
- Full attribution research (ongoing)
