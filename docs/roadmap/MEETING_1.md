# Meeting 1 — Release Planning Session

**Date:** 1 May 2026
**Participants:** Fred (project lead), Copilot (interviewer)
**Format:** Grill-me session — structured Q&A to reach shared
understanding on v1.0 release scope.

**Outputs:**
- [ROADMAP.md](ROADMAP.md) — Decision record
- [ROADMAP_PLAN.md](ROADMAP_PLAN.md) — Phased execution plan
- [../GLOSSARY.md](../GLOSSARY.md) — Project terminology

---

## Q1: What's the single most important thing maxivmac should do next?

**Context:** Four categories of open work identified — bug fixes
(dangling-else ASC sound, autoSlowTime mismatch), architecture/code
quality (m68k.cpp monolith, 107 globals, 294 `#if 0` blocks),
feature work (shared drive, clipboard, file transfer), and UI polish.

**Recommendation:** Fix the dangling-else ASC bug first — one-line
fix, restores sound on Mac II/IIx.

**Fred:** I haven't touched it because I don't believe it will be
sufficient to fix it — it did seem to really work on the original
minivmac. We will need a session on sound together later. What I'd
like instead is a roadmap to release — a minimal checklist to publicly
release the emulator. I know it fits a real need in the Mac community,
but I need it to be release-ready. I can spend so much time grinding,
but I want to achieve it.

---

## Q2: What does "publicly release" mean?

**Options presented:** A) GitHub source only, B) GitHub + pre-built
binaries, C) Package managers, D) Full polish with website/docs.

**Recommendation:** B — pre-built binaries for macOS minimum.

**Fred:** I am a perfectionist. I strive for D. My other mac project
macflim (https://github.com/fstark/macflim) has all of that.

---

## Q3: Which Mac models must work flawlessly on day one?

**Recommendation:** Ship v1.0 with black-and-white compact Macs
(Plus, SE as heroes). Mac II as experimental/beta.

**Fred:** I agree on the focus. MacPlus (full support) and MacII (no
sound) is correct.

**Correction from Fred:** macflim's scope was underestimated — it
works on every known compact Mac, from Mac XL / Mac 128K to SE/30,
supports all Mac OS from System 1 to System 7.5.3. Took 4 years.
maxivmac is based on minivmac which already supports all those things.
macflim work is massively bigger.

---

## Q4: Which guest OS versions must the shared drive INIT work with?

**Options:** A) 6.0.8 + 7.x only, B) 4.1+ through 7.5.x, C) System
1–3 through 7.5.x.

**Recommendation:** A — System 6.0.8 and System 7.x.

**Fred:** A. Specifically 6.0.8, a 7.0x, a 7.1x and a 7.5.x. Maybe
trim a couple of the 7x. Deferred decision based on complexity.

On the INITs: just the clipboard init and shared disk init, probably
merged into a single one. The others come from minivmac and can be
removed. After release, can't change the internal API, so moving the
sony disk to register-based API may be a task on the roadmap.

Additional requirements for guest OS/INIT:
- Proper clean OS installs on HFS disks (users must not scour the
  internet).
- Support only US English.
- A way to package the INIT for easy installation — "tools" concept
  where host checks if guest has tools and offers to mount a read-only
  HFS disk with tools ready to install.

---

## Q5: How are you going to legally distribute Mac OS system images?

**Context:** Apple System software copyright. ROM files. Apple
distribution media found in repo with zero legal documentation.

**Options:** A) Ship free ones only (6.0.8, 7.0.1), document rest.
B) Ship free ones + first-launch wizard for others. C) Ship
everything, rely on abandonware. D) Partner with preservation org.

**Recommendation:** B — ship 6.0.8 and 7.0.1 (clearly free), guided
download for 7.5.x, user-supplied ROMs.

**Fred:** C. Worst that happens is I have to remove them. I'll deal
with inconsistency later. Let me rephrase: let's design for the
perfect world. If there's a manual step (download ROMs, download OS),
it's a post-install/pre-first-launch step. I don't want the *design*
of the running system to be influenced by lawyers. That said, if 6.0.8
and 7.0.1 are "clean," let's focus on them. On the ROM, we know their
MD5, so we can easily check at startup.

---

## Q6: What is the out-of-box experience?

**Options presented:** A) Guided wizard flow, B) Opinionated defaults
(boot into Plus+6.0.8 automatically), C) Model selector → boot →
tools disk auto-mounted.

**Recommendation:** C, but with INIT pre-installed on bundled boot
disks.

**Fred:** Some sort of C. But non-US users will need to install the
INIT. And many people in the Mac community already have dozens of
images they want to boot.

**Critical correction:** The flow presented was wrong — ROM implies
model. Choosing ROM then model makes no real sense. First boot should
be model selector. All models with ROMs enabled.

