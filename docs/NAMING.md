# Naming Conventions

This document defines the naming rules for the maxivmac codebase.
It captures the dominant patterns already in use and prescribes a single
consistent convention for each category. Legacy names inherited from
minivmac are listed as exceptions; new code must follow the rules below.

---

TODO: add info on NeXTstep-like naming: MacRomanFromUTF8, UTF8FromMacRoman (not UTF8ToMacRoman). Seems more logical x = y => x = XFromY( y ).

## Scope

These conventions apply to all code under `src/` **except**:

| Directory | Reason |
|-----------|--------|
| `src/macsrc/` | Classic Macintosh source — not part of the emulator |
| `src/cpu/` | Deeply intertwined m68k/FPU emulation code inherited from minivmac. Internal naming (e.g. `myfp_*`, `Bit32u`, `si6r`) is frozen; only the public API symbols that leak into other directories need to follow the conventions. |

---

## Files

| Element | Convention | Example |
|---------|-----------|---------|
| Source files | `snake_case.cpp` | `via_base.cpp`, `sdl_sound.cpp` |
| Header files | `snake_case.h` | `machine_config.h`, `wire_ids.h` |
| C++ headers | `snake_case.hpp` | `state_recorder.hpp` |

Source and header filenames are all-lowercase with underscores.
A `.cpp`/`.h` pair share the same stem (`sony.cpp` / `sony.h`).

---

## Types

### Classes and Structs

**PascalCase**, no prefix.

```cpp
class Rig;
class WireBus;
struct MachineConfig;
struct VIAConfig;
struct LaunchConfig;
```

Device classes append `Device` to the hardware name:

```cpp
class VIA1Device;     // not Via1Device
class SCCDevice;      // acronyms stay uppercase
class KeyboardDevice;
class SoundDevice;
```

Acronyms in type names keep their conventional casing (`VIA`, `SCC`,
`SCSI`, `ADB`, `ASC`, `IWM`, `PMU`, `RTC`, `ROM`, `FPU`, `MMU`).
When an acronym starts a name it stays uppercase: `SCCDevice`, `ADBDevice`.

### Enum Types

**PascalCase**, no prefix. Prefer `enum class`.

```cpp
enum class MacModel : int { ... };
enum class RecorderMode { ... };
enum class EvtQElKind { ... };
```

**Legacy exception:** `tMacErr` uses a `t` prefix because it mirrors
the original Mac OS error type. Do not create new `t`-prefixed types.

### Enum Values

Two styles, chosen by context:

| Context | Convention | Example |
|---------|-----------|---------|
| Scoped (`enum class`) | PascalCase or original Mac symbol | `MacModel::Plus`, `tMacErr::fnfErr` |
| Unscoped enum | `k` + PascalCase | `kLangEnglish`, `kKybdStateIdle` |

For unscoped enums the `k` prefix prevents global namespace collisions.
The convention follows Apple's classic Toolbox style.

Sentinel / count values use the plural of the enum name:

```cpp
kKybdStates   // count of KybdState values
kLangCount    // count of Language values
```

### Typedefs and Type Aliases

**Preferred:** `PascalCase` for application types.

```cpp
using MacModel_ut = std::underlying_type_t<MacModel>;
```

**Legacy exceptions** (do not extend these patterns):

| Pattern | Example | Origin |
|---------|---------|--------|
| `tXxx` prefix | `tMacErr`, `tPbuf`, `tDrive` | minivmac |
| Lowercase abbreviation | `si6r`, `ui6r`, `si6b`, `ui6b` | minivmac integer width aliases |
| SoftFloat types | `floatx80`, `float128`, `flag` | Third-party SoftFloat library |
| Bochs types | `Bit32u`, `Bit64s` | Third-party Bochs FPU code |

New type aliases must use PascalCase: `DriveIndex`, `ErrorCode`, etc.

---

## Functions

### Class Methods

**camelCase**, starting with a lowercase letter.

```cpp
void reset();
void zap();
uint32_t access(uint32_t data, bool writeMem, uint32_t addr);
const char* name() const;
void dataLineChngNtfy();
void receiveEndCommand();
bool interruptsEnabled() const;
```

Short verb-first names are preferred: `reset()`, `update()`, `access()`.

### Free Functions — Modern Style

**PascalCase**, matching the module context.

```cpp
void ProgramMain();
void ProgramEarlyInit(int argc, char* argv[]);
bool EmulationReserveAlloc();
void DoneWithDrawingForTick();
void WaitForNextTick();
```

### Free Functions — Module-Prefixed (Legacy)

`Module_PascalCase` with an underscore separator. Found in flat C-style
subsystems that predate the class hierarchy.

