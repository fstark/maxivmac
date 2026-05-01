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

The `slug` field for each model uses short CLI-friendly names:
`Twig43`, `Twiggy`, `128K`, `512Ke`, `Kanji`, `Plus`, `SE`, `SEFDHD`,
`Classic`, `PB100`, `II`, `IIx`.

ROM MD5s: compute from the bundled ROM files in `roms/` using
`md5 -q roms/MacPlus.ROM` etc.  For models that share a ROM file
(e.g. Twig43 and Twiggy may share), use the same MD5.  For models
whose ROM we don't bundle, use an empty string `""` — validation
will skip the MD5 check when the string is empty.

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

`MakeVIA1ConfigFor()` dispatches to the existing `MakeVIA1Config_Plus()`,
`MakeVIA1Config_SE()`, `MakeVIA1Config_PB100()`, `MakeVIA1Config_MacII()`
based on model.  `MakeVIA2ConfigFor()` returns `MakeVIA2Config_MacII()`
for II-family, default `VIAConfig{}` otherwise.

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
- `"ValidateMacEntry: disk missing"` — `allDisksAvailable == false`

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
`host_volume.cpp` can access it.  Options: (a) add a `dataDir`
field to `LaunchConfig`, (b) resolve once in `ProgramEarlyInit()`
and store in a file-scope static, (c) pass as parameter.  Prefer
(a) — `LaunchConfig` already carries `romDir`, and `dataDir`
subsumes it.

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
- `draw()` renders a card for each entry:
  - Name (from `.mac` file)
  - Model info (look up `ModelDefFor()` for CPU, default RAM)
  - Boot disk name(s)
  - Status: green if bootable, greyed out with reason if not
  - Valid cards are clickable — returns the selected `MacFileEntry*`
- One click boots immediately (no config panel)

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
