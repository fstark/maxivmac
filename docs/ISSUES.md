# Maxivmac Code Quality Assessment

A survey of `src/` identifying the ten biggest structural problems and twenty
easy high-value cleanups.

---

## The 10 Biggest Issues

### 1. Global State Everywhere (~107 `extern` symbols, God object)

`core/machine.h` and `platform/platform.h` together export **107 mutable
globals** — `RAM`, `VidMem`, `CurMouseH`, `WantMacReset`,
`g_InstructionCount`, etc.  On top of that, `g_machine` (declared in
`core/machine_obj.h`) is a God-object pointer used everywhere.

The CPU lives as a single file-scope `static struct regstruct` in
`cpu/m68k.cpp` (~100 fields).  The SCC is a `static SCC_Ty SCC;` in
`devices/scc.cpp`.  Nothing is encapsulated; everything can touch everything.

**Impact:** multi-instance impossible, side-effect reasoning impossible,
testing impossible.  Every file is coupled to every other file through global
state.

### 2. `m68k.cpp` — 8,943-line monolith with no encapsulation

`cpu/m68k.cpp` contains ~150 `static void DoCodeXXX()` instruction handlers,
the dispatch table, memory access routines, address translation, the main
execution loop, reset logic, and FPU includes — all as file-scope statics.
A custom `#define LOCALIPROC static void` macro is used ~100 times instead of
actual function signatures.  There is no class, no separation between
decode/execute/memory, and no way to unit-test any individual instruction.

**Impact:** any CPU bug requires navigating a 9K-line file of flat static
functions.  Decode, execute, and memory subsystems should be separate
compilation units.

### 3. ~~Massive platform code duplication~~ — RESOLVED

> **Status: Resolved.** All non-SDL backends have been removed. Only
> `platform/sdl.cpp` (5,261 lines) remains, plus shared code in
> `platform/common/`. The ~33K lines of duplicated platform code across
> Cocoa, Carbon, X11, GTK, Win32, DOS, NDS, and Classic Mac backends are gone.

**Impact:** No longer an issue. A behavioral change only needs to be made once
in `sdl.cpp` (or in `platform/common/` for shared logic).

### 4. Dead code cemetery — 294 `#if 0` blocks (72 in SCC alone)

There are **294 `#if 0` blocks** across the codebase.  `devices/scc.cpp`
alone has **72** — entire features that were never enabled, alternative
implementations, and half-finished ideas entombed in preprocessor conditionals.
Combined with 6,500 total preprocessor directives (many leftover `#ifdef`
guards from the original `#define`-driven build system), the actual logic is
buried under layers of dead conditional code.

**Impact:** developers can't tell what code actually runs.  The SCC — one of
the hardest devices to debug — is roughly 50% dead code by volume.

### 5. Address space mapping: magic numbers + copy-paste

The ATT (Address Translation Table) setup in `core/machine.cpp` uses raw hex
masks (`0xFF01E000`, `0x50000000 | 0x4000`, …) copy-pasted across
`SetUp_address_compact()`, `SetUp_address24()`, `SetUp_address32()`, and
model-specific variants — with no named constants for I/O space offsets.

The 220-line `MMDV_Access` switch dispatches on a raw `uint8_t` discriminator.
The `ATTer` struct uses raw `Device*` pointers and no type safety.

**Impact:** this is the central nervous system of the emulator — the thing
that maps addresses to devices — and it is the least readable, least safe code
in the project.  Most hard-to-diagnose "wrong device at wrong address" bugs
originate here.

### 6. `scc.cpp` — compile-time conditionals masquerading as runtime config

`devices/scc.cpp` has **316 `#if`/`#ifdef` directives** — more than one for
every ten lines of code.  The `Channel_Ty` struct alone has **93 `#if`/`#endif`
pairs**, most guarded by `SCC_TrackMore` (hardcoded to 0) or `EmLocalTalk`
(hardcoded to 0).  The struct's actual layout at compile time is tiny, but
reading it requires mentally evaluating dozens of dead branches.