**Key insight from Fred:** As some models may not work with all OSes,
there needs to be a file describing each machine (name, model, ROM,
disk, RAM, network, etc.). A generic "probe" to see what can boot and
why not. Advanced users could add their own. It's not feature bloat —
it's the minimal correct abstraction.

---

## Q7: Define "machine" precisely — system-level or user-level?

**Two layers identified:**

**Layer 1 — Model (system-level):** Static record of what a
configuration *is*. Maps to `MachineConfigForModel()`. Small, bounded.
The model selector reads this.

**Layer 2 — Machine (user-level):** Saved, user-editable
configuration. Name, base model, disk, RAM, shared drive, etc. What
advanced users create for multiple setups.

**Recommendation:** Layer 1 (Model) for release, Layer 2 (Machine)
for v1.1.

**Fred:** OK for machine being a default instantiation of a model.
But no workarounds — no way to specify RAM etc. in v1.0, because
those are machine concepts. If there are variations, we'd be faster to
implement Machine properly. One way to instantiate a model, period.

On the naming conflict (existing `Machine` class in runtime code):
must be solved. Find a better name for the existing runtime class.
Painful but trivial refactoring.

---

## Q8: Naming the three layers

**Proposed and accepted:**

| Term | Meaning |
|------|---------|
| **Model** | What Apple designed. Immutable, ships with app. |
| **Macintosh** | What's on your desk. User-owned saved config. |
| **Rig** | The live runtime emulation engine. |

Natural language: *"What model is it?" → "A Plus." "How's your
machine set up?" → "4 megs, System 7, shared drive."*

"Macintosh" was Fred's idea — questioned whether it was ridiculous,
but it perfectly matches. Nobody in 1988 said "my machine" — they said
"my Macintosh."

**Code mapping:**
- `MacModel` enum → unchanged
- `Macintosh` struct → v1.1 (user config, saved to disk)
- `Rig` class → rename of current `Machine` class, `g_rig`

**Decision:** v1.0 ships with Model only. Each Model has exactly one
hardcoded Macintosh (default config, no customization). Rig rename
done early as mechanical refactor.

---

## Q9: Rig naming for runtime object

**Candidates evaluated:** Emulator, EmulationCore, Rig, Board,
System, Engine.

**Selected: Rig.** Short (3 letters), evocative ("hardware rig"),
no collision with Mac terminology or codebase. `g_rig->config()`,
`g_rig->findDevice<VIA1>()`.

---

## Q10: v1.0 feature set for shared drive

**Current state (much more complete than TODO suggested):**
- Multiple shared drives, read/write
- Proper resource forks
- Automatic text transcoding both ways
- Drag/drop of folders from host Finder
- Developed and tested on Mac II + System 6.0.8

**Missing:** Watch-for-changes. Any kind of test suite. Not tested on
Plus.

**v1.0 TODO:** Test on Plus + System 6.0.8 and 7.0.1.

**Deferred:** Watch-for-changes.

**Fred's correction on TODO items:**
- Sony driver to register API → mandatory (locks extension interface)
- File import/export kludge → superseded by shared drive
- Multiple shared drives → already done