```cpp
void MySound_Start();      // My = platform glue
void MySound_Init();
void Extn_Reset();
void Memory_Reset();
void ICT_Zap();
void Keyboard_UpdateKeyMap2();
```

**Phasing out:** When a subsystem is converted to a class, drop the
`Module_` prefix and use camelCase methods on the class instead.

**The `My` prefix** (`MyMoveBytes`, `MySound_*`, `MyMoveMouse`) is a
minivmac holdover meaning "our implementation of". Do not introduce new
`My`-prefixed names. When refactoring, drop the prefix.

### Free Functions — CPU/FPU Internals

Low-level CPU code uses `module_camelCase`:

```cpp
void m68k_reset();
void MINEM68K_Init();
void myfp_Add(myfpr *r, ...);
void myfp_IsNan(myfpr *x);
```

These follow the style of the original minivmac CPU emulation and
SoftFloat library. Changing them is not a priority because the code
is rarely modified.

### Debug / Logging Functions

`dbglog_camelCase` — an accepted convention, not legacy. The `dbglog_`
prefix acts as a namespace and is consistent throughout the codebase.
New debug logging functions should follow the same pattern.

```cpp
void dbglog_writeCStr(char *s);
void dbglog_writeReturn();
void dbglog_writeHex(uint32_t x);
```

---

## Variables

### Member Variables

**camelCase with trailing underscore.**

```cpp
Machine* machine_ = nullptr;
KybdState kybdState_ = kKybdStateIdle;
bool haveKeyBoardResult_ = false;
uint8_t keyBoardResult_ = 0;
int inquiryCommandTimer_ = 0;
uint8_t soundInvertState_ = 0;
```

The trailing underscore distinguishes members from locals and parameters.
This convention is consistent throughout the device and core classes.

### Global Variables

**`g_` prefix + camelCase.**

```cpp
extern Rig* g_rig;
extern CPU g_cpu;
```

**Legacy exceptions** (bare globals from minivmac):

```cpp
extern uint8_t* ROM;
extern uint8_t* RAM;
extern bool ROM_loaded;
extern bool RequestMacOff;
```

Do not create new bare globals. Prefix with `g_`.

### Static File-Scope Variables

**`s_` prefix + camelCase.** The `s_` prefix distinguishes file-scope
statics from locals at a glance and mirrors the `g_` convention for
globals.

```cpp
static bool s_useFullScreen;
static bool s_useMagnify;
static bool s_curSpeedStopped;
static int  s_windowScale;
static bool s_haveCursorHidden;
static bool s_wantCursorHidden;
```

**Legacy exceptions** (do not extend these patterns):

```cpp
static bool UseFullScreen;     // legacy PascalCase — prefer s_ prefix
static bool gBackgroundFlag;   // legacy g prefix
static char *d_arg;            // legacy argument pointer
static char *pref_dir;         // legacy
```

New file-scope statics must use `s_camelCase`.

### Local Variables

**camelCase**, short and descriptive.

```cpp
int byteCount;
bool silentfail;
uint32_t addr;
```

Single-letter names are acceptable for tight loops (`i`, `n`, `v`, `r`).

### Parameters

**camelCase**, same as local variables.

```cpp
void access(uint32_t data, bool writeMem, uint32_t addr);
bool AllocBlock(uint8_t **p, uint32_t n, bool FillOnes);
```

---

## Constants and Macros

### Compile-Time Config Macros

**UPPER_SNAKE_CASE.**

```cpp
#define USE_68020 1
#define EM_FPU 1
#define WANT_DISASM 1
#define WANT_CYC_BY_PRI_OP 1
#define INCLUDE_EXTN_PBUFS 1
```

Module-scoped config macros use `MODULE_UPPER_SNAKE_CASE`:

```cpp
#define SONY_SUPPORT_DC42 1
#define SONY_WANT_CHECKSUMS_UPDATED 1
```

**Legacy exception:** many existing macros use PascalCase (`Use68020`,
`EmFPU`, `WantDisasm`, `Sony_SupportDC42`). These will be migrated
incrementally. New macros must use `UPPER_SNAKE_CASE`.

### Feature-Test Macros

**UPPER_SNAKE_CASE**, often `HAVE_` or `ENABLE_` prefix:

```cpp
#define HAVE_ASR 0
#define ENABLE_FS_MOUSE_MOTION 1
#define ENABLE_RECREATE_W 1
```

### Named Constants (constexpr / const)

**`k` prefix + PascalCase** for numeric constants:

```cpp
constexpr int kKeepMaskControl  = (1 << 0);
constexpr int kKeepMaskCapsLock = (1 << 1);
```

