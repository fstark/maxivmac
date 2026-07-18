# Tool Disk

A read-only 800KB HFS disk image containing the maxivmac guest-side
tools.  Built from host-side artifacts and mounted on demand via an
overlay button.

## Contents

| File | Source | Description |
|------|--------|-------------|
| README | `macsrc/tooldisk/README` | Plain-text install instructions (ASCII, type `TEXT`/creator `ttxt`) |
| maxivmac INIT | `macsrc/init/maxivmac INIT` | Host-integration INIT (clipboard sync, shared drive, host commands) |
| ImportFl | `macsrc/tooldisk/ImportFl` | Import a file from the host (minivmac utility) |
| ExportFl | `macsrc/tooldisk/ExportFl` | Export a file to the host (minivmac utility) |

## README Content

The README includes:

- Version number
- Compatibility: works with System 6.0.8 (minor issues); stability
  issues with 7.0.1 and 7.5.3
- INIT installation instructions ("drag into System Folder, restart")
- Clipboard sync: automatic copy/paste, MacOS Roman ↔ UTF-8
  conversion, text and images (PICT) supported; caveat about apps
  with private scraps needing an app switch to discover changes
- Shared drive: host folder mounted as a Mac volume, file name
  mapping, MacOS Roman conversion including line endings, automatic
  Type/Creator assignment, drag-and-drop a host folder to
  automatically mount it
- Host commands: Launch (start an app from host), ExitToShell (quit
  to Finder), ShutDown (clean power-off)
- ImportFl / ExportFl description (these are the minivmac versions)

## Disk Image

- Format: 800KB HFS
- Volume name: `maxivmac Tools <version>` (e.g. `maxivmac Tools v1.2.3`)
- Created from scratch at build time using `hformat` + `hcopy`
- Mounted read-only (host file permissions `444`)
- No custom Finder icon positions (auto-arranged by Finder)

## Source Layout

```
macsrc/tooldisk/
├── README              (plain text, LF — converted to CR at build time)
├── ExportFl            (pre-built binary, checked in)
├── ._ExportFl          (AppleDouble resource fork)
├── ImportFl            (pre-built binary, checked in)
├── ._ImportFl          (AppleDouble resource fork)
├── build.mac           (emulator config for Desktop file creation)
└── build-tooldisk.dbg  (debug script: wait for Finder, quit)
```

The INIT is not stored here — it comes from `macsrc/init/maxivmac INIT`
(+ `._maxivmac INIT`) after running `build-init.sh`.

## Build Script

`build-tooldisk.sh` at the repository root.  Separate from
`build-init.sh` because the Tool Disk is a packaging layer above the
INIT — future guest-side apps (cdev, reimplemented import/export)
will be additional inputs.

### Prerequisites

- `hfsutils` installed (`brew install hfsutils`)
- `ad2bin` built (`bld/macos/ad2bin`)
- `build-init.sh` has been run (INIT compiled)
- `macsrc/build.hfs` (bootable System 6 disk for Desktop file creation)
- `bld/macos/maxivmac` built (for the headless Finder step)

### Flow

```bash
# 1. Create fresh 800K HFS image
dd if=/dev/zero of=data/system/tools.hfs bs=1024 count=800
hformat -l "maxivmac Tools $VERSION" data/system/tools.hfs

# 2. Convert AppleDouble files to MacBinary via ad2bin
ad2bin "macsrc/init/maxivmac INIT"
ad2bin "macsrc/tooldisk/ImportFl"
ad2bin "macsrc/tooldisk/ExportFl"

# 3. Mount and copy artifacts
hmount data/system/tools.hfs
hcopy -r /tmp/README.mac ":README"
hattrib -t TEXT -c ttxt ":README"
hcopy -m "macsrc/init/maxivmac INIT.bin" ":maxivmac INIT"
hcopy -m "macsrc/tooldisk/ImportFl.bin" ":ImportFl"
hcopy -m "macsrc/tooldisk/ExportFl.bin" ":ExportFl"
humount

# 4. Boot emulator to let Finder create Desktop file
maxivmac --headless --dbg-script=macsrc/tooldisk/build-tooldisk.dbg \
    macsrc/tooldisk/build.mac

# 5. Make read-only
chmod 444 data/system/tools.hfs
```

The README is converted from LF to CR before copy:
```bash
tr '\n' '\r' < macsrc/tooldisk/README > /tmp/README.mac
```

`ad2bin` (in `tools/ad2bin/`) converts macOS AppleDouble (._file + data
file) to MacBinary II format, which `hcopy -m` can inject with both
forks and correct type/creator metadata.

### Output

`data/system/tools.hfs` — ready to be mounted in the guest.

## Runtime Location

`data/system/tools.hfs` relative to the binary, resolved via the
standard Data Directory mechanism.

## Overlay UI

### Button Placement

In the Information section of the control overlay, right-aligned on
the same line as the INIT version display.  The flow reads left to
right: "status → action."  The button includes the maxivmac floppy
icon (`data/defaults/floppy-color.png`) alongside the text label.

### Button States

| INIT state | Button label | Enabled? |
|---|---|---|
| No INIT reported | Install Tools (T) | Yes |
| Any INIT present | Update Tools (T) | Yes |
| Tool Disk already mounted | Tools Mounted | No (greyed) |
| `tools.hfs` not found | Install Tools (T) | No (greyed, tooltip: "Tool Disk not found") |

### Keyboard Shortcut

**T** — available in the overlay context.

### Behaviour

- Calls `Sony_Insert1("data/system/tools.hfs", false)`
- File is `chmod 444`, so `Sony_Insert1` opens read-only and the disk
  mounts locked (existing fallback logic)
- Disk stays mounted until the user ejects from the guest (standard
  Mac behaviour)

## INIT Version Display

The Information section already shows the INIT version string
(reported via `kCmdInitIdent` at boot).  Extend this to show
freshness:

```
Mac Plus • System 6.0.7 • INIT dev-0054e84 (up to date)
Mac Plus • System 6.0.7 • INIT dev-0054e84 (outdated)
Mac Plus • System 6.0.7 • No INIT installed
```

Comparison is string equality between the INIT's reported version and
the host binary's compiled-in version.  This is best-effort — the
binary may have changed without a new commit (same version string,
different content).

## Future Work

- Custom guest-side apps (cdev for emulator control, reimplemented
  import/export) added as new inputs to `build-tooldisk.sh`
- Auto-mount on first boot (detect missing INIT, mount Tool Disk
  automatically)
- INIT auto-update (host writes updated INIT directly to guest boot
  disk)
