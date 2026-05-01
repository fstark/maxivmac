# Model and Macintosh — Implementation Plan

Design: [MODEL_DESIGN.md](MODEL_DESIGN.md)
Spec: [MODEL.md](MODEL.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Constexpr ModelDef table and lookup functions | |
| 2 | Refactor MachineConfigForModel() to use ModelDef table | |
| 3 | Simplify ParseModelName() and ModelToString() | |
| 4 | .mac file parser and validator | |
| 5 | LaunchConfigFromMacEntry adapter | |
| 6 | Data directory resolution + asset migration (ROMs, .def files) | |
| 7 | Launcher UI (replaces model selector) | |
| 8 | Boot path integration | |
| 9 | Human testing — manual boot verification | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests && cd test && ./verify.sh`

---

## Phase 1 — Constexpr ModelDef Table

Create the authoritative `constexpr` table of hardware facts for all
12 models.  This is the root of the dependency tree — everything else
reads from it.

### 1.1 — Create `src/core/model_defs.h`

New header-only file.  Define `RomDef`, `ScreenDef`, `ModelDef` structs
and the `kModelDefs` constexpr table.  See design §2.1 for full struct
definitions.

```cpp
// src/core/model_defs.h
#pragma once
#include "core/machine_config.h"
#include <array>
#include <cstdint>
#include <string_view>

struct RomDef { ... };      // design §2.1
struct ScreenDef { ... };   // design §2.1
struct ModelDef { ... };    // design §2.1

inline constexpr std::array<ModelDef, 12> kModelDefs = {{ ... }};

constexpr const ModelDef *ModelDefFor(MacModel model);
const ModelDef *ModelDefForSlug(std::string_view slug);
```

Populate `kModelDefs` entries by extracting the current values from
each `case` in `MachineConfigForModel()` (src/core/machine_config.cpp
lines 142–465).  Every field must match the existing
switch statement exactly — this is the highest-risk transcription.

**Field mapping — ModelDef field → MachineConfig source:**

| ModelDef field     | MachineConfig field | Notes                         |
|--------------------|---------------------|-------------------------------|
| `id`               | `model`             | MacModel enum value           |
| `name`             | (new)               | e.g. `"Macintosh Plus"`       |
| `slug`             | (new)               | e.g. `"Plus"` (CLI/mac-file)  |
| `use68020`         | `use68020`          | direct copy                   |
| `emFPU`            | `emFPU`             | direct copy                   |
| `emMMU`            | `emMMU`             | direct copy                   |
| `ramASize`         | `ramASize`          | direct copy                   |
| `ramBSize`         | `ramBSize`          | direct copy (0 for single-bank) |
| `rom.filename`     | `romFileName`       | direct copy                   |
| `rom.size`         | `romSize`           | direct copy                   |
| `rom.base`         | `romBase`           | direct copy                   |
| `rom.md5`          | (new)               | from MD5 table above          |
| `screen.width`     | `screenWidth`       | direct copy                   |
| `screen.height`    | `screenHeight`      | direct copy                   |
| `screen.depth`     | `screenDepth`       | direct copy                   |
| `emVIA1`           | `emVIA1`            | direct copy                   |
| `emVIA2`           | `emVIA2`            | direct copy                   |
| `emADB`            | `emADB`             | direct copy                   |
| `emClassicKbrd`    | `emClassicKbrd`     | direct copy                   |
| `emRTC`            | `emRTC`             | direct copy (default `true`)  |
| `emPMU`            | `emPMU`             | direct copy (default `false`) |
| `emASC`            | `emASC`             | direct copy                   |
| `emClassicSnd`     | `emClassicSnd`      | direct copy                   |
| `emVidCard`        | `emVidCard`         | direct copy                   |
| `includeVidMem`    | `includeVidMem`     | direct copy                   |
| `vidMemSize`       | `vidMemSize`        | direct copy                   |
| `vidROMSize`       | `vidROMSize`        | direct copy                   |
| `extnBlockBase`    | `extnBlockBase`     | direct copy                   |
| `extnLn2Spc`       | `extnLn2Spc`        | default `7`, not set per-model |
| `clockMult`        | `clockMult`         | direct copy                   |
| `autoSlowSubTicks` | `autoSlowSubTicks`  | default `16384`, not per-model |
| `autoSlowTime`     | `autoSlowTime`      | default `60`, not per-model   |
| `maxATTListN`      | `maxATTListN`       | direct copy                   |

**Mac II gotcha:** The `case MacModel::II` block only sets
`romFileName`, `clockMult`, and VIA configs — all other fields
come from MachineConfig's **default member initializers**.  Copy
those defaults explicitly:
`use68020=true`, `emFPU=true`, `emMMU=false`,
`ramASize=0x00400000`, `ramBSize=0x00400000`,
`romSize=0x00040000`, `romBase=0x00800000`,
`extnBlockBase=0x50F0C000`,
`emVIA1=true`, `emVIA2=true`, `emADB=true`,
`emClassicKbrd=false`, `emRTC=true`, `emPMU=false`,
`emASC=true`, `emClassicSnd=false`, `emVidCard=true`,
`includeVidMem=true`, `vidMemSize=0x00080000`,
`vidROMSize=0x002000`, `maxATTListN=20`,
`screenWidth=640`, `screenHeight=480`, `screenDepth=3`.

The `slug` field for each model uses short CLI-friendly names:
`Twig43`, `Twiggy`, `128K`, `512Ke`, `Kanji`, `Plus`, `SE`, `SEFDHD`,
`Classic`, `PB100`, `II`, `IIx`.

ROM MD5s (pre-computed from `roms/`):

| Model    | ROM file          | MD5                              |
|----------|-------------------|----------------------------------|
| Twig43   | Twig43.ROM        | `e4faf3ecb169b875a1e66abbd5306b52` |
| Twiggy   | Twiggy.ROM        | `4f28b54a2c6d699b596a1e6072a57f58` |
| Mac128K  | Mac128K.ROM       | `db7e6d3205a2b48023fba5aa867ac6d6` |
| Mac512Ke | Mac512Ke.ROM      | `8a41e0754ffd1bb00d8183875c55164c` |
| Kanji    | MacPlusKanji.ROM  | `56737a4960e70635e310db0a7fb5332c` |
| Plus     | MacPlus.ROM       | `8a41e0754ffd1bb00d8183875c55164c` |
| SE       | MacSE.ROM         | `9fb38bdcc0d53d9d380897ee53dc1322` |
| SEFDHD   | SEFDHD.ROM        | `886444d7abc1185112391b8656c7e448` |
| Classic  | Classic.ROM       | `c229bb677cb41b84b780c9e38a09173e` |
| PB100    | PB100.ROM         | `dd390f7c86a730caac46fd522f8b2665` |
| II       | MacII.ROM         | `2a8a4c7f2a38e0ab0771f59a9a0f1ee4` |
| IIx      | MacIIx.ROM        | `2a8a4c7f2a38e0ab0771f59a9a0f1ee4` |

Note: Mac512Ke and Plus share the same MD5 (same ROM binary).
Same for II and IIx.

### 1.2 — Implement `ModelDefFor()` and `ModelDefForSlug()`

`ModelDefFor()` is `constexpr` — loops over `kModelDefs` looking for
a matching `id` field.  Asserts if not found (every enum value must
have an entry).

`ModelDefForSlug()` is a regular function (case-insensitive comparison
requires runtime tolower).  Lives in a new `src/core/model_defs.cpp`
or can be inline in the header if kept short.  Implementation: loop
over `kModelDefs`, compare `toLower(slug)` against `toLower(def.slug)`.

### 1.3 — Static assertions

Add `static_assert` checks at the bottom of `model_defs.h` for
critical fields of every model (design §10, Layer 1).  At minimum:
`id`, `rom.size`, `rom.base`, `use68020`, `screen.width`,
`screen.height` for all 12 models.

### 1.4 — Tests: `test/test_model_defs.cpp`

Create new test file.  Add to the `tests` target in CMakeLists.txt.

Test cases:
- `"ModelDef table has 12 entries"` — `CHECK(kModelDefs.size() == 12)`
- `"ModelDef table covers all MacModel values"` — iterate all 12
  enum values, call `ModelDefFor()`, verify non-null
- `"ModelDefFor returns correct entry"` — spot-check Plus and II
- `"ModelDefForSlug finds Plus"` — `ModelDefForSlug("Plus")`
- `"ModelDefForSlug is case-insensitive"` — `ModelDefForSlug("plus")`
- `"ModelDefForSlug returns nullptr for unknown"` — `ModelDefForSlug("Amiga")`

### Fence

- [ ] `src/core/model_defs.h` exists with all 12 entries
- [ ] `static_assert` checks compile
- [ ] `src/core/model_defs.cpp` exists (if needed for slug lookup)
- [ ] `test/test_model_defs.cpp` added to CMakeLists.txt
- [ ] Unit tests pass: `./bld/macos/tests --test-case="ModelDef*"`
- [ ] Full build clean
- [ ] Commit: `"model: phase 1 — constexpr ModelDef table"`

---

## Phase 2 — Refactor MachineConfigForModel()

Replace the 317-line switch statement with a table lookup from
`kModelDefs`.  This is the highest-risk change — a single wrong
field breaks a model.  Triple verification (design §10).

### 2.1 — Preserve old implementation

In `src/core/machine_config.cpp`, rename the current
`MachineConfigForModel()` to `OldMachineConfigForModel()`.  Keep it
as a `static` function.  This is temporary — deleted after the
round-trip test passes.

### 2.2 — Unify VIA dispatcher functions

Create two dispatcher functions:

```cpp
static VIAConfig MakeVIA1ConfigFor(MacModel model);
static VIAConfig MakeVIA2ConfigFor(MacModel model);
```

`MakeVIA1ConfigFor()` dispatches to the existing functions.
`MakeVIA2ConfigFor()` returns `MakeVIA2Config_MacII()` for II-family,
default `VIAConfig{}` otherwise.

**VIA1 routing table** (extracted from existing switch cases):

| Models                                          | VIA1 function           |
|-------------------------------------------------|-------------------------|
| Twig43, Twiggy, Mac128K, Mac512Ke, Kanji, Plus  | `MakeVIA1Config_Plus()` |
| SE, SEFDHD, Classic                              | `MakeVIA1Config_SE()`   |
| PB100                                            | `MakeVIA1Config_PB100()`|
| II, IIx                                          | `MakeVIA1Config_MacII()`|

**VIA2 routing table:**

| Models   | VIA2 function             |
|----------|---------------------------|
| II, IIx  | `MakeVIA2Config_MacII()`  |
| all others | `VIAConfig{}` (default) |

### 2.3 — New MachineConfigForModel()

Write the new implementation that reads from `kModelDefs` and calls
`MakeVIA1ConfigFor()` / `MakeVIA2ConfigFor()`.  Follow the exact
pattern in design §3.1.

### 2.4 — Round-trip regression test

Add to `test/test_model_defs.cpp`:

```cpp
TEST_CASE("ModelDef produces identical MachineConfig")
```

This test calls both `OldMachineConfigForModel()` and the new
`MachineConfigForModel()` for all 12 models, comparing **every field**
including VIA configs (design §10, Layer 2).

To make `OldMachineConfigForModel()` accessible to the test, either:
(a) expose it via a test-only header, or (b) move the test into a
file that can `#include "machine_config.cpp"` with a define.

Preferred approach: add a `OldMachineConfigForModel()` declaration in
`machine_config.h` guarded by `#ifdef TESTING`, and define `TESTING`
for the test target in CMakeLists.txt.

### 2.5 — Golden test verification

Run `./test/verify.sh` to confirm all golden tests pass with the new
implementation (Layer 3).

### Fence

- [ ] `OldMachineConfigForModel()` preserved as static function
- [ ] `MakeVIA1ConfigFor()` and `MakeVIA2ConfigFor()` dispatchers work
- [ ] New `MachineConfigForModel()` reads from `kModelDefs`
- [ ] Round-trip test passes for all 12 models (every field matches)
- [ ] Golden tests pass: `./test/verify.sh`
- [ ] Full build clean
- [ ] Commit: `"model: phase 2 — table-driven MachineConfigForModel"`

---

## Phase 3 — Simplify ParseModelName() and ModelToString()

Replace the if-chains and switch statements with table lookups.

### 3.1 — Simplify ParseModelName()

File: `src/core/config_loader.cpp` (lines 17–70).

Replace the ~50-line if-chain with a loop over `kModelDefs`.  Match
on both `slug` and the result of `ModelToString(def.id)`, both
case-insensitive.

Preserve the existing legacy aliases (`"plus"`, `"se"`, `"ii"`,
`"iix"`, `"128k"`, `"512ke"`, `"kanji"`, `"powerbook100"`) — these
are handled by the slug field in `kModelDefs`.  If any alias is not
covered by `slug` or the canonical name, add a small static alias
table.

### 3.2 — Simplify ModelToString()

File: `src/core/config_loader.cpp` (lines 624–656).

Replace the 12-case switch with a call to `ModelDefFor(model)->name`.
See design §3.3.

### 3.3 — Tests

Add to `test/test_model_defs.cpp`:

- `"ParseModelName accepts slug"` — test `"Plus"`, `"II"`, `"IIx"`
- `"ParseModelName accepts legacy aliases"` — test `"plus"`, `"se"`,
  `"128k"`, `"powerbook100"`
- `"ParseModelName rejects unknown"` — test `"Amiga"`, `""`
- `"ModelToString round-trips"` — for each model, confirm
  `ParseModelName(ModelToString(m))` returns `m`

### Fence

- [ ] `ParseModelName()` uses `kModelDefs` loop
- [ ] `ModelToString()` uses table lookup
- [ ] All existing tests still pass (no behavioral change)
- [ ] New ParseModelName/ModelToString tests pass
- [ ] Golden tests pass
- [ ] Full build clean
- [ ] Commit: `"model: phase 3 — table-driven ParseModelName and ModelToString"`

---

## Phase 4 — .mac File Parser and Validator

Create the `src/config/` module with `ParseMacFile()`,
`ScanMacDirectory()`, and `ValidateMacEntry()`.

### 4.1 — Create `src/config/mac_file.h`

Public header with `MacFileEntry` struct, parser and validator
function declarations, and `ResolveDataDir()`.  See design §2.2
and §2.3 for exact definitions.

### 4.2 — Create `src/config/mac_file.cpp`

Implement `ParseMacFile()` following the algorithm in design §5.1.
Key details:
- Line-oriented: read line-by-line, strip `#` comments, trim whitespace
- Split on first `=` → key, value
- Known keys: `name`, `description`, `model`, `disk`, `shared`,
  `serial-a`, `ram`, `screen`
- Unknown keys → parse error (fail fast)
- `model` → calls `ParseModelName()` from `config_loader.h`
- `disk` and `shared` are repeatable (push_back)
- `ram` → parse `"4M"` / `"2560K"` per design §5.2 (MB-only for v1.0)
- `screen` → parse `"640x480x8"` per design §5.3

Implement `ScanMacDirectory()`:
- Iterate directory entries matching `*.mac`
- Call `ParseMacFile()` on each
- Skip (and log via `DIAG`) entries that fail to parse
- Return vector of successfully parsed entries

Implement `ValidateMacEntry()`:
- Look up the model's `RomDef` via `ModelDefFor(entry.model)`
- Resolve ROM path: `<romDir>/<romDef.filename>`
- Check file exists
- If `romDef.md5` is non-empty, compute MD5 via `md5_file()` and compare
- Check each disk in `entry.disks` exists in `<diskDir>/`
- Populate `romAvailable`, `allDisksAvailable`, `validationError`

### 4.3 — Add to CMakeLists.txt

Add `src/config/mac_file.cpp` to `MINIVMAC_SOURCES`.

### 4.4 — Tests: `test/test_mac_file.cpp`

Create new test file.  Add to `tests` target in CMakeLists.txt.

Use temporary files for testing (create `.mac` content in a temp dir,
parse, verify).  Or use fixed test data strings if the parser can
accept a string buffer.

Test cases:
- `"ParseMacFile: valid file"` — all fields populated correctly
- `"ParseMacFile: missing required name"` — error
- `"ParseMacFile: missing required model"` — error
- `"ParseMacFile: unknown key"` — error
- `"ParseMacFile: repeatable disk"` — multiple disk entries
- `"ParseMacFile: repeatable shared"` — multiple shared entries
- `"ParseMacFile: RAM size 4M"` — `ramOverrideMB == 4`
- `"ParseMacFile: RAM size 2560K"` — error (sub-MB not supported in v1.0,
  actually 2560K = 2.5MB, check how this is handled)
- `"ParseMacFile: screen spec 640x480x8"` — correct W/H/depth
- `"ParseMacFile: comments and blank lines"` — ignored correctly
- `"ValidateMacEntry: ROM present"` — `romAvailable == true`
- `"ValidateMacEntry: ROM missing"` — `romAvailable == false`, error set
- `"ValidateMacEntry: ROM MD5 mismatch"` — wrong ROM file, error set
- `"ValidateMacEntry: disk missing"` — `allDisksAvailable == false`

**MD5 dependency note:** `md5_file()` already exists in
`src/core/md5.h` — it takes a path and writes 16 raw bytes.
To compare against the hex strings in `ModelDef.rom.md5`, write
a small helper `md5_file_hex(path, out33)` that calls `md5_file()`
then formats to hex with `snprintf("%02x")` (same pattern as
`hashToHex()` in `state_recorder.cpp`).  Put it in `mac_file.cpp`
as a `static` helper — it's only needed by `ValidateMacEntry()`.

### Fence

- [ ] `src/config/mac_file.h` and `src/config/mac_file.cpp` exist
- [ ] `test/test_mac_file.cpp` added to CMakeLists.txt tests target
- [ ] Parser handles all keys from the spec
- [ ] Validator checks ROM existence and MD5, disk existence
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*MacFile*,*ParseMac*"`
- [ ] Full build clean
- [ ] Commit: `"model: phase 4 — .mac file parser and validator"`

---

## Phase 5 — LaunchConfigFromMacEntry Adapter

Bridge `MacFileEntry` → `LaunchConfig` so the existing boot path
can consume .mac file data.

### 5.1 — Add `LaunchConfigFromMacEntry()` to config_loader

File: `src/core/config_loader.h` — add declaration:

```cpp
LaunchConfig LaunchConfigFromMacEntry(const MacFileEntry &entry,
                                      std::string_view dataDir);
```

File: `src/core/config_loader.cpp` — implement per design §3.6.
Converts model, ROM path, disks, shared dirs, RAM/screen overrides,
and serial config into a `LaunchConfig`.

Add `#include "config/mac_file.h"` to config_loader.cpp.

### 5.2 — Tests

Add to `test/test_mac_file.cpp`:

- `"LaunchConfigFromMacEntry: basic conversion"` — create a
  `MacFileEntry` with known values, convert, check `LaunchConfig` fields
- `"LaunchConfigFromMacEntry: disk paths resolved"` — verify
  `diskPaths` are prefixed with `<dataDir>/disks/`
- `"LaunchConfigFromMacEntry: absolute shared dir preserved"` — verify
  absolute paths pass through unchanged
- `"LaunchConfigFromMacEntry: relative shared dir resolved"` — verify
  relative paths get `<dataDir>/` prefix

### Fence

- [ ] `LaunchConfigFromMacEntry()` declared and implemented
- [ ] Unit tests pass
- [ ] Full build clean
- [ ] Commit: `"model: phase 5 — LaunchConfigFromMacEntry adapter"`

---

## Phase 6 — Data Directory Resolution and Asset Migration

Implement `ResolveDataDir()`, move ROMs and `.def` files into the
`data/` tree, and update all code that referenced the old paths.

**Current state of files on disk:**
- ROMs: `roms/` (repo root) — 12 `.ROM` files
- Debug defs: `assets/` — `traps.def`, `globals.def`, `types.def`,
  `typemap.def`, `errors.def`
- Disks: `data/disks/` — already in place
- `.mac` files: `data/macs/` — already in place
- `data/roms/` and `data/debug/` — exist but empty

**Code with hardcoded paths to update:**
- `src/core/config_loader.cpp` line 700: `ResolveRomPath()` searches
  `roms/<name>` as a fallback
- `src/core/main.cpp` lines 241–255: loads `"assets/types.def"`,
  `"assets/errors.def"`, `"assets/traps.def"`, `"assets/globals.def"`
- `src/storage/host_volume.cpp` line 42: loads `"assets/typemap.def"`
- `src/platform/emulator_shell.cpp`: passes `romDir` / `romPath_`
  through to `LoadMacRom()`
- `src/platform/imgui_model_selector.cpp` line 89: calls
  `ResolveRomPath("", e.model, romDir)`

### 6.1 — Implement `ResolveDataDir()`

Add to `src/config/mac_file.cpp` (declared in `mac_file.h`):

```cpp
std::string ResolveDataDir(std::string_view appParent);
```

Search order (design §5.4):
1. `<appParent>/data/` — check directory exists
2. `CWD/data/` — fallback
3. Return `""` if neither found

Use `std::filesystem::is_directory()` or POSIX `stat()` for existence
check.

### 6.2 — Move ROMs: `roms/` → `data/roms/`

`git mv` all 12 `.ROM` files from `roms/` to `data/roms/`.

Update `ResolveRomPath()` in `src/core/config_loader.cpp` to search
`data/roms/<name>` instead of (or in addition to) `roms/<name>`.
The preferred approach: add `<dataDir>/roms/<name>` as the primary
search path, keep `roms/<name>` as a legacy fallback so existing
users' setups don't break immediately.  Accept `dataDir` as a new
parameter or use `ResolveDataDir()` internally.

Update `imgui_model_selector.cpp` (still alive in this phase) to
pass the new ROM directory.  This code will be deleted in phase 7,
but it must compile and work in-between.

Update `build-debug.sh`, `run-debug.sh`, `build-reference.sh`,
`run-reference.sh`, and any other scripts that reference `roms/`.

Update `.gitignore` if it has `roms/`-specific entries.

### 6.3 — Move debug defs: `assets/` → `data/debug/`

`git mv` the 5 `.def` files from `assets/` to `data/debug/`.

Update the 5 hardcoded load paths in the source:

| File | Line | Old path | New path |
|------|------|----------|----------|
| `src/core/main.cpp` | 241 | `"assets/types.def"` | `<dataDir> + "/debug/types.def"` |
| `src/core/main.cpp` | 242 | `"assets/errors.def"` | `<dataDir> + "/debug/errors.def"` |
| `src/core/main.cpp` | 248 | `"assets/globals.def"` | `<dataDir> + "/debug/globals.def"` |
| `src/core/main.cpp` | 254–255 | `"assets/traps.def"`, `"assets/errors.def"` | `<dataDir> + "/debug/..."` |
| `src/storage/host_volume.cpp` | 42 | `"assets/typemap.def"` | `<dataDir> + "/debug/typemap.def"` |

This requires `ResolveDataDir()` to be called early in the boot
sequence and its result stored where `main.cpp` and
`host_volume.cpp` can access it.

**Decision: add `std::string dataDir` field to `LaunchConfig`.**
It already carries `romDir`, and `dataDir` subsumes it.  Add the
field after `romDir` in `src/core/config_loader.h`:

```cpp
std::string romDir;  // --romdir: directory to search for ROM files
std::string dataDir; // resolved data/ directory (set by ResolveDataDir)
```

Set `dataDir` once in `app_main.cpp` before any loading code runs.
All code that currently uses hardcoded `"assets/"` paths switches
to `launchConfig.dataDir + "/debug/..."`.

### 6.4 — Tests

Add to `test/test_mac_file.cpp`:

- `"ResolveDataDir: finds data/ next to app"` — create a temp dir
  structure, verify resolution
- `"ResolveDataDir: falls back to CWD"` — verify CWD fallback
- `"ResolveDataDir: returns empty if not found"` — verify empty return

### 6.5 — Smoke verification

Build and run `./bld/macos/tests`.  Run `./test/verify.sh` (golden
tests).  Both must pass with ROMs now in `data/roms/` and defs in
`data/debug/`.  The golden test runner (`verify.sh`) may need its
ROM path updated to `data/roms/`.

### Fence

- [ ] `ResolveDataDir()` implemented and tested
- [ ] All 12 ROMs moved to `data/roms/`
- [ ] `ResolveRomPath()` searches `data/roms/` (primary) + `roms/` (legacy)
- [ ] All 5 `.def` files moved to `data/debug/`
- [ ] All 6 hardcoded `"assets/"` paths updated
- [ ] Scripts and `.gitignore` updated for new paths
- [ ] Debugger still loads trap/global definitions correctly
- [ ] Golden tests pass with new ROM locations
- [ ] Full build clean
- [ ] Commit: `"model: phase 6 — data directory layout and asset migration"`

---

## Phase 7 — Launcher UI

Replace the model selector with the Launcher.  The Launcher scans
`.mac` files and shows them as clickable cards.

### 7.1 — Create `src/platform/imgui_launcher.h`

Define the `Launcher` class per design §4.2:

```cpp
class Launcher
{
public:
    void init(std::vector<MacFileEntry> entries);
    const MacFileEntry *draw();
private:
    std::vector<MacFileEntry> entries_;
    int selectedIndex_ = -1;
};
```

### 7.2 — Create `src/platform/imgui_launcher.cpp`

Implement the Launcher UI (design §5.5):
- `init()` stores the entries
- `draw()` renders a card grid for each entry, returns
  `const MacFileEntry*` when clicked (null otherwise)
- One click boots immediately (no config panel)

**Draw pseudocode** (mirrors existing `ModelSelector::drawModelGrid()`
pattern in `imgui_model_selector.cpp`):

```cpp
const MacFileEntry *Launcher::draw()
{
    // Full-window, no decoration (same as ModelSelector)
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("##Launcher", nullptr, kFullscreenFlags);

    // Title
    ImGui::Text("Maxi vMac");
    ImGui::Separator();

    // Card grid: 4 columns, 16px gap
    float avail = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)((avail + 16) / (200 + 16)));
    float cardW = (avail - (cols - 1) * 16) / cols;
    float cardH = 100.0f;

    int col = 0;
    for (int i = 0; i < (int)entries_.size(); ++i)
    {
        const auto &e = entries_[i];
        if (col > 0) ImGui::SameLine(0, 16);

        bool valid = e.romAvailable && e.allDisksAvailable;
        if (!valid) ImGui::BeginDisabled();

        // Card background — ImGui::Button sized as card
        ImGui::PushID(i);
        if (ImGui::Button("##card", {cardW, cardH}))
        {
            if (valid) { ImGui::PopID(); ImGui::End(); return &entries_[i]; }
        }

        // Overlay text on the button (use SetCursorPos back)
        // Line 1: name (bold via PushFont or just larger)
        //   e.g. "Mac Plus · System 6"
        // Line 2: model info from ModelDefFor(e.model)
        //   e.g. "68000 · 4 MB RAM · 512×342"
        // Line 3: boot disk filename
        //   e.g. "plus-608.hfs"
        // Line 4 (invalid only): red text with e.validationError

        ImGui::PopID();
        if (!valid) ImGui::EndDisabled();

        col = (col + 1) % cols;
        if (col == 0) ImGui::Spacing();
    }

    ImGui::End();
    return nullptr;
}
```

The overlay text rendering uses `ImGui::SetCursorPos()` to
position text inside the button rect (same technique as the
existing model selector's `drawModelGrid`).  Valid cards are
clickable; invalid cards use `ImGui::BeginDisabled()` /
`ImGui::EndDisabled()` and show `e.validationError` in
`ImVec4(0.8f, 0.2f, 0.2f, 1)` (red) as a final line.

### 7.3 — Add to CMakeLists.txt

Add `src/platform/imgui_launcher.cpp` to `MINIVMAC_SOURCES`.
Remove `src/platform/imgui_model_selector.cpp` from `MINIVMAC_SOURCES`.

### 7.4 — Update ImGuiBackend

File: `src/platform/imgui_backend.h`:
- Replace `ModelSelector modelSelector_` with `Launcher launcher_`
- Add `#include "platform/imgui_launcher.h"`
- Remove `#include` of the old model selector header

File: `src/platform/imgui_backend.cpp`:
- Replace `drawModelSelector()` to call `launcher_.draw()` instead
  of `modelSelector_.draw()`
- Update `createSelectorWindow()` → `createLauncher(entries)` (or
  equivalent initialization method)
- When `launcher_.draw()` returns a non-null entry, convert to
  `LaunchConfig` via `LaunchConfigFromMacEntry()` and boot

### 7.5 — Update UIState enum

Add `Launcher` to `UIState` if it doesn't exist, or rename
`ModelSelector` → `Launcher`.

### Fence

- [ ] `src/platform/imgui_launcher.h` and `.cpp` exist
- [ ] `src/platform/imgui_model_selector.cpp` removed from build
- [ ] `ImGuiBackend` uses `Launcher` instead of `ModelSelector`
- [ ] UIState updated
- [ ] Full build clean (no references to old model selector)
- [ ] Commit: `"model: phase 7 — Launcher UI replaces model selector"`

---

## Phase 8 — Boot Path Integration

Wire everything together: Launcher flow, direct `.mac` boot, and
cleanup.

### 8.1 — Update app_main.cpp — Launcher flow

File: `src/platform/app_main.cpp`

Replace the no-model path (currently lines ~45–54):

```cpp
// Old:
// shell.initPlatform();
// imguiBackend.createSelectorWindow();
// UIState::ModelSelector

// New:
std::string dataDir = ResolveDataDir(appParent);
auto entries = ScanMacDirectory(dataDir + "/macs");
std::string romDir = dataDir + "/roms";
std::string diskDir = dataDir + "/disks";
for (auto &e : entries)
    ValidateMacEntry(e, romDir, diskDir);
imguiBackend.createLauncher(std::move(entries));
imguiBackend.setUIState(UIState::Launcher);
```

### 8.2 — Update app_main.cpp — Direct .mac file launch

Add detection for `.mac` file argument (design §3.5):

```cpp
if (argc >= 2 && HasSuffix(argv[1], ".mac"))
{
    MacFileEntry entry;
    std::string err;
    if (!ParseMacFile(argv[1], entry, err)) { ... error ... }
    ValidateMacEntry(entry, romDir, diskDir);
    if (!entry.romAvailable) { ... error ... }
    LaunchConfig lc = LaunchConfigFromMacEntry(entry, dataDir);
    SetLaunchConfig(lc);
    // proceed to boot
}
```

### 8.3 — Delete old model selector files

Remove `src/platform/imgui_model_selector.cpp` and
`src/platform/imgui_model_selector.h` from the source tree.

Remove `test/test_ui.cpp` references to the model selector if any
exist, or update them.

### 8.4 — Delete OldMachineConfigForModel()

Now that all verification is complete, remove the old switch-based
`OldMachineConfigForModel()` from `machine_config.cpp` and the
round-trip test that depended on it.

### 8.5 — Golden test verification

Run `./test/verify.sh` to confirm all golden tests pass through the
entire new boot path.

### Fence

- [ ] Launcher flow works: no-arg launch → scan → cards → boot
- [ ] Direct `.mac` file launch works: `maxivmac foo.mac` → boot
- [ ] Old model selector files deleted
- [ ] `OldMachineConfigForModel()` deleted
- [ ] Golden tests pass: `./test/verify.sh`
- [ ] All unit tests pass: `./bld/macos/tests`
- [ ] Full build clean
- [ ] Commit: `"model: phase 8 — boot path integration"`

---

## Phase 9 — Human Testing

Manual boot verification.  Both bundled Macintoshes must boot
successfully through the full Launcher → .mac → boot path.

### 9.1 — Launcher display test

Launch `maxivmac` with no arguments.  Verify:
- Data directory is resolved automatically
- Two cards appear: "Mac Plus · System 6" and "Mac II · System 7"
- Both cards show green/valid status (ROM + disk present)
- Model info (CPU, RAM) is displayed on each card

### 9.2 — Mac Plus boot test

Click the "Mac Plus · System 6" card.  Verify:
- Mac boots to System 6 Finder
- Shared drive is mounted
- No crashes or visual glitches

### 9.3 — Mac II boot test

Click the "Mac II · System 7" card.  Verify:
- Mac boots to System 7 Finder in color
- Shared drive is mounted
- SLIP networking initializes (serial-a = slip)
- No crashes or visual glitches

### 9.4 — Direct .mac launch test

Run `maxivmac data/macs/plus-608.mac` from the command line.  Verify:
- Launcher is not shown
- Mac Plus boots directly

### 9.5 — Invalid .mac test

Create a temporary `.mac` file referencing a missing ROM or disk.
Launch it.  Verify:
- Clear error message printed to stderr
- App exits cleanly (no crash)

### 9.6 — Launcher validation display test

Temporarily rename a ROM file.  Relaunch `maxivmac`.  Verify:
- The affected card is greyed out
- The reason ("ROM missing") is visible
- The other card remains clickable

### Fence

- [ ] Launcher shows both bundled Macintoshes
- [ ] Mac Plus boots to Finder via Launcher
- [ ] Mac II boots to Finder via Launcher
- [ ] Direct .mac launch bypasses Launcher
- [ ] Invalid configs produce clear error messages
- [ ] Missing ROM greys out affected cards
- [ ] Commit: `"model: phase 9 — verified manual boot"`