**Product differentiation (Fred's list):**
1. Clipboard and shared drive (and screenshots)
2. All models in one binary, no recompilation for RAM changes
3. Dynamic video card on Mac II (resolution changes)
4. Serial port networking via SLIP
5. Debugger (too advanced to detail, but huge differentiator)

---

## Q11: Clipboard bug and release bar

**The "weird bug":** Trailing character on some host↔guest pastes —
probably \0 from C string terminator. Non-issue.

**Communication delay:** Implemented infrequent polling. Could be
snappier. v1.2 concern.

**Private scrap limitation (THINK C etc.):** Correct behavior per
MultiFinder design. Documented limitation, not a bug.

---

## Q12: Which host platforms for v1.0?

**Fred's answer (non-negotiable, based on macflim experience):**
- macOS x86_64 and arm64, via Homebrew
- Windows x86_64 and arm64, via downloadable ZIP
- Linux via git clone + make

Five targets, same as macflim. The Windows retro computing community
is large. CI/CD through GitHub Actions is mandatory — manual releases
are a fake timesaver.

---

## Q13: CI/CD status

**Current:** No GitHub Actions. No CI pipelines. No release
automation. 260 unit tests exist locally. CMake presets for macOS
only.

**Fred:** No CI/CD. We need to build it.

---

## Q14: CI/CD pipeline design

**Agreed scope:**
- On push: build + unit tests on all 3 OSes
- On tag: + golden test + release artifacts
- SDL3 vendored in-repo (self-contained builds)
- Golden test: commit Mac Plus ROM, rely on abandonware principle.
  If forced to remove, at least we know it worked at some point.
- Start with build + unit tests, add release artifacts, defer
  golden-file CI initially if needed.

**Fred on dependencies:** Added all dependencies in folder for macflim
so it's self-contained. Same approach. Not worried about
implementation, more about getting the right files in the right place.

---

## Q15: INIT porting effort (Mac II → Plus)

**Assessment:** Bounded unknown. Could be 15 minutes ("oh look, it
works!") or 8 hours. Won't take more than 8 hours.

**Key question if lots of work:** Two INITs, or one with lots of `if()`
and dead code?

**Decision:** Time-boxed at 8 hours. Investigate and port.

---

## Q16-17: Overlay UI for release

**Current state:** ~90% implemented. 11 phases completed.

**Release blockers:**
- G8: Close window doesn't quit. Must fix.
- Toast notifications: errors go to stderr, invisible to users.

**Polish:**
- Shortcut keys on buttons (discoverability).
- Terminology consistency (Integer vs Pixel Perfect).

**Deferred:** Configurable overlay key, snap/zoom minor bugs, macOS
Ctrl+Click quirk.

**Additional issue from Fred:** The model selection/configuration
screen exists but doesn't work (not wired). This led to the launcher
discussion.

---

## Q18: Launcher vs config screen

**The tension:** Fred hates config screens. But without one, you need
command line. Command line means no casual users.

**Resolution:** A **launcher** is not a config screen. It's a list of
things you can boot. "Choose a Macintosh" with cards. Click → boot.
No tabs, no fields, no dropdowns.

**v1.0 scope:**
- 2-3 hardcoded Macintoshes (Plus+6.0.8, Plus+7.0.1, MacII+6.0.8)
- CLI escape hatch for power users
- No model → error (don't guess, fail with clear message)
- Kill the current broken config tabs
- "+ New Macintosh" button comes in v1.1

**Fred agreed.** Corrected: "if we don't know, we error" — never
default to Plus, because HFS drives that need Mac II would crash.

---

## Q19-20: Website, docs, README

**README = landing page.** Hero section with mp4/gif clips (10 seconds
each) showing differentiators. Getting Started below.

**Blog post:** Reuse README visuals + background text.

**Social:** Mastodon post with repo link. Discord channel (new or
reuse macflim's — TBD).

**External dependency:** Blog site must generate early-HTML pages for
the SLIP/Netscape demo clips.

---

## Q21: App icon

**Decision:** 32x32 black-and-white, Mac-inspired with a "maxi"
twist. Fred will create it (created macflim's icon himself). Separate
creative task, not engineering. Step in the GTM plan.

---

## Q22: Remaining loose ends

**Debugger:** Ship as-is.

**SLIP networking:** Works. Needs demo material. External dependency
on blog site early-HTML generation.

**Tools disk:** Do the v1.1 version directly — build the HFS anyway
(that's the hard part, skip CI/CD for now). When HFS is in
distribution: detect if INIT is installed, if not add overlay button
to mount the disk. Takes less time than writing a README explaining
manual installation. INIT having an icon/display on start is v1.1.

**Versioning:** 1.0.0 = first public. 1.0.x = bug fixes. 1.1.0 = new
features. Serves marketing, not strict semver.

**Packaging:** Homebrew (macOS), ZIP (Windows), source (Linux). No
notarization. No .app bundle ceremony. Zero "you are not allowed to
run your own software" crap.

**Name:** maxivmac is final. Pun on minivmac, signals "maxi" / do
more. Positions as natural successor.

**Attribution:** Lineage is vMac → Mini vMac (Paul C. Pratt + others,
including UAE CPU code) → maxivmac. Placeholder attribution for v1.0,
full credits researched later. Fred removed a lot of original code, so
attribution must be accurate about what's actually still in there.

---

## Decisions Summary

| # | Topic | Decision |
|---|-------|----------|
| 1 | Release level | D — full polish |
| 2 | Guest models | Plus (full) + Mac II (beta, no sound) |
| 3 | Guest OS | 6.0.8 + 7.0.1 (freely distributable) |
| 4 | Legal stance | Ship everything, remove if asked |
| 5 | ROM validation | MD5 at startup |
| 6 | Naming | Model / Macintosh / Rig (three layers) |
| 7 | v1.0 Macintosh scope | Hardcoded defaults only |
| 8 | Shared drive | Feature-complete; test on Plus |
| 9 | Sony driver | Migrate to register API before release |
| 10 | Clipboard | Working; trailing \0 noted; latency v1.2 |
| 11 | INIT | Merge + port to Plus; time-boxed 8h |
| 12 | Host platforms | macOS + Windows + Linux (5 targets) |
| 13 | Distribution | Homebrew / ZIP / source |
| 14 | CI/CD | GitHub Actions, all platforms |
| 15 | Launcher | Card-based, no config panel |
| 16 | No model → error | Never guess |
| 17 | UI blockers | G8 (close=quit), toast notifications |
| 18 | Website/docs | README-as-landing-page with mp4/gif hero |
| 19 | Rename Machine→Rig | Mechanical refactor, do early |
| 20 | Name | maxivmac (final) |
| 21 | Attribution | Placeholder v1.0, full credits later |
| 22 | Icon | 32x32 B&W, creative task |