This is the worst leftover of the original "one build per machine
configuration" design.  Since all models now share a single binary, these
should be runtime fields (present but unused when not needed) or factored into
a separate LocalTalk module.

**Impact:** the SCC is the #1 source of emulation bugs (serial, mouse on
compact Macs, printing).  The preprocessor maze makes it nearly impossible to
read, let alone debug.

### 7. Half-migrated architecture — four global singletons, two ownership models

The codebase is stuck between two architectures:

| Singleton | References | Purpose |
|-----------|----------:|---------|
| `g_machine` | 207 | God object, owns devices |
| `g_wires` | 92 | Inter-device signal bus |
| `g_ict` | 22 | Cycle-based task scheduler |
| `g_cpu` | 12 | CPU dispatch entry point |

These were extracted from raw globals into classes (good), but are still
global singletons (bad).  The `Machine` class owns `std::unique_ptr<uint8_t[]>
ram_` that is *never populated* — actual RAM comes from the platform arena
allocator and lives in the global `uint8_t * RAM`.  Wire change callbacks
do `g_machine->findDevice<RTCDevice>()` lookups on every signal transition
instead of holding direct pointers.

**Impact:** the codebase has all the complexity of *two* architectures but the
benefits of neither.  You can't reason about ownership because it's split
between the arena allocator and the `Machine` object.

### 8. `endian.h` — hand-rolled byte-swap with dead alternatives

`core/endian.h` (129 lines, 36 preprocessor directives) implements
big-endian memory access with multiple hand-rolled byte-swap strategies
guarded by `BigEndianUnaligned`, `LittleEndianUnaligned`, and
`HaveMySwapUi5r`.  Two of the `do_get_mem_long` strategies are inside
`#if 0` blocks with comments like *"no, this doesn't do well with apple
tools"* and *"better, though still doesn't use BSWAP instruction"*.

Modern compilers recognize `__builtin_bswap32` / `std::byteswap` (C++23)
and emit optimal `BSWAP`/`REV` instructions.  The entire file could be
replaced by ~15 lines using `<bit>` or compiler builtins.

**Impact:** this is the innermost hot path of the emulator — every memory
read and write goes through these macros.  The hand-rolled code is both
harder to read and potentially slower than what the compiler would generate
with a simple `__builtin_bswap32`.

### 9. `machine.cpp` — a 1,821-line junk drawer

`core/machine.cpp` mixes at least six unrelated concerns in one file:

- Address map setup (ATT table construction, ~400 lines)
- Device I/O dispatch (`MMDV_Access`, ~220 lines)
- Debug logging infrastructure (~100 lines)
- Extension/host-interface protocol (`Extn_Access`, ~200 lines)
- Memory transfer helpers (`PbufTransferVM`, disk I/O)
- Wire callback registration (~40 lines)

There are 62 functions with no grouping, no classes, and no clear boundaries.
The extension protocol alone (`ExtnParamBuffers_Access`,
`ExtnHostTextClipExchange_Access`, `ExtnFind_Access`, `Extn_Access`) is a
self-contained subsystem that should be its own file.

**Impact:** any change to address mapping risks breaking the extension
protocol, debug logging, or disk I/O — because they all live in the same
scope with access to the same static variables.

### 10. No tests, no assertions, no safety net

There are **zero unit tests** anywhere in the project.  The only validation
is the `selftest.sh` script that boots a known ROM and compares trace output.
There are no assertions beyond `ReportAbnormalID` (which swallows all errors
after the first one).  The `Device` base class has no contract enforcement —
a device that doesn't implement `access()` will silently return the input data.

Combined with the global state and half-migrated architecture, this means
every refactoring is a high-wire act: you can't know if you've broken
something until you boot a ROM and manually look for regressions.

**Impact:** the single biggest barrier to cleaning up anything else on this
list.  Without tests, every change is a risk, which disincentivizes
refactoring, which lets the debt compound.

---

* Unknown model on the command line should be a hard error
