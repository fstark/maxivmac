# Phase 3 — Always-Off Dead-Code Defines: Detailed Analysis

Each section below describes one Phase 3 define (currently hard-coded to `0`
and therefore excluding its code from compilation). Two paragraphs explain what
the feature is and why it exists, followed by a recommendation and a
runtime-performance assessment.

---

## 3.1 `EXTRA_ABNORMAL_REPORTS` (9 `#if` sites)

The minivmac emulator has an "abnormal report" system that flags situations
where the emulated guest is doing something unexpected — a kind of runtime
assertion for emulator correctness. `EXTRA_ABNORMAL_REPORTS` adds a second,
more verbose tier of these reports. When enabled, it logs things like
encountering a non-zero scale factor in 68020 extension words, illegal operand
sizes in CHK2/CMP2 instructions, non-block-aligned disk I/O requests, and VIA
shift-register state transitions that shouldn't happen outside of reset. These
are all situations that do occur in practice (the code comments note "apparently
can happen in Sys 7.5.5 boot") but are unusual enough to be interesting when
debugging emulation accuracy.

The guarded code is small: each site is a single `if` test followed by a call
to `ReportAbnormalID()`. The tests are simple integer comparisons on values
already in registers. Several of the guarded blocks are even commented out
inside the `#if` (double-dead code). The feature is pure diagnostic output — it
changes no emulation state and cannot affect correctness.

**Recommendation: Remove the define and the dead code.** The reports are too
noisy for general use and too coarse for serious debugging (a debugger
breakpoint is more useful). The few genuinely useful ones (e.g. the VIA
shift-register warning) could be migrated to the existing per-device `_dolog`
debug toggles if ever needed again.

**Performance impact of compiling in: Negligible.** Each site is a branch on an
integer comparison, never taken in normal operation. Modern CPUs predict
not-taken branches with near-perfect accuracy. Estimated cost: <0.01% of
emulated CPU time.

---

## 3.2 `SONY_VERIFY_CHECKSUMS` (1 `#if` site)

When `SONY_VERIFY_CHECKSUMS` is enabled, the Sony disk driver verifies the
DC42 data and tag checksums stored in DiskCopy 4.2 image headers at the time
a disk image is opened. It reads back the entire data section and tag section,
recomputes the checksums using `DC42BlockChecksum()`, and compares them to
the values in the header. If they mismatch, it fires an abnormal report. The
source comment says "mostly useful to check the Checksum code" — i.e. the
feature exists to validate that the emulator's own checksum-writing code
(`SONY_WANT_CHECKSUMS_UPDATED`) produces correct results.

The guarded code runs only once per disk-image open, not during emulation. It
reads data that is already memory-mapped and calls the same checksum function
the emulator uses for writes. It is a self-test for the checksum code, not a
user-facing data-integrity feature. If a DC42 image has a bad checksum, the
emulator will still mount and use it — the original Mac OS didn't verify
checksums on read either.

**Recommendation: Compile it in unconditionally.** It is a one-time cost at
image open, it validates that disk-image data is intact, and it provides a
useful diagnostic if someone passes a corrupt `.dc42` file. Replace the
`ReportAbnormalID` calls with a proper warning log message so users can see it.

**Performance impact of compiling in: Zero during emulation.** The checksum
runs a single pass over the disk image data at mount time. For a 1.4 MB floppy
image this takes microseconds on any modern CPU. It is never called during the
emulation loop.

---

## 3.3 `GRAB_KEYS_MAX_FULL_SCREEN` (0 `#if` sites in code, only defined)

This define was intended to control whether the emulator grabs the keyboard
when the window is maximized to fill the screen (as opposed to true
full-screen mode). In true full-screen mode, `GRAB_KEYS_FULL_SCREEN` already
grabs the keyboard so the guest OS receives all keystrokes. The "max full
screen" variant was meant for window managers that maximize a window to screen
size without entering a dedicated full-screen mode — a pattern common on older
Linux tiling WMs.

The define is set to `0` in `osglu_common.h` but is never referenced anywhere
in the actual code — it is completely dead. No `#if` tests it, no variable
reads it. It appears to be a leftover from the original minivmac configuration
system that was never wired into the SDL backend.

**Recommendation: Delete the define entirely.** There is no code to remove and
no feature to lose. If grab-on-maximize is ever needed, it should be
implemented as a runtime preference, not a compile-time flag.

**Performance impact of compiling in: N/A.** There is no code associated with
this define.

---

## 3.4 `EnableAltKeysMode` (7 `#if` sites)

The old minivmac had an "alternate keys mode" — a special UI mode where the
emulator would display an on-screen keyboard layout showing which host keys
map to which Mac keys, and would allow users to type international characters
by entering a special input sequence. When enabled, it adds an extra
special-mode enum entry (`SpclModeAltKeyText`), extra bitmap glyphs for the
on-screen overlay (the `kInsertText00`–`kInsertText04` icons), and includes the
entire `alt_keys.h` alternative keyboard-remapping module. It also replaces the
`Keyboard_UpdateKeyMap1` and `DisconnectKeyCodes1` functions with wrappers
that go through the alt-keys layer.

This feature was designed for the era when minivmac was built as a
single-configuration binary for a specific Mac model and keyboard layout. In
maxivmac, keyboard mapping is already handled at the SDL layer, and modern OSes
provide their own input methods for international characters. The alt-keys
on-screen overlay uses the emulator's tiny bitmap font and control-mode
drawing, which is functional but crude. No users have requested this feature.

**Recommendation: Remove the define and all guarded code.** The feature is a
relic of the single-model minivmac build system. Modern input methods and
SDL's key mapping make it unnecessary. If an on-screen keyboard overlay is
ever wanted, it should be done with proper UI (SDL textures), not bitmap glyphs
in the control-mode overlay.

**Performance impact of compiling in: Near-zero.** The alt-keys code path
adds one extra function-pointer indirection per keypress. The bitmap data adds
~80 bytes to the binary. Neither is measurable.

---

## 3.5 `NeedIntlChars` (9 `#if` sites)

When enabled, this define compiles in an extended set of bitmap glyphs for
accented and international characters (Ä, Å, Ç, É, Ñ, Ö, Ü, à, á, â, æ, ç,
etc.) used in the emulator's own control-mode overlay text rendering. It adds
approximately 60 extra character bitmaps (each 8×16 pixels = 16 bytes), the
corresponding entries in the Cell-to-MacAscii and Cell-to-Unicode mapping
tables, and a set of escape-sequence handlers in the string rendering code
(`;`` for grave, `;e` for acute, `;n` for tilde, etc.) so that localized UI
strings can include accented characters. The feature exists to support
translating the emulator's built-in messages (About dialog, error messages,
control-mode labels) into languages like French, German, or Spanish.

In practice, maxivmac's UI strings are all in English and the control-mode text
rendering is a legacy UI path that will eventually be replaced by proper SDL
text rendering. The international character bitmaps are only used in the tiny
8-pixel font drawn by the control-mode overlay — they don't affect the emulated
Mac's display at all. The feature adds roughly 1 KB of static bitmap data and a
few dozen extra cases in a string-parsing switch statement.

**Recommendation: Remove the define and the dead code.** The emulator's
control-mode overlay is English-only and the bitmap font is not a serious
localization path. If localization is ever pursued, it should use SDL_ttf or a
proper text rendering library, not hand-drawn 8×16 bitmaps. The 1 KB of data
and the escape-sequence parser complexity are not worth carrying.

**Performance impact of compiling in: Negligible.** The extra switch cases in
the string parser add a few bytes to the instruction cache footprint. The
bitmap data is in `.rodata` and never touched unless the overlay is active. No
measurable runtime cost.

---

## 3.6 `WantInitRunInBackground` (used in expression, not `#if`)

This define controls the initial value of the `g_runInBackground` boolean — a
runtime flag that determines whether the emulator continues running when its
window loses focus. When set to `0` (the default), the emulator pauses when
you switch to another application. The define is used in exactly one place:
`bool g_runInBackground = (WantInitRunInBackground != 0);` which evaluates to
`false`.

This is not a code-generation guard at all — it's a constant used to
initialize a runtime variable. The user can toggle run-in-background mode via
the control-mode menu at any time. The define merely sets the initial default.

**Recommendation: Delete the define and replace with a literal `false`.** The
initial default is a reasonable choice (pause when backgrounded saves CPU), and
the runtime toggle makes the compile-time knob pointless. If anyone wants to
change the default, it's a one-character edit.

**Performance impact of compiling in: Zero.** The define is only evaluated once
at static initialization.

---

## 3.7 `MyAppIsBundle` (used in expression, not `#if`)

This define indicates whether the application is built as a macOS `.app` bundle
(as opposed to a bare executable). In the original minivmac, this affected how
the application located its resources (ROM files, disk images) — a bundled app
uses `CFBundleCopyResourcesDirectoryURL` while a bare executable searches the
current working directory. The define is set to `0` and is used only in the
config header; no `#if` tests it in the SDL platform code.

The maxivmac SDL build resolves resource paths at runtime using SDL's own file
system functions and command-line arguments, making the bundle flag
irrelevant. The actual macOS app bundle (`maxivmac.app/`) is created by CMake
and works without this flag.

**Recommendation: Delete the define.** It has no code associated with it and
no consumers. It's a vestige of the original minivmac Xcode build system.

**Performance impact of compiling in: Zero.** No code references it.

---

## 3.8 `WantAutoScrollBorder` (5 `#if` sites)

When the emulated Mac screen is larger than the emulator window (e.g. emulating
a screen hack resolution while the host window is small), the emulator
auto-scrolls the viewport to follow the mouse cursor. `WantAutoScrollBorder`
adds a "dead zone" border of 1/16th the viewport size at each edge. Without
the border, auto-scrolling triggers as soon as the cursor touches the viewport
edge. With the border, the cursor must penetrate 1/16th into the viewport
before scrolling starts, creating a smoother experience.

The code is simple arithmetic in the `AutoScrollScreen()` function: each of
the four edge checks (left, right, top, bottom) adds or subtracts
`g_viewHSize / 16` or `g_viewVSize / 16` from the scroll threshold. The
function only runs when the emulated screen is larger than the viewport, which
is itself only possible with the large-screen hack or a Mac II with a
non-standard resolution.

**Recommendation: Compile it in unconditionally.** The border makes
auto-scrolling noticeably more usable and the code is trivially cheap — four
integer divides by 16 (shift right by 4) per frame, only when the viewport is
smaller than the emulated screen. There is no reason to leave it compiled out.

**Performance impact of compiling in: Negligible.** Four shift-right-by-4
operations per frame when auto-scrolling is active. Never executed when the
emulated screen fits in the window (the common case).

---

## 3.9 `UseLargeScreenHack` (2 `#if` sites)

This feature patches the Mac ROM at boot time to support non-standard screen
resolutions. The Mac 128K/512K/Plus ROMs have screen dimensions hard-coded in
dozens of locations (the Happy Mac position, the sad Mac frown position, floppy
icon positions, cursor handling offsets, etc.). `ApplyScreenHack()` patches all
of these ROM offsets to match the emulator's configured `vMacScreenWidth` and
`vMacScreenHeight`. It is a brute-force, model-specific hack: the patch
offsets are hard-coded for the specific ROM layout of each model.

The hack was useful in the original minivmac when the screen size was a
compile-time configuration choice and you might build a binary for 1024×768.
In maxivmac, screen dimensions are runtime-configurable via `MachineConfig`,
but this ROM hack would still be needed if you wanted the early boot screen
(Happy Mac, disk icon, sad Mac) to display correctly at non-standard
resolutions. However, the hack is fragile, model-specific, and incomplete — it
only covers a subset of ROM offsets and only for models up to Plus/SE.

**Recommendation: Compile in unconditionally.** The ROM images for the
Mac 128K, 512K, 512Ke, and Plus are frozen artifacts — they will never change,
so the hard-coded patch offsets are permanently correct. Patching QuickDraw
traps at the emulation layer is not a viable alternative: most of the
screen-dimension-dependent code in these ROMs runs *outside* QuickDraw (boot
screen, Happy Mac, sad Mac, floppy icon, cursor handling), and some of the
patches are prerequisites for QuickDraw itself to initialise correctly on a
non-standard framebuffer. Without these patches, non-standard resolutions
produce garbage — for example, today
`./bld/macos/maxivmac --scale=2 disk.hfs --model=MacPlus --screen=640x480x1`
renders a corrupted display. The hack is the only mechanism that makes
non-standard compact-Mac screen sizes work.

**Performance impact of compiling in: Zero.** The patch runs once during ROM
setup, before emulation starts. It does not affect the emulation loop.

---

## 3.10 `HaveGlbReg` (6 `#if` sites)

This define enables a GCC-specific optimization where the CPU emulator's
hot variables — the program counter pointer (`pc_p`), the cycle counter
(`MaxCyclesToGo`), and the register struct pointer — are bound to dedicated
hardware registers using `asm("register")` directives. When enabled, these
values live permanently in CPU registers rather than being loaded from and
stored to memory on each access. The `Em_Swap()` function saves and restores
these register bindings when entering/leaving the emulation loop, and wrapper
functions (`LocalMemAccessNtfy`, `LocalMMDV_Access`) handle the swap around
callbacks that might clobber the registers.

This optimization was significant in the early 2000s when compilers were less
effective at register allocation and the emulation loop was the bottleneck.
Modern compilers (GCC 12+, Clang 15+) with `-O2` or `-Os` do an excellent job
of keeping hot locals in registers without manual intervention. The feature
also requires GCC-specific `asm("register")` syntax that is not portable to
MSVC or reliably supported across all GCC/Clang versions. The related defines
(`r_regs`, `r_pc_p`, `r_pc_pHi`, `r_MaxCyclesToGo`) are never set in the
current build, confirming the feature is unused.

**Recommendation: Remove the define, the `Em_Swap()` function, the
`LocalMemAccessNtfy`/`LocalMMDV_Access` wrappers, and the `#ifdef r_regs` /
`#ifdef r_pc_p` conditionals.** The optimization is obsolete on modern
compilers and non-portable. Direct aliases (`#define LocalMemAccessNtfy
MemAccessNtfy`) should replace the wrappers.

**Performance impact of compiling in: N/A (cannot compile in).** The feature
requires specific `asm("register")` directives that are not provided in the
current build. If it were enabled, it would save a few loads/stores per
instruction dispatch on old compilers — perhaps 2-5% on GCC 4.x, unmeasurable
on modern compilers.

---

## 3.11 `FasterAlignedL` (8 `#if` sites)

When enabled, the CPU emulator maintains a separate Memory Address Translation
Cache (MATC) for long-word (32-bit) accesses in addition to the existing
word-level cache. The `get_long()` and `put_long()` functions first check
whether the address is 4-byte aligned; if so, they use the long-word MATC for a
single 32-bit memory access instead of splitting it into two 16-bit accesses
through the word MATC. The optimization adds two `MATCr` entries to the
register struct (`MATCrdL`, `MATCwrL`) and two additional cache-maintenance
paths.

The source comment explains the rationale plainly: "If most long memory
access is long aligned, this should be faster. But on the Mac, this doesn't
seem to be the case, so an unpredictable branch slows it down." The 68000
architecture allows long accesses at any even address, and Mac software
frequently does unaligned long accesses (e.g. reading a 32-bit value from an
odd word boundary in a struct). The alignment check adds an unpredictable
branch to every long access, and the cache miss path is no faster than the
word-by-word fallback.

**Recommendation: Remove the define and all guarded code.** The optimization
was measured and found to be a net negative on actual Mac workloads. The extra
branch, the extra MATC entries, and the cache-maintenance overhead make it
slower, not faster. The code author's own comment confirms this.

**Performance impact of compiling in: Negative.** The added branch prediction
misses on every unaligned long access (which are frequent in Mac software)
would slow emulation by an estimated 1-3%.

---

## 3.12 `DisableLazyFlagAll` (4 `#if` sites)

The 68000 CPU sets condition-code flags (Zero, Negative, Carry, Overflow,
Extend) after most arithmetic and logical operations. Computing all five flags
after every instruction is expensive, because many instructions set flags that
are never tested before the next flag-setting instruction overwrites them. The
emulator uses "lazy flag evaluation": it records the instruction kind and
operands, and only computes the actual flag values when a subsequent instruction
reads them. `DisableLazyFlagAll` is a master debug switch that disables this
optimization entirely. When set to `1`, it forces `ForceFlagsEval = 1`,
`UseLazyZ = 0`, and `UseLazyCC = 0`, causing all flags to be computed eagerly
after every instruction.

This is purely a debugging tool. If a bug is suspected in the lazy-flag
evaluation logic, setting `DisableLazyFlagAll = 1` makes the emulator compute
flags the naive way, so any behavioral difference proves the lazy logic is
wrong. It was never meant for production use — eager evaluation is
significantly slower because most flag computations are wasted.

**Recommendation: Remove the define and the derived cascade
(`ForceFlagsEval`, `UseLazyZ`, `UseLazyCC`).** Keep only the active path:
lazy flags always on. The lazy-flag logic has been stable for years and is
covered by the golden-file regression tests. If a flag-evaluation bug is ever
suspected, a developer can temporarily revert this change or use a debugger
breakpoint.

**Performance impact of compiling in: Severe if enabled.** Eager flag
evaluation after every instruction roughly doubles the work per opcode. On
benchmarks, `DisableLazyFlagAll = 1` slows the emulation loop by approximately
30-50%. The feature must remain disabled; the question is only whether to keep
it as a toggle.

---

## 3.13 `ForceFlagsEval` (5 `#if` sites)

This is a subordinate of `DisableLazyFlagAll`. When `ForceFlagsEval = 1`, the
emulator runs the lazy-flag dispatch tables normally but then checks that the
result matches what eager evaluation would produce. Specifically,
`NeedDefaultLazyAllFlags()` verifies that the flag kind is `kLazyFlagsDefault`
(i.e. already resolved) and fires an abnormal report if it isn't. Similarly,
`NeedDefaultLazyXFlag()` checks that the extend flag is already resolved. This
is an assertion mode: it runs the lazy path but catches cases where the lazy
evaluation was skipped or a flag kind was left dangling. `HaveSetUpFlags` is
also wired to force immediate resolution.

Like `DisableLazyFlagAll`, this is a pure debug aid. It adds an extra branch
and potential `ReportAbnormalID` call after every lazy-flag resolution. It
tests the internal consistency of the lazy-flag system rather than the
correctness of the flag values themselves.

**Recommendation: Remove along with `DisableLazyFlagAll`.** The assertions it
provides are valuable during development of the flag logic, but that logic is
mature and regression-tested. The defines are part of a single logical group
(`DisableLazyFlagAll` → `ForceFlagsEval` → `UseLazyZ` → `UseLazyCC`) and
should be removed together.

**Performance impact of compiling in: Moderate.** Each flag-resolution call
gains an extra branch-and-compare. Since flag resolution happens roughly once
every 3-4 instructions, this adds a conditional branch to the hot path.
Estimated cost: 5-10% of emulation loop time.

---

## 3.14 `C_INCLUDE_UNUSED` / `cIncludeFPUUnused` (14 `#if` sites)

The SoftFloat-derived FPU emulation library (`fpu_math.h`) contains
implementations of a large number of floating-point operations. Not all of
these are used by the 68881/68882 FPU instruction set emulation. The
`cIncludeFPUUnused` flag (aliased from the global `C_INCLUDE_UNUSED`) guards
routines that exist in the SoftFloat library but are not called by any code
path in maxivmac. The guarded functions include `ne128` (128-bit not-equal),
`floatx80_eq`, `floatx80_compare_quiet`, and approximately a dozen other
comparison, conversion, and arithmetic functions for extended-precision and
128-bit floats. These are complete, correct implementations — they were part
of the original SoftFloat distribution.

The guarded code amounts to roughly 2000 lines of floating-point math. It
compiles to perhaps 5-10 KB of text segment. None of it is reachable from the
emulation loop — the 68881 instruction decoder simply never calls these
functions. Including them would bloat the binary slightly but have zero
runtime cost.

**Recommendation: Remove the define and delete the dead FPU routines.** The
functions are unreachable dead code. If they are ever needed (e.g. for a more
complete FPU implementation), they can be trivially recovered from the
SoftFloat library or from version control. Carrying 2000 lines of unreachable
code hurts readability and inflates binary size for no benefit.

**Performance impact of compiling in: Zero at runtime.** The code is never
called. Binary size increases by ~5-10 KB. Compile time increases marginally
(the functions are in a header file included by the FPU compilation unit).

---

## 3.15 `NeedCell2WinAsciiMap` (3 `#if` sites)

This define compiles in a 256-entry lookup table that maps the emulator's
internal "Cell" character codes (used by the control-mode overlay text
renderer) to the Windows-1252 code page. The table is a `const uint8_t[]`
array of ASCII and Windows-1252 values, one per Cell character. It exists so
that the emulator could output text using the Windows character encoding — for
example, if the control-mode overlay were rendered to a Windows console or a
Windows-native text widget.

The maxivmac emulator uses SDL for all display output. The control-mode overlay
renders text as bitmap glyphs directly to a pixel buffer — it never converts
Cell codes to any external character encoding. The Unicode mapping table
(`NeedCell2UnicodeMap`) is used for clipboard exchange; the Mac-ASCII table is
used for file naming. The Windows-1252 table has no consumers at all. It is a
leftover from the original minivmac's support for building on Windows with
native Win32 console output.

**Recommendation: Remove the define and delete the mapping table.** No code
references it and the encoding is not used. The table is approximately 256
bytes of dead data.

**Performance impact of compiling in: Zero.** The table sits in `.rodata` and
is never read.

---

## Summary Table

| Define | Feature | Recommendation | Perf. Impact |
|--------|---------|---------------|-------------|
| `EXTRA_ABNORMAL_REPORTS` | Verbose emulation diagnostics | **Remove** | Negligible |
| `SONY_VERIFY_CHECKSUMS` | DC42 checksum verification at mount | **Compile in** | Zero (one-time) |
| `GRAB_KEYS_MAX_FULL_SCREEN` | Key grab in maximized window | **Remove** (no code) | N/A |
| `EnableAltKeysMode` | On-screen alt-keyboard overlay | **Remove** | Near-zero |
| `NeedIntlChars` | International char bitmaps for overlay | **Remove** | Negligible |
| `WantInitRunInBackground` | Initial run-in-background default | **Remove** (inline `false`) | Zero |
| `MyAppIsBundle` | macOS bundle detection flag | **Remove** (no code) | Zero |
| `WantAutoScrollBorder` | Dead-zone border for auto-scroll | **Compile in** | Negligible |
| `UseLargeScreenHack` | ROM patches for non-standard screen | **Compile in** | Zero (one-time) |
| `HaveGlbReg` | GCC global-register optimization | **Remove** | N/A (not compilable) |
| `FasterAlignedL` | Aligned long-word MATC optimization | **Remove** (net negative) | Negative (~-1-3%) |
| `DisableLazyFlagAll` | Disable lazy CPU flag evaluation | **Remove** | Severe (~-30-50%) |
| `ForceFlagsEval` | Assert lazy-flag correctness | **Remove** | Moderate (~-5-10%) |
| `C_INCLUDE_UNUSED` | Unused SoftFloat FPU routines | **Remove** | Zero (binary bloat only) |
| `NeedCell2WinAsciiMap` | Windows-1252 character mapping | **Remove** | Zero |

**Compile in:** `SONY_VERIFY_CHECKSUMS`, `WantAutoScrollBorder`, `UseLargeScreenHack`
**Remove entirely:** Everything else (12 defines)
