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

### 3. Massive platform code duplication (~38K lines across 6 backends)

| File | Lines |
|------|------:|
| `platform/win32.cpp` | 6,299 |
| `platform/x11.cpp` | 5,754 |
| `platform/carbon.cpp` | 5,669 |
| `platform/classic_mac.cpp` | 5,601 |
| `platform/cocoa.mm` | 5,364 |
| `platform/sdl.cpp` | 5,261 |

Each re-implements the same ~13 lifecycle patterns: `AllocMyMemory`,
`LoadMacRom`, `InitOSGLU`, `CheckForSavedTasks`, screen scaling, keycode
tables, tick timing, etc.  Changes to common logic must be replicated across
all six files.

**Impact:** any behavioral change (e.g. a new disk insertion path) must be
patched in six places.  Bugs will exist in some backends but not others.

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

## The 20 Easiest "Most Buck for the Bang" Wins

Ordered roughly by effort (easiest first).

### 1. Replace `(void)` with `()` everywhere

*Effort: ~10 minutes with sed.  Fixes 1,745 instances.*

```
find src/ -name '*.cpp' -o -name '*.h' -o -name '*.mm' | xargs sed -i '' 's/(void)/()/g'
```

A C-ism unnecessary in C++.  Trivial, zero risk, instant modernization signal.

### 2. Delete the `unused/` directory

*Effort: ~1 minute.  Removes 2,686 lines.*

Four files (`LTOVRBPF.h`, `LTOVRUDP.h`, `SGLUALSA.h`, `SGLUDDSP.h`) sitting
in `src/unused/`.  Nothing includes them.  They're dead weight in the tree and
in searches.  `git rm -r src/unused/`.

### 3. Kill the `LOCALIPROC` macro

*Effort: ~15 minutes with sed.  Affects `m68k.cpp` (159 uses) and
`fpu_emdev.h` (9 uses).*

```
sed -i '' 's/LOCALIPROC/static void/g' src/cpu/m68k.cpp src/cpu/fpu_emdev.h
```

Then delete the `#define LOCALIPROC static void` line.  No behavior change.

### 4. Replace `Bit0`–`Bit7` defines with `(1 << n)` in `scc.cpp`

*Effort: ~15 minutes.  8 defines, ~120 use sites.*

```cpp
// Before
#define Bit0 1
#define Bit5 32
if (Data & Bit5) ...

// After
if (Data & (1 << 5)) ...
```

Or better: name the actual SCC register bits (`kSCC_RR0_RxCharAvail`,
`kSCC_RR0_TxBufEmpty`, etc.).  The `Bit0`–`Bit7` names tell you nothing about
the *meaning* of the bit.

### 5. Replace `Ui3rPowOf2` / `Ui3rTestBit` macros with plain C++

*Effort: ~15 minutes.  14 call sites in `via.cpp` / `via2.cpp`.*

```cpp
// Before
#define Ui3rPowOf2(p) (1 << (p))
#define Ui3rTestBit(i, p) (((i) & Ui3rPowOf2(p)) != 0)

// After — just inline
(i & (1 << p)) != 0
```

Legacy naming from the old type system (`Ui3r` = `uint8_t`).  The macros
obscure trivial bit operations.

### 6. Strip all `#if 0` dead code blocks

*Effort: ~1 hour.  Removes ~2,000+ lines of noise.*

A single grep pass plus manual review.  The 72 blocks in `scc.cpp` alone would
dramatically improve readability of the hardest device file.  If anything in
`#if 0` is ever needed again, it's in git history.  Pure noise removal — zero
behavioral change.

### 7. Replace `ui5r_From*` macros with `static_cast`

*Effort: ~1 hour.  147 call sites, 6 macro definitions in `machine.h`.*

```cpp
// Before
#define ui5r_FromSByte(x) ((uint32_t)(int32_t)(int8_t)(uint8_t)(x))
uint32_t r = ui5r_FromSByte(do_get_mem_byte(V_pc_p + 1));

// After
uint32_t r = static_cast<uint32_t>(static_cast<int8_t>(do_get_mem_byte(V_pc_p + 1)));
```

Or wrap in a properly-named `inline` function like `sign_extend_byte()`.
The `ui5r` prefix is from the dead type system and actively misleads.

### 8. Add `operator<=>`  to `MacModel` (or just `operator<`)

*Effort: ~30 minutes.  Eliminates 40 `static_cast<int>` comparisons.*

```cpp
// Before (used 40 times)
if (static_cast<int>(g_machine->config().model) <= static_cast<int>(MacModel::Plus))

// After
if (g_machine->config().model <= MacModel::Plus)
```

Since `MacModel` is `enum class MacModel : int`, just add a defaulted
comparison operator (C++20) or a free `operator<` that casts internally once.

### 9. Make `tMacErr` a proper `enum class`

*Effort: ~1–2 hours.  19 `#define` codes → enum members, ~440 use sites.*

```cpp
// Before
using tMacErr = uint16_t;
#define mnvm_noErr   ((tMacErr) 0x0000)
#define mnvm_eofErr  ((tMacErr) 0xFFD9)

// After
enum class tMacErr : uint16_t {
    noErr   = 0x0000,
    eofErr  = 0xFFD9,
    ...
};
```

The compiler will then catch sites where error codes are confused with
integers or used as booleans.

### 10. Merge `via.cpp` / `via2.cpp` into a single parameterized class

