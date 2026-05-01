# Model and Macintosh Definitions

Two complementary data structures describe what maxivmac can emulate
and what configurations are ready to boot.

See [GLOSSARY.md](../GLOSSARY.md) for terminology: **Model**,
**Macintosh**, **Rig**, **Launcher**.

---

## Concepts

### Model

A static, immutable description of a Macintosh hardware platform.
Defined in C++ as `constexpr` data.  Ships compiled into the binary.
The user never edits a Model.

Each of the 12 `MacModel` enum values has exactly one Model
definition.  A Model describes:

- Identity: enum id, internal name (e.g. "Macintosh Plus"), CLI
  slug (e.g. `Plus`).
- CPU: 68000 vs 68020, FPU, MMU.
- ROM: filename, expected MD5 checksum, size, base address.
- Memory: default RAM size.
- Screen: default width, height, depth (log₂ bpp).
- Devices: VIA1/VIA2, ADB, classic keyboard, RTC, PMU, ASC, classic
  sound, video card.
- Wiring: VIA port-to-wire mappings.
- Timing: clock multiplier, auto-slow parameters.

Models are the authoritative source for hardware truth.  Nothing
outside the Model definition may hard-code hardware facts.

### Macintosh

A bootable configuration that references a Model and adds everything
needed to boot: disk image(s), optional RAM/screen overrides, shared
drive(s), serial port configuration.

Macintosh definitions live as `.mac` files in the `data/macs/`
directory.  The format is the same `key = value` syntax used by
`.def` files.  `#` begins a comment.  Keys may repeat to express
lists.

The Launcher scans `data/macs/`, parses each `.mac` file, validates
that the referenced ROM and disk(s) exist, and presents every
Macintosh as a card.  Valid Macintoshes are clickable.  Invalid ones
(missing ROM, missing disk) are greyed out with a reason shown so
the user understands why they can't boot.

---

## `.mac` File Format

```
# Mac Plus · System 6
name = Mac Plus · System 6
description = The classic compact Macintosh experience.
model = Plus
disk = plus-608.hfs
shared = shared/
```

```
# Mac II · System 7
name = Mac II · System 7
description = Color Macintosh with networking.
model = II
disk = macii-7.hfs
shared = shared/
serial-a = slip
screen = 640x480x8
ram = 8M
```

### Fields

| Key           | Required | Repeatable | Description |
|---------------|----------|------------|-------------|
| `name`        | yes      | no  | Informal user-facing label for the Launcher card. |
| `description` | no       | no  | Longer descriptive text.  May or may not be displayed. |
| `model`       | yes      | no  | Model identifier matching a `MacModel` enum name (`Plus`, `II`, `SE`, etc.). |
| `disk`        | no       | yes | Disk image filename, looked up in `data/disks/`.  First occurrence is the boot disk.  Zero disks is valid — the Mac boots to the "insert disk" screen. |
| `shared`      | no       | yes | Shared drive directory.  Absolute if starts with `/`, otherwise relative to `data/`. |
| `serial-a`    | no       | no  | Serial port A configuration (e.g. `slip`). |
| `ram`          | no       | no  | RAM size override (e.g. `4M`, `2560K`).  Replaces the Model default. |
| `screen`       | no       | no  | Screen geometry override (e.g. `640x480x8`).  Replaces the Model default. |

Unrecognized keys cause a parse error (fail fast).

---

## Data Directory Layout

All runtime data lives under `data/`, a sibling directory of the
binary.  The binary resolves its own path at startup and looks for
`data/` next to it.

```
maxivmac              (binary)
data/
├── roms/             ROM files (MacPlus.ROM, MacII.ROM, ...)
├── disks/            Boot disk images (plus-608.hfs, macii-7.hfs)
├── macs/             Macintosh definitions (.mac files)
├── shared/           Default shared drive content
├── system/           maxivmac internal assets (tool disk with INIT, etc.)
└── debug/            Debugger .def files (traps.def, globals.def, ...)
```

### Tool Disk

The `data/system/` directory holds maxivmac's own internal assets,
notably the tool disk containing the INIT installer.  This disk is
not referenced by any `.mac` file — the code mounts it on demand
when the user requests it (e.g. "Install maxivmac extensions").

### ROM Validation

Each Model declares a ROM filename and an expected MD5 checksum.
At startup (or when the Launcher is displayed), the app:

1. Checks that `data/roms/<romFile>` exists.
2. Computes its MD5 and compares against the Model's expected hash.
3. ROM missing → Macintoshes using that Model are greyed out
   ("ROM missing").
4. ROM wrong checksum → Macintoshes using that Model are greyed out
   ("ROM checksum mismatch").

### Disk Validation

Each `.mac` file may reference disk filenames.  The app checks that
each referenced file exists in `data/disks/`.  Any missing disk →
that Macintosh is greyed out in the Launcher with a reason shown.
A `.mac` file with zero disks is valid — the Mac boots to the
"insert disk" screen and the user can drag-and-drop a disk image.

---

## v1.0 Scope

### Models

All 12 `MacModel` values have Model definitions:
Twig43, Twiggy, Mac128K, Mac512Ke, Kanji, Plus, SE, SEFDHD,
Classic, PB100, II, IIx.

### Bundled Macintoshes

Two `.mac` files ship with the binary:

| File | Name | Model | Disk | Notes |
|------|------|-------|------|-------|
| `plus-608.mac` | Mac Plus · System 6 | Plus | `plus-608.hfs` | Classic compact Mac |
| `macii-7.mac` | Mac II · System 7 | II | `macii-7.hfs` | Color, networking via SLIP |

### Bundled Assets

- 2 ROM files: `MacPlus.ROM`, `MacII.ROM`
- 2 boot disk images with INIT pre-installed
- 1 tool disk with INIT installer (in `data/system/`)
- 1 `shared/` directory with default content
- Debugger `.def` files (moved from current `assets/`)

Everything is bundled in the distribution archive.  Download → unzip
→ launch → click a card → running Mac.  No setup steps.

---

## Boot Flow

```
Launch (no args) → scan data/macs/*.mac
                  → validate ROM + disks for each
                  → Launcher shows Macintoshes as cards
                  → user clicks card
                  → build MachineConfig from Model
                  → apply .mac overrides (ram, screen)
                  → attach disks, shared drives, serial
                  → create Rig → boot

Launch foo.mac                     → load .mac file, bypass Launcher, boot directly
Launch --model=Plus --disk=foo.hfs → bypass Launcher, boot directly
Launch foo.hfs (no --model)        → error with clear message
```

---

## Relationship to Existing Code

`MachineConfigForModel()` in `machine_config.cpp` is the current
implementation of Model data.  A2 restructures this into declarative
`constexpr` Model definitions while preserving the same hardware
configuration output.  The existing `MachineConfig` struct remains
the runtime representation consumed by the Rig — Models populate it,
Macintosh overrides modify it.