### Utility Macros

**UPPER_SNAKE_CASE** for all macros, including function-like ones:

```cpp
#define POW_OF_2(p)  ((uint32_t)1 << (p))
#define POW2_MASK(p) (POW_OF_2(p) - 1)
#define UNUSED(p)    (void)(p)
```

**Legacy exception:** existing PascalCase macros (`PowOf2`, `Pow2Mask`,
`UnusedParam`) will be migrated incrementally.

### Wire Enum Values

`Wire_` prefix + signal name in PascalCase:

```cpp
Wire_SoundDisable,
Wire_VIA1_iA0,
Wire_VIA2_InterruptRequest,
Wire_MemOverlay,
```

The underscore after `Wire` and after `VIA1`/`VIA2` acts as a namespace
separator. Pin identifiers (`iA0`, `iB3`, `iCB2`) preserve hardware
data-sheet notation.

---

## Booleans

### Have/Want Pattern

State variables that track current vs. desired state use the
**Have/Want** pair:

```cpp
static bool s_haveCursorHidden;
static bool s_wantCursorHidden;
```

This pattern makes toggling logic clear: "set want, then reconcile
have to match."

### Config Booleans

Config struct booleans use the `em` prefix (short for "emulate") for
device-enable flags:

```cpp
bool emVIA1 = true;
bool emADB  = true;
bool emFPU  = true;
bool emMMU  = false;
```

Other config booleans use a descriptive `use` or `include` prefix:

```cpp
bool use68020 = true;
bool includeVidMem = true;
```

### Boolean Functions

Predicate methods should read as questions — `is`, `has`, `can`,
or a verb phrase returning a truth value:

```cpp
bool interruptsEnabled() const;
bool attemptToFinishInquiry();
bool AnyDiskInserted();
```

---

## Summary Table

| Element | Convention | Example |
|---------|-----------|---------|
| File names | `snake_case` | `via_base.cpp` |
| Classes / Structs | `PascalCase` | `MachineConfig` |
| Device classes | `AcronymDevice` | `VIA1Device` |
| Enum types | `PascalCase` (`enum class`) | `MacModel` |
| Enum values (scoped) | PascalCase | `MacModel::Plus` |
| Enum values (unscoped) | `kPascalCase` | `kLangEnglish` |
| Class methods | `camelCase` | `dataLineChngNtfy()` |
| Free functions | `PascalCase` | `ProgramMain()` |
| Module free functions | `Module_PascalCase` | `Extn_Reset()` |
| Member variables | `camelCase_` (trailing `_`) | `kybdState_` |
| Global variables | `g_camelCase` | `g_rig` |
| File-scope statics | `s_camelCase` | `s_useFullScreen` |
| Local variables | `camelCase` | `byteCount` |
| Parameters | `camelCase` | `writeMem` |
| Config macros | `UPPER_SNAKE_CASE` | `WANT_DISASM` |
| Constants | `kPascalCase` | `kKeepMaskControl` |
| Utility macros | `UPPER_SNAKE_CASE` | `POW_OF_2()` |
| Wire IDs | `Wire_SignalName` | `Wire_SoundDisable` |
| Config booleans | `emXxx` / `useXxx` | `emFPU`, `use68020` |
| Boolean state pairs | `s_haveXxx` / `s_wantXxx` | `s_haveCursorHidden` |

---

## Legacy Exceptions (Do Not Extend)

These patterns exist in the codebase but should not be used for new code:

| Pattern | Examples | Reason |
|---------|---------|--------|
| `t` prefix on types | `tMacErr`, `tPbuf` | minivmac convention |
| `My` prefix on functions | `MyMoveBytes`, `MySound_*` | minivmac "our implementation" |
| `mnvm_` prefix | `mnvm_noErr`, `mnvm_miscErr` | Backward-compat aliases |
| Bare globals | `ROM`, `RAM`, `ROM_loaded` | Pre-class legacy |
| Abbreviated type aliases | `si6r`, `ui6r`, `ui6b` | minivmac integer types |
| `myfp_` / `myfpr` | `myfp_Add`, `myfp_IsNan` | FPU math library wrapper (cpu/ only) |
| `Bit32u` / `Bit64s` | Bochs FPU types | Third-party code (cpu/ only) |
| PascalCase file-scope statics | `UseFullScreen`, `CurSpeedStopped` | minivmac convention |
| PascalCase macros | `Use68020`, `PowOf2` | minivmac convention |

When touching legacy code, rename incrementally toward the rules above
when the change is low-risk and doesn't affect the reference test suite.