*Effort: ~2 hours.  Eliminates ~800 lines of duplication.*

The two files are **95% identical** — only ~144 lines differ, and those are
pure name substitutions (`VIA1` → `VIA2`, `kICT_VIA1_Timer1Check` →
`kICT_VIA2_Timer1Check`, etc.).  Make one `VIADevice` class that takes a VIA
number as a constructor parameter and uses it for ICT IDs and wire IDs.

### 11. Name the I/O address constants

*Effort: ~1–2 hours.  Huge readability gain in `machine.cpp`.*

```cpp
namespace io {
    constexpr uint32_t kBase  = 0x50000000;
    constexpr uint32_t kVIA1  = 0x00000;
    constexpr uint32_t kVIA2  = 0x02000;
    constexpr uint32_t kSCC   = 0x04000;
    constexpr uint32_t kDisk  = 0x08000;
    constexpr uint32_t kSCSI  = 0x10000;
    constexpr uint32_t kSound = 0x14000;
    constexpr uint32_t kIWM   = 0x16000;
}
```

Replace all raw hex in ATT setup.  Mechanical, safe, massive clarity boost
for the most confusing part of the codebase.

### 12. Convert `kATTA_*` bit-field `#defines` to `constexpr`

*Effort: ~30 minutes.  11 macros → named constants.*

```cpp
// Before (machine.h)
#define kATTA_readreadybit 0
#define kATTA_mmdvmask (1 << kATTA_mmdvbit)

// After
namespace ATT {
    constexpr uint32_t kReadReadyBit  = 0;
    constexpr uint32_t kReadReadyMask = 1 << kReadReadyBit;
    ...
}
```

### 13. Make `MyEvtQElKind` an `enum class`

*Effort: ~30 minutes.  4 `#define` constants, ~50 use sites.*

```cpp
// Before (platform.h)
#define MyEvtQElKindKey 0
#define MyEvtQElKindMouseButton 1
#define MyEvtQElKindMousePos 2
#define MyEvtQElKindMouseDelta 3

// After
enum class EvtKind : uint8_t {
    Key = 0,
    MouseButton = 1,
    MousePos = 2,
    MouseDelta = 3,
};
```

Also rename `MyEvtQEl` → `InputEvent` while you're there.  The `My` prefix
is a minivmac-ism that adds nothing.

### 14. Convert `MKC_*` keycode defines to a `constexpr` table or `enum`

*Effort: ~1 hour.  104 `#define` constants in `platform.h` lines 325–460.*

These are Mac virtual keycodes.  As `#defines` they pollute the global
namespace and can silently conflict.  Move to an `enum class MKC : uint8_t`
or a `constexpr` lookup.  The 104 values are just a flat mapping that's
trivial to convert.

### 15. Turn `screen_hack.h` / `hpmac_hack.h` into proper functions

*Effort: ~1 hour.  403 + 255 = 658 lines of `.h` files `#include`d as code.*

These headers are `#include`d inside functions in `rom.cpp` — they're code
fragments, not headers.  Convert each to a proper function in `rom.cpp` (or
its own `.cpp`).  This eliminates a confusing anti-pattern where headers
contain raw executable statements.

### 16. Promote `ReportAbnormalID` hex codes to a central `enum`

*Effort: ~2 hours.  276 call sites with raw hex IDs like `0x1108`, `0x074C`.*

Create a single `enum class AbnormalID : uint16_t` with named members:

```cpp
enum class AbnormalID : uint16_t {
    VIA1_NonStandardAddress = 0x1108,
    SCC_AutoEnables         = 0x0714,
    ...
};
```

Right now a collision between two IDs in different files would be invisible.
A central enum makes them searchable and unique.

### 17. Replace `label_N:` / `goto label_N` with loops

*Effort: ~1 hour.  6 instances across `machine.cpp` and `sony.cpp`.*

```cpp
// Before (machine.cpp:299)
label_1:
    if (0 == count) {
        result = mnvm_noErr;
    } else {
        ...
        goto label_1;
    }

// After
while (count != 0) {
    ...
}
result = mnvm_noErr;
```

These are all simple retry loops disguised as gotos.  6 easy conversions,
big readability win.

### 18. Remove the C-style `typedef struct` pattern

*Effort: ~30 minutes.  Affects `MyEvtQEl` and a handful of others.*

```cpp
// Before (platform.h)
struct MyEvtQEl { ... };
typedef struct MyEvtQEl MyEvtQEl;

// After — in C++ the typedef is redundant
struct InputEvent { ... };
```

### 19. Replace C-style casts with `static_cast` / `reinterpret_cast`

*Effort: ~2–3 hours.  183 C-style casts across the codebase.*

Most are `(uint8_t *)`, `(char *)`, `(void *)`.  Converting to explicit C++
casts makes intent clear (is this a reinterpret, a const_cast, or a
safe numeric conversion?) and makes them grep-able.  Can be done file by file.

### 20. Move the 104 `MKC_` keycodes out of `platform.h`

*Effort: ~30 minutes.  Zero behavioral change.*

`platform.h` is 461 lines, and 135 of those are Mac keycode `#defines`.
Move them to a dedicated `keycodes.h` (which already has a natural home
alongside `platform/common/alt_keys.h`).  This shrinks the mega-header
so that files which just need `tMacErr` or `vSonyTransfer` don't also
pull in keyboard constants they'll never use.
