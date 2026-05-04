# SharedDrive INIT — Automated Mac Build

Build the SharedDrive INIT inside the emulator itself, proving the
emulator can host its own development workflow.

## Overview

```
VS Code (edit init.c)
  → SharedDrive (host filesystem)
    → THINK C (guest, compiles on SharedDrive volume)
      → built INIT appears on SharedDrive as AppleDouble
        → commit to git
          → AppleDouble→MacBinary converter (host tool)
            → hcopy -m into release .hfs disk
```

The emulator is both the build tool and the product.  Source lives in
`macsrc/shareddrive/`, is edited in VS Code, and is visible to the
guest via SharedDrive.  THINK C compiles it inside the emulated Mac.
The built artifact (an INIT code resource) lands back on SharedDrive
as an AppleDouble file, ready for git commit.

## Files

| File | Purpose |
|------|---------|
| `macsrc/shareddrive/init.c` | INIT source (edited in VS Code) |
| `macsrc/shareddrive/build.script` | Automation script (drives THINK C) |
| `macsrc/shareddrive/thinkc-build.hfs` | Boot disk: System 6.0.8 + THINK C (committed binary) |
| `shared/Shared Drive Project/` | THINK C project + built output (AppleDouble on host) |

## Build disk setup (one-time, manual)

Create `thinkc-build.hfs` with:

- System 6.0.8, non-MultiFinder
- THINK C installed, set as Startup Application (no Finder)
- Cmd-K shortcut added to "Build Code Resource…" menu item via ResEdit
- SharedDrive INIT installed in System file (so the shared/ volume mounts at boot)

This is done once by hand, committed as a binary artifact.

## Script language

See [docs/features/SCRIPTING.md](../../docs/features/SCRIPTING.md) for
the full scripting language reference.

Scripts are plain text files on the host, passed via `--script <path>`.
The emulator runs at max speed, headless.  On failure: non-zero exit,
stderr shows what was expected vs what happened, screenshot saved to disk.

## Example build script

```
# build.script — Build SharedDrive INIT with THINK C
# Boot disk: thinkc-build.hfs (THINK C as startup app)
# SharedDrive: shared/ (contains project + source)

timeout 5

# THINK C launches, shows Open dialog
expect "Open"
key tab                            # switch to SharedDrive volume
type "Shared Drive"
key return                         # open the project file

# Project opens, source window visible
expect "SharedDrive.c"

# Build
key cmd-K                          # Build Code Resource…

# Compilation runs, then "Save code resource as:" dialog
expect "Save code resource" timeout 10
key return                         # accept default name

# Back to editor = build complete
expect "SharedDrive.c"

# Done — artifact is on SharedDrive as AppleDouble
shutdown
```

## Host invocation

```sh
# Build the INIT
./bld/macos/maxivmac \
  -disk macsrc/shareddrive/thinkc-build.hfs \
  -share shared/ \
  --script macsrc/shareddrive/build.script

# Check result
echo $?   # 0 = success, 1 = script failure
```

## Release pipeline

The built INIT lands in `shared/Shared Drive Project/` as AppleDouble:

- `Shared Drive Maxi vMac` — 0-byte data fork
- `._Shared Drive Maxi vMac` — resource fork (INIT/SDvM, ~10KB)

To get this into an HFS disk image for release:

1. **AppleDouble→MacBinary converter** — small host-side C++ tool (~200 LOC)
   that reads the AppleDouble sidecar, packages data+resource forks into
   MacBinary format with correct type/creator.

2. **hcopy** — `hcopy -m init.bin disk.hfs:` copies MacBinary into HFS image.

hcopy (hfsutils) doesn't support AppleDouble directly — only MacBinary
(`-m`), BinHex (`-b`), text (`-t`), raw (`-r`).  Hence the converter.

## Implementation order

1. **Keystroke injection** (J1) — host posts into emulator key queue
2. **Text trap watching** (F8) — hook DrawString/SFPutFile/ParamText
3. **Script parser + runner** — parse `build.script`, execute commands
4. **`--script` mode** — headless, max speed, non-zero exit on failure
5. **Screenshot on failure** — save framebuffer as PNG on timeout
6. **Screen comparison** (F7) — `expect screen` with framebuffer compare
7. **`on` handlers** (L2) — reactive pattern matching
8. **AppleDouble→MacBinary converter** — host-side release tool
9. **INIT-side shutdown** — register-block command to exit emulator

## Open questions

- Exact Cmd-K shortcut in THINK C — verify with ResEdit what menu item
  ID to patch.  Might need Cmd-B or another unused combo.
- SFGetFile dialog navigation — TAB switches drives, but may need
  multiple TABs if more than 2 volumes are mounted.  Test manually first.
- "Replace existing?" dialog — will appear on second+ builds.  Handled
  by future `on "Replace" { key return }`.  For MVP, delete the old
  output before building, or accept the dialog manually in the script
  with an extra `expect`/`key return` pair.
- THINK C project file (.π) churn — the sidecar may change on every
  build.  Acceptable; just commit it.