# Model and Macintosh — Detailed Design

Implements the specification in [MODEL.md](MODEL.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

The feature touches three areas: a new `src/config/` directory for
`.mac` file parsing, additions to the existing `src/core/` for the
Model definition refactoring, and the Data Directory layout on disk.

```
src/config/
    mac_file.h          # Public header — MacFileEntry struct, parser API
    mac_file.cpp         # .mac file parser and validator

src/core/
    machine_config.h     # (existing) MacModel enum, MachineConfig struct
    machine_config.cpp   # (existing) MachineConfigForModel() — refactored
                         #   to use constexpr ModelDef table
    model_defs.h         # NEW: constexpr ModelDef table — authoritative
                         #   hardware facts for all 12 models
    config_loader.h      # (existing) LaunchConfig, ParseModelName(), etc.
    config_loader.cpp    # (existing) BuildMachineConfig() — updated to
                         #   accept Macintosh overrides
    main.cpp             # (existing) boot path changes

src/platform/
    imgui_model_selector.cpp   # (existing) replaced by Launcher
    imgui_launcher.h           # NEW: Launcher UI (replaces model selector)
    imgui_launcher.cpp         # NEW: scans data/macs/, renders cards
    app_main.cpp               # (existing) boot path changes

data/                   # NEW: sibling of the binary
    roms/               # ROM files (MacPlus.ROM, MacII.ROM, ...)
    disks/              # Boot disk images (plus-608.hfs, macii-7.hfs, ...)
    macs/               # Macintosh definitions (.mac files)
    shared/             # Default shared drive content
    system/             # Tool disk + INIT installer
    debug/              # Debugger .def files (moved from assets/)
```

The `src/config/` directory is new and dedicated to file-format
parsers.  It has no dependency on platform or device code — only on
`src/core/machine_config.h` for the `MacModel` enum.

---

## 2. Public Interface

### 2.1 ModelDef — Constexpr Model Table

```cpp
// src/core/model_defs.h
#pragma once
#include "core/machine_config.h"
#include <array>
#include <cstdint>
#include <string_view>

struct RomDef
{
    std::string_view filename;    // e.g. "MacPlus.ROM"
    uint32_t size;                // expected ROM size in bytes
    uint32_t base;                // base address in guest memory map
    std::string_view md5;         // expected MD5 hex string (lowercase)
};

struct ScreenDef
{
    uint16_t width;
    uint16_t height;
    uint8_t depth;                // log₂ bpp: 0=1bpp, 3=8bpp
};

struct ModelDef
{
    MacModel id;
    std::string_view name;        // e.g. "Macintosh Plus"
    std::string_view slug;        // e.g. "Plus" — CLI / .mac file key

    // CPU
    bool use68020;
    bool emFPU;
    bool emMMU;

    // Memory
    uint32_t ramASize;
    uint32_t ramBSize;            // 0 for single-bank models

    // ROM
    RomDef rom;

    // Screen
    ScreenDef screen;

    // Devices
    bool emVIA1;
    bool emVIA2;
    bool emADB;
    bool emClassicKbrd;
    bool emRTC;
    bool emPMU;
    bool emASC;
    bool emClassicSnd;
    bool emVidCard;
    bool includeVidMem;
    uint32_t vidMemSize;
    uint32_t vidROMSize;

    // Extension space
    uint32_t extnBlockBase;
    uint8_t extnLn2Spc = 7;

    // Timing
    int clockMult;
    int autoSlowSubTicks;
    int autoSlowTime;
    int maxATTListN;
};

// Authoritative table — one entry per MacModel value.
inline constexpr std::array<ModelDef, 12> kModelDefs = {{ ... }};

// Lookup by MacModel enum.  Asserts if not found — every enum value
// must have an entry.  A missing entry is a program error.
constexpr const ModelDef *ModelDefFor(MacModel model);

// Lookup by slug (case-insensitive at call site — caller lowercases).
const ModelDef *ModelDefForSlug(std::string_view slug);
```

### 2.2 MacFileEntry — Parsed .mac File

```cpp
// src/config/mac_file.h
#pragma once
#include "core/machine_config.h"
#include <string>
#include <string_view>
#include <vector>

struct MacFileEntry
{
    // Identity
    std::string name;             // user-facing label
    std::string description;      // optional longer text
    std::string filePath;         // absolute path to the .mac file

    // Model
    MacModel model;

    // Disks (first = boot disk)
    std::vector<std::string> disks;

    // Shared drives
    std::vector<std::string> sharedDirs;

    // Serial
    std::string serialA;

    // Overrides (0 = use model default)
    uint32_t ramOverrideMB = 0;
    uint16_t screenW = 0;
    uint16_t screenH = 0;
    uint8_t screenDepth = 0;

    // Validation status (populated by ValidateMacEntry)
    bool romAvailable = false;
    std::string romPath;          // resolved path to ROM file
    bool allDisksAvailable = false;
    std::string validationError;  // human-readable reason if invalid
};

// Parse a single .mac file.  Returns true on parse success (even if
// the referenced ROM/disks are missing — that's a validation issue,
// not a parse error).  On parse failure, sets errorOut.
bool ParseMacFile(std::string_view path, MacFileEntry &out,
                  std::string &errorOut);

// Scan a directory for .mac files, parse each, return all entries.
// Entries with parse errors are silently skipped (logged via DIAG).
std::vector<MacFileEntry> ScanMacDirectory(std::string_view dirPath);

// Validate ROM and disk availability for a single entry.
// Populates romAvailable, allDisksAvailable, and validationError.
void ValidateMacEntry(MacFileEntry &entry, std::string_view romDir,
                      std::string_view diskDir);
```

### 2.3 Data Directory Resolution

```cpp
// src/config/mac_file.h (continued)

// Resolve the data/ directory.  Searches:
//   1. <appParent>/data/
//   2. CWD/data/
// Returns empty string if not found.
std::string ResolveDataDir(std::string_view appParent);
```

---

## 3. Integration Points

### 3.1 MachineConfigForModel() Refactoring

**File:** [src/core/machine_config.cpp](../../../src/core/machine_config.cpp)

The current 400-line switch statement is replaced by a table lookup
into `kModelDefs`.  The function reads the `ModelDef` and populates
a `MachineConfig`, then calls the existing `MakeVIA*Config_*()` helper
functions for VIA wiring (which remain unchanged — they encode port
wiring logic that doesn't belong in declarative data).

```cpp
MachineConfig MachineConfigForModel(MacModel model)
{
    const ModelDef *def = ModelDefFor(model);
    MachineConfig c;
    c.model = def->id;
    c.use68020 = def->use68020;
    c.emFPU = def->emFPU;
    c.emMMU = def->emMMU;
    c.ramASize = def->ramASize;
    c.ramBSize = def->ramBSize;
    c.romSize = def->rom.size;
    c.romBase = def->rom.base;
    c.romFileName = def->rom.filename.data();
    c.extnBlockBase = def->extnBlockBase;
    c.extnLn2Spc = def->extnLn2Spc;
    c.emVIA1 = def->emVIA1;
    c.emVIA2 = def->emVIA2;
    c.emADB = def->emADB;
    c.emClassicKbrd = def->emClassicKbrd;
    c.emRTC = def->emRTC;
    c.emPMU = def->emPMU;
    c.emASC = def->emASC;
    c.emClassicSnd = def->emClassicSnd;
    c.emVidCard = def->emVidCard;
    c.includeVidMem = def->includeVidMem;
    c.vidMemSize = def->vidMemSize;
    c.vidROMSize = def->vidROMSize;
    c.clockMult = def->clockMult;
    c.autoSlowSubTicks = def->autoSlowSubTicks;
    c.autoSlowTime = def->autoSlowTime;
    c.maxATTListN = def->maxATTListN;
    c.screenWidth = def->screen.width;
    c.screenHeight = def->screen.height;
    c.screenDepth = def->screen.depth;

    // VIA wiring — still procedural (port mapping logic)
    c.via1Config = MakeVIA1ConfigFor(model);
    c.via2Config = MakeVIA2ConfigFor(model);
    return c;
}
```

The existing `MakeVIA1Config_Plus()`, `MakeVIA1Config_SE()`, etc. are
unified into a single `MakeVIA1ConfigFor(MacModel)` dispatcher (plus
the corresponding VIA2 variant).  These stay procedural because the
wire mapping arrays don't compress well into constexpr data.

**Cost:** Zero — same work as before, just table-driven.

### 3.2 ParseModelName() Simplification

**File:** [src/core/config_loader.cpp](../../../src/core/config_loader.cpp), line 23

The current 80-line if-chain becomes a loop over `kModelDefs`:

```cpp
bool ParseModelName(const std::string &name, MacModel &out)
{
    std::string lower = toLower(name);
    for (const auto &def : kModelDefs)
    {
        if (toLower(std::string(def.slug)) == lower ||
            toLower(std::string(ModelToString(def.id))) == lower)
        {
            out = def.id;
            return true;
        }
    }
    return false;
}
```

### 3.3 ModelToString() Simplification

**File:** [src/core/config_loader.cpp](../../../src/core/config_loader.cpp), line 624

Becomes a table lookup.  No fallback — a missing entry is a bug:

```cpp
const char *ModelToString(MacModel model)
{
    const ModelDef *def = ModelDefFor(model);
    assert(def && "ModelToString: no ModelDef for model");
    return def->name.data();
}
```

### 3.4 Boot Path — New Launcher Flow

**File:** [src/platform/app_main.cpp](../../../src/platform/app_main.cpp)

The current flow is:

```
no --model → ModelSelector → SetLaunchConfig → Rig → boot
```

The new flow is:

```
no --model → ResolveDataDir → ScanMacDirectory → ValidateMacEntry(each)
           → Launcher shows cards → user clicks → SetLaunchConfig → Rig → boot
```

Insertion point: replace `imguiBackend.createSelectorWindow()` with
`imguiBackend.createLauncher(macEntries)`.

```cpp
// app_main.cpp — updated no-model path
std::string dataDir = ResolveDataDir(appParent);
auto entries = ScanMacDirectory(dataDir + "/macs");
std::string romDir = dataDir + "/roms";
std::string diskDir = dataDir + "/disks";
for (auto &e : entries)
    ValidateMacEntry(e, romDir, diskDir);
imguiBackend.createLauncher(std::move(entries));
imguiBackend.setUIState(UIState::Launcher);
```

### 3.5 Boot Path — Direct .mac File Launch

**File:** [src/platform/app_main.cpp](../../../src/platform/app_main.cpp)

When the user launches `maxivmac foo.mac`, parse the file directly and
boot without showing the Launcher.  Insertion in the existing
`modelExplicit` branch:

```cpp
// Detect .mac file argument
if (argc >= 2 && HasSuffix(argv[1], ".mac"))
{
    MacFileEntry entry;
    std::string err;
    if (!ParseMacFile(argv[1], entry, err))
    {
        fprintf(stderr, "Error: %s\n", err.c_str());
        return 1;
    }
    ValidateMacEntry(entry, romDir, diskDir);
    if (!entry.romAvailable)
    {
        fprintf(stderr, "Error: %s\n", entry.validationError.c_str());
        return 1;
    }
    // Convert MacFileEntry → LaunchConfig
    LaunchConfig lc = LaunchConfigFromMacEntry(entry, dataDir);
    SetLaunchConfig(lc);
    // ... proceed to boot
}
```

### 3.6 LaunchConfig Adapter

**File:** [src/core/config_loader.cpp](../../../src/core/config_loader.cpp)

New function that converts a `MacFileEntry` into a `LaunchConfig`:

```cpp
LaunchConfig LaunchConfigFromMacEntry(const MacFileEntry &entry,
                                      std::string_view dataDir)
{
    LaunchConfig lc;
    lc.model = entry.model;
    lc.modelExplicit = true;
    lc.romPath = entry.romPath;
    lc.ramMB = entry.ramOverrideMB;
    lc.screenW = entry.screenW;
    lc.screenH = entry.screenH;
    lc.screenDepth = entry.screenDepth;
    lc.serialA = entry.serialA;

    std::string diskDir = std::string(dataDir) + "/disks/";
    for (const auto &d : entry.disks)
        lc.diskPaths.push_back(diskDir + d);

    for (const auto &s : entry.sharedDirs)
    {
        if (!s.empty() && s[0] == '/')
            lc.sharedDirs.push_back(s);
        else
            lc.sharedDirs.push_back(std::string(dataDir) + "/" + s);
    }

    return lc;
}
```

### 3.7 ROM Validation with MD5

**File:** [src/platform/common/rom_loader.cpp](../../../src/platform/common/rom_loader.cpp)

The existing `ROM_IsValid()` performs a basic checksum check.
`ValidateMacEntry()` needs a lighter pre-boot check: file exists and
MD5 matches.  This uses the existing ROM search logic plus an MD5
computation:

The project already has a public-domain MD5 implementation in
[src/core/md5.h](../../../src/core/md5.h) — `md5_file()` computes the
hash of a file, and `md5_buf()` hashes an in-memory buffer.  Already
used by `main.cpp` to hash ROMs and disks for state recorder headers.
`ValidateMacEntry()` calls `md5_file()` directly — no new code needed.

```cpp
// src/config/mac_file.cpp  (inside ValidateMacEntry)
uint8_t digest[16];
if (!md5_file(romPath.c_str(), digest))
{
    entry.validationError = "ROM file unreadable";
    return;
}
if (std::memcmp(digest, expectedMD5, 16) != 0)
{
    entry.validationError = "ROM checksum mismatch";
    return;
}
```

The `ModelDef::rom.md5` field stores the expected hash as a hex string.
At validation time, the hex string is decoded to 16 bytes for comparison.

**Cost:** One file read per ROM at Launcher display time.  With 2
bundled ROMs this is negligible (~256 KB each).

### 3.8 Model Selector Removal

**File:** [src/platform/imgui_model_selector.cpp](../../../src/platform/imgui_model_selector.cpp)

The existing `ModelSelector` class is deleted entirely:

- `imgui_model_selector.cpp` and `imgui_model_selector.h` are removed.
- The `kModelTable` metadata (display names, CPU strings, RAM strings)
  and `kRAMTable` are not migrated — they duplicate data now in
  `ModelDef`.
- **The config panel (RAM picker, speed picker, disk slots) is
  deleted.**  The Launcher goes straight from card click to boot.
  The `.mac` file defines everything needed.  If the user wants
  different RAM or a different disk, they edit the `.mac` file or
  create a new one.  No intermediate configuration step.

---

## 4. Internal State

### 4.1 .mac File Parser State

The parser is stateless — it reads a file, returns a struct.  No global
or persistent state.

```cpp
// Internal to mac_file.cpp
struct ParseContext
{
    std::string_view path;
    int lineNum = 0;
    MacFileEntry entry;
};
```

### 4.2 Launcher State

The Launcher holds a vector of `MacFileEntry` structs populated at
startup.  It does not own any emulation state.

```cpp
// src/platform/imgui_launcher.h
#pragma once
#include "config/mac_file.h"
#include <vector>

class Launcher
{
public:
    void init(std::vector<MacFileEntry> entries);
    // Returns the selected entry, or nullptr if no selection yet.
    const MacFileEntry *draw();

private:
    std::vector<MacFileEntry> entries_;
    int selectedIndex_ = -1;
};
```

---

## 5. Key Algorithms

### 5.1 .mac File Parsing

```
Open file at path
For each line:
    Strip comments (# to end of line)
    Strip leading/trailing whitespace
    Skip empty lines
    Split on first '=' → key, value
    Trim key and value
    Switch on key:
        "name"        → entry.name = value
        "description" → entry.description = value
        "model"       → ParseModelName(value) → entry.model
        "disk"        → entry.disks.push_back(value)
        "shared"      → entry.sharedDirs.push_back(value)
        "serial-a"    → entry.serialA = value
        "ram"          → ParseRamSize(value) → entry.ramOverrideMB
        "screen"       → ParseScreenSpec(value) → W, H, depth
        unknown       → error: "unknown key '<key>' on line N"
Return entry
```

### 5.2 RAM Size Parsing

Accepts `"4M"`, `"2560K"`, `"8M"`.  Suffix is case-insensitive.

```
Remove trailing whitespace
If ends with 'M' or 'm':
    Parse leading digits → megabytes
    entry.ramOverrideMB = megabytes
If ends with 'K' or 'k':
    Parse leading digits → kilobytes
    entry.ramOverrideMB = 0  (store raw KB for sub-MB values)
    entry.ramOverrideKB = kilobytes
```

Given that `LaunchConfig.ramMB` is `uint32_t` and sub-MB values are
rare (only 128K and 512K models, which don't need overrides), we
handle the sub-MB case by storing raw bytes in a new field or rounding.
For v1.0, only MB granularity is supported — sub-MB values in `.mac`
files produce a parse error.

### 5.3 Screen Spec Parsing

Accepts `"640x480x8"`.

```
Split on 'x' → tokens[0], tokens[1], tokens[2]
width = parse(tokens[0])
height = parse(tokens[1])
depth = parse(tokens[2])     // bpp: 1, 2, 4, 8, 16, 32
screenDepth = log₂(depth)    // convert to log₂ form
```

### 5.4 Data Directory Resolution

```
Check <appParent>/data/ exists → return it
Check CWD/data/ exists → return it
Return ""
```

The binary's location (`appParent`) is the primary search path.  This
means the distribution archive can be unzipped anywhere and it works —
`data/` is always next to the binary.

### 5.5 Launcher Card Rendering

Each card shows:

- Name (from `.mac` file)
- Model info (from `ModelDef`: CPU, default RAM)
- Boot disk name(s)
- Status: green checkmark (bootable), grey with reason (ROM missing,
  disk missing, etc.)

Valid cards are clickable — one click boots immediately (no config
panel, no extra steps).  Invalid cards are greyed out with the
validation error displayed as a tooltip or subtitle.

---

## 6. Reused Infrastructure

| Existing API / Data | Where | How Used |
|---------------------|-------|----------|
| `MacModel` enum | `machine_config.h` | Model identity in `ModelDef` and `MacFileEntry` |
| `MachineConfig` struct | `machine_config.h` | Remains the runtime config consumed by `Rig` |
| `MachineConfigForModel()` | `machine_config.cpp` | Refactored to read from `kModelDefs` table |
| `BuildMachineConfig()` | `config_loader.cpp` | Unchanged — applies overrides from `LaunchConfig` |
| `ParseModelName()` | `config_loader.cpp` | Simplified to loop over `kModelDefs` |
| `ModelToString()` | `config_loader.cpp` | Simplified to table lookup |
| `ResolveRomPath()` | `config_loader.cpp` | Used by `ValidateMacEntry()` to find ROM files |
| `SetLaunchConfig()` | `main.cpp` | Called by Launcher when user clicks a card |
| `LaunchConfig` struct | `config_loader.h` | Populated from `MacFileEntry` via adapter |
| VIA config helpers | `machine_config.cpp` | `MakeVIA1Config_*()` / `MakeVIA2Config_*()` — unchanged |
| `kModelTable` metadata | `imgui_model_selector.cpp` | Replaced by `kModelDefs` — not duplicated |
| `Rig` class | `rig.h` | No changes — still consumes `MachineConfig` |
| `ChildPath()` | `path_utils.h` | Used for path construction in data directory resolution |

Nothing is duplicated.  The `ModelDef` table becomes the single source
of truth for hardware facts.  The existing `MachineConfig` struct
remains the runtime representation — `ModelDef` populates it.

---

## 7. Build Integration

### CMakeLists.txt Changes

Add new source files to the existing source list:

```cmake
# New files
set(CONFIG_SOURCES
    src/config/mac_file.cpp
)

set(CORE_SOURCES
    # ... existing ...
    src/core/model_defs.h          # header-only (constexpr table)
)

set(PLATFORM_SOURCES
    # ... existing ...
    src/platform/imgui_launcher.cpp
    # Remove: src/platform/imgui_model_selector.cpp
)
```

### Dependencies

- `src/config/mac_file.cpp` depends on: `core/machine_config.h`,
  `core/model_defs.h`, `core/config_loader.h` (for `ParseModelName`)
- `src/platform/imgui_launcher.cpp` depends on: `config/mac_file.h`,
  `core/model_defs.h`, ImGui
- `src/core/model_defs.h` depends on: `core/machine_config.h` only

No new external library dependencies.  MD5 computation uses the
existing `md5_file()` / `md5_buf()` in
[src/core/md5.h](../../../src/core/md5.h) (public domain, header-only).

---

## 8. Dependency Diagram

```
                     ┌──────────────┐
                     │  model_defs.h │  (constexpr table)
                     └──────┬───────┘
                            │
               ┌────────────┴────────────┐
               │                         │
    ┌──────────▼─────────┐    ┌──────────▼──────────┐
    │ machine_config.cpp │    │   mac_file.cpp       │
    │ (MachineConfigFor  │    │   (ParseMacFile,     │
    │  Model refactored) │    │    ValidateMacEntry) │
    └──────────┬─────────┘    └──────────┬───────────┘
               │                         │
    ┌──────────▼─────────┐    ┌──────────▼───────────┐
    │ config_loader.cpp  │    │ imgui_launcher.cpp   │
    │ (BuildMachineConfig│    │ (Launcher UI)        │
    │  unchanged)        │    └──────────┬───────────┘
    └──────────┬─────────┘               │
               │              ┌──────────▼───────────┐
               └──────────────► app_main.cpp         │
                              │ (boot path)          │
                              └──────────┬───────────┘
                                         │
                              ┌──────────▼───────────┐
                              │ main.cpp             │
                              │ (SetLaunchConfig→Rig)│
                              └──────────────────────┘
```

All arrows point downward.  No circular dependencies.  The
`model_defs.h` header is the root — everything flows from it.

---

## 9. Data Directory Bundling

### Distribution Archive Layout

```
maxivmac-v1.0-macos/
    maxivmac              (binary)
    data/
        roms/
            MacPlus.ROM
            MacII.ROM
        disks/
            plus-608.hfs
            macii-7.hfs
        macs/
            plus-608.mac
            macii-7.mac
        shared/
            (default shared content)
        system/
            tool.hfs      (tool disk with INIT installer)
        debug/
            traps.def
            globals.def
            types.def
            typemap.def
            errors.def
```

The `assets/` directory currently holding `.def` files is migrated to
`data/debug/`.  The binary's `InitEmulation()` path for loading `.def`
files is updated to look in `<dataDir>/debug/` instead of `assets/`.

### Bundled .mac Files

```
# data/macs/plus-608.mac
name = Mac Plus · System 6
description = The classic compact Macintosh experience.
model = Plus
disk = plus-608.hfs
shared = shared/
```

```
# data/macs/macii-7.mac
name = Mac II · System 7
description = Color Macintosh with networking.
model = II
disk = macii-7.hfs
shared = shared/
serial-a = slip
```

---

## 10. Testing

### Unit Tests

| Test | File | Framework |
|------|------|-----------|
| .mac parser: valid file | `test/config/mac_file_test.cpp` | doctest |
| .mac parser: missing required key | `test/config/mac_file_test.cpp` | doctest |
| .mac parser: unknown key (fail fast) | `test/config/mac_file_test.cpp` | doctest |
| .mac parser: repeatable keys (disk, shared) | `test/config/mac_file_test.cpp` | doctest |
| .mac parser: RAM size parsing (4M, 2560K) | `test/config/mac_file_test.cpp` | doctest |
| .mac parser: screen spec parsing (640x480x8) | `test/config/mac_file_test.cpp` | doctest |
| .mac parser: comments and blank lines | `test/config/mac_file_test.cpp` | doctest |
| ModelDef table completeness (all 12 models) | `test/core/model_defs_test.cpp` | doctest |
| ModelDef → MachineConfig round-trip matches old behavior | `test/core/model_defs_test.cpp` | doctest |
| ParseModelName with slug and canonical name | `test/core/model_defs_test.cpp` | doctest |
| ValidateMacEntry: ROM present | `test/config/mac_file_test.cpp` | doctest |
| ValidateMacEntry: ROM missing | `test/config/mac_file_test.cpp` | doctest |
| ValidateMacEntry: disk missing | `test/config/mac_file_test.cpp` | doctest |
| LaunchConfigFromMacEntry conversion | `test/config/mac_file_test.cpp` | doctest |

### Round-Trip Regression Test — Triple Verification

This is the highest-risk refactoring in the feature.  `MachineConfigForModel()`
is the only source of hardware truth — any field mismatch means a model
boots wrong or crashes.  Three independent verification layers:

**Layer 1 — Compile-time static_assert.**  A `constexpr` test function
compares `kModelDefs` entries against hand-written expected values for
critical fields (romSize, romBase, use68020, screenWidth, screenHeight).
Compile fails if any value drifts:

```cpp
// model_defs.h (bottom)
static_assert(kModelDefs[5].id == MacModel::Plus);
static_assert(kModelDefs[5].rom.size == 0x00020000);
static_assert(kModelDefs[5].rom.base == 0x00400000);
static_assert(kModelDefs[5].use68020 == false);
static_assert(kModelDefs[5].screen.width == 512);
static_assert(kModelDefs[10].id == MacModel::II);
static_assert(kModelDefs[10].rom.size == 0x00040000);
static_assert(kModelDefs[10].use68020 == true);
// ... critical fields for all 12 models ...
```

**Layer 2 — doctest field-by-field comparison.**  The old switch-based
implementation is temporarily preserved as `OldMachineConfigForModel()`.
A test iterates all 12 models, calls both old and new, and compares
**every single field** of `MachineConfig` (including VIA configs, wire
mappings, every device flag, every timing parameter):

```cpp
TEST_CASE("ModelDef produces identical MachineConfig")
{
    for (int i = 0; i < 12; ++i)
    {
        auto model = static_cast<MacModel>(i);
        MachineConfig expected = OldMachineConfigForModel(model);
        MachineConfig actual = MachineConfigForModel(model);
        CAPTURE(i);
        CHECK(actual.use68020 == expected.use68020);
        CHECK(actual.emFPU == expected.emFPU);
        CHECK(actual.emMMU == expected.emMMU);
        CHECK(actual.ramASize == expected.ramASize);
        CHECK(actual.ramBSize == expected.ramBSize);
        CHECK(actual.romSize == expected.romSize);
        CHECK(actual.romBase == expected.romBase);
        CHECK(actual.extnBlockBase == expected.extnBlockBase);
        CHECK(actual.emVIA1 == expected.emVIA1);
        CHECK(actual.emVIA2 == expected.emVIA2);
        CHECK(actual.emADB == expected.emADB);
        CHECK(actual.emClassicKbrd == expected.emClassicKbrd);
        CHECK(actual.emRTC == expected.emRTC);
        CHECK(actual.emPMU == expected.emPMU);
        CHECK(actual.emASC == expected.emASC);
        CHECK(actual.emClassicSnd == expected.emClassicSnd);
        CHECK(actual.emVidCard == expected.emVidCard);
        CHECK(actual.includeVidMem == expected.includeVidMem);
        CHECK(actual.vidMemSize == expected.vidMemSize);
        CHECK(actual.vidROMSize == expected.vidROMSize);
        CHECK(actual.clockMult == expected.clockMult);
        CHECK(actual.autoSlowSubTicks == expected.autoSlowSubTicks);
        CHECK(actual.autoSlowTime == expected.autoSlowTime);
        CHECK(actual.maxATTListN == expected.maxATTListN);
        CHECK(actual.screenWidth == expected.screenWidth);
        CHECK(actual.screenHeight == expected.screenHeight);
        CHECK(actual.screenDepth == expected.screenDepth);
        CHECK(actual.extnLn2Spc == expected.extnLn2Spc);
        // VIA1 config
        CHECK(actual.via1Config.oraFloatVal == expected.via1Config.oraFloatVal);
        CHECK(actual.via1Config.orbFloatVal == expected.via1Config.orbFloatVal);
        CHECK(actual.via1Config.oraCanIn == expected.via1Config.oraCanIn);
        CHECK(actual.via1Config.oraCanOut == expected.via1Config.oraCanOut);
        CHECK(actual.via1Config.orbCanIn == expected.via1Config.orbCanIn);
        CHECK(actual.via1Config.orbCanOut == expected.via1Config.orbCanOut);
        CHECK(actual.via1Config.portAWires == expected.via1Config.portAWires);
        CHECK(actual.via1Config.portBWires == expected.via1Config.portBWires);
        CHECK(actual.via1Config.cb2Wire == expected.via1Config.cb2Wire);
        CHECK(actual.via1Config.interruptWire == expected.via1Config.interruptWire);
        // VIA2 config (for models that have one)
        CHECK(actual.via2Config.oraFloatVal == expected.via2Config.oraFloatVal);
        CHECK(actual.via2Config.orbFloatVal == expected.via2Config.orbFloatVal);
        CHECK(actual.via2Config.portAWires == expected.via2Config.portAWires);
        CHECK(actual.via2Config.portBWires == expected.via2Config.portBWires);
        CHECK(actual.via2Config.cb2Wire == expected.via2Config.cb2Wire);
        CHECK(actual.via2Config.interruptWire == expected.via2Config.interruptWire);
    }
}
```

The old implementation is deleted **only after** this test passes green.

**Layer 3 — Golden test boot.**  Run the existing golden test
infrastructure for both Mac Plus and Mac II.  These replay a
recorded input sequence and compare framebuffer hashes against a
known-good reference.  A single wrong `MachineConfig` field causes
memory mapping, device initialization, or interrupt routing to change,
which produces a different framebuffer hash within the first few
hundred thousand instructions.

All three layers must pass before the old switch statement is removed.

### Integration Tests

Beyond the golden tests, manual boot testing for both bundled
Macintoshes (Mac Plus · System 6 and Mac II · System 7) confirms
end-to-end correctness through the full Launcher → .mac → boot path.
