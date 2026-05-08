# INIT Versioning

Report the guest INIT's identity, version, and environment to the host
at boot time.  Enable the overlay to display INIT status and detect
stale builds.

## Problem

The INIT is compiled inside the guest with THINK C and baked into HFS
disk images.  When the host binary is updated, the INIT on the user's
boot disk may be out of date.  Today there is no way for the host to
know:

- Whether an INIT loaded at all.
- What version of the INIT is running.
- Whether the INIT speaks a compatible protocol.
- What guest OS and machine the INIT is running on.

## Design

### Single boot command — `kCmdInitIdent` ($0001)

A new system-level register command in the `$0xx` range (separate from
`$1xx` clipboard and `$2xx` shared drive).

**Guest sends:**

| Param | Type    | Content                                      |
|-------|---------|----------------------------------------------|
| p0    | integer | `kApiVersion` — monotonic protocol version    |
| p1    | pointer | Version string (Pascal, e.g. `"\pv1.0-a3b7c9f"`) |
| p2    | integer | `vRefNum` of INIT's own file                  |
| p3    | integer | `dirID` of INIT's parent directory             |
| p4    | pointer | Filename of INIT (Pascal, from `PBGetFCBInfo`) |
| p5    | integer | `machineType` (from `SysEnvirons`)            |
| p6    | integer | `systemVersion` (BCD, from `SysEnvirons`)     |

**Host returns:**

| Field  | Value | Meaning                              |
|--------|-------|--------------------------------------|
| result | 0     | Compatible — proceed with boot       |
| result | ≠ 0   | Incompatible — INIT must bail        |

If the host returns non-zero, the INIT logs a message and exits
without installing traps or the jGNEFilter.  The host still stashes
the reported version so the overlay can display it.

### Replaces per-feature version checks

The existing `ExtFSVersion` ($0200) and `ClipVersion` ($0100) checks
are removed from the INIT boot sequence.  API compatibility is decided
once by the host in `kCmdInitIdent`.  The INIT installs all features
(traps, jGNEFilter, clipboard polling) unconditionally if the API
version is accepted.

In particular, `ExtFSVersion` returning 0 when no shared directory was
configured used to prevent trap installation entirely.  This special
case is removed: traps are always installed.  When no drives are
mounted the trap handlers pass through to ROM (they already do this
for non-shared volumes).  Drives appear whenever the host mounts them
— at boot or later — via the existing `PollMounts` mechanism in the
jGNEFilter.  There is no difference between "shared drive configured
at launch" and "shared drive added later."

The host continues to handle the old `$0200` / `$0100` commands for
backward compatibility with INITs that predate this spec.

## Version string format

The INIT version string uses the same format as the host:
`git describe --tags --match "v*" --always`, prefixed with `dev-` if
no tag is present.

Examples: `v1.0`, `v1.0-3-ga3b7c9f`, `dev-a3b7c9f`, `dev`.

## Build pipeline

| File                 | Tracked | Content                                    |
|----------------------|---------|--------------------------------------------|
| `version.h.default`  | yes     | `#define kInitVersion "\pdev"`             |
| `version.h`          | no (.gitignore) | Generated with real git version   |

**`build.sh`** (host-side, before launching emulator):

1. Run `git describe --tags --match "v*" --always` to get the version.
2. Write `version.h` with `#define kInitVersion "\p<version>"`.
3. Launch emulator with shared drive pointing at `macsrc/init/`.
4. Run `build.dbg` to compile the INIT inside the guest.

**Manual builds:** Copy `version.h.default` to `version.h`.  The INIT
reports `"dev"`.

## Guest boot sequence

```
main():
  1. RememberA0 / SetUpA4
  2. If Shift key held → dbg_log "skipped by user" → bail
  3. self = GetResource('INIT', 128)
  4. refNum = HomeResFile(self)
  5. PBGetFCBInfo(refNum) → vRefNum, dirID, fileName
  6. DetachResource(self) / HLock / HNoPurge
  7. SysEnvirons(1, &env) → machineType, systemVersion
  8. Store file spec + env in Globals
  9. reg_command(kCmdInitIdent) with p0–p6
 10. If result ≠ 0 → bail
 11. Install traps, jGNEFilter, clipboard — unconditionally
```

Shift-skip follows standard Mac INIT convention: if the user holds
Shift during startup, Extensions are disabled.  The INIT checks
the keyboard modifier state early and bails without installing.  A
`dbg_log` is sent to the host so the overlay can show the INIT was
present but skipped (the host receives the log but no `kCmdInitIdent`,
so overlay shows "INIT: not loaded").

## Host side

### New file: `extn_system.cpp` / `extn_system.h`

Handles the `$0xx` command range.  Exposes an `InitInfo` class:

```cpp
class InitInfo {
public:
    bool        loaded() const;
    std::string version() const;
    int         apiVersion() const;
    bool        isStale() const;   // version != MAXIVMAC_VERSION

    // Guest environment
    int         machineType() const;
    int         systemVersion() const;  // BCD

    // File spec (for future auto-update)
    int16_t     vRefNum() const;
    int32_t     dirID() const;
    std::string fileName() const;
};
```

A module-level function provides the dispatch entry point and access
to the singleton:

```cpp
void ExtnSystemDispatch(/* register params */);
const InitInfo& ExtnSystemInitInfo();
```

### Compatibility check

The host maintains a `kMinApiVersion` and `kMaxApiVersion`.  If the
guest's `kApiVersion` falls within the range, result = 0
(compatible).  Otherwise result = non-zero (bail).

Bumping `kApiVersion` is required only when register commands are
added, removed, or change semantics.  Code-only bug fixes do not
require an API version bump.

## Overlay display

Three states:

| State        | Condition                       | Display                        | Color  |
|--------------|---------------------------------|--------------------------------|--------|
| Not loaded   | No `kCmdInitIdent` received     | `INIT: not loaded`             | gray   |
| Current      | Version matches host            | `INIT: v1.0`                   | normal |
| Stale        | Version differs from host       | `INIT: v0.9`                   | yellow |

The overlay also displays environment info from the INIT:
```
Mac II · System 7.0.1 · INIT v1.0
```

## Known issues (fix alongside this work)

### TickCount wraparound stalls polling

`TickCount()` wraps after ~9 hours of guest time.  The unsigned
subtraction `TickCount() - g->lastPollTick >= N` produces a huge
value after wrap, stalling polling until the next wrap.  Fix:
cast both sides to `unsigned long` and let wraparound arithmetic
work naturally — or simply poll every tick (see below).

### Poll interval is too slow

The jGNEFilter currently throttles `PollMounts` + `PollGuestCmd` to
every 60 ticks (~1 second).  This adds unnecessary latency to mount
events and host→guest commands.  Reduce to every tick or a small
handful — the register read is cheap (a single memory-mapped access).

### KV store not cleared on reboot

The host-side KV store (`s_kvStore` in `extn_clip.cpp`) persists
across guest reboots.  Stale sequence numbers cause clipboard sync to
miss updates.  Fix: when `kCmdInitIdent` is received, the host clears
state in all subsystems (system, clip, drive) — treating `InitIdent`
as a session-reset signal.

### Resource ID 314 → 128

The INIT resource currently uses `GetResource('INIT', 314)` — a
legacy ID from the old SharedDrive-only INIT.  Move to ID 128
(standard for a single INIT resource) as part of this work.

### Clipboard errors are silent

`SyncClipboard()` returns void and ignores all errors from
`ExportMacToHost` / `ImportHostToMac`.  If clipboard transfer fails,
nothing is logged.  Add `dbg_log` calls on error paths so failures
are visible in the debug console.

## Future work (parked)

### Protocol cleanup — unify under `$0xx`

Several mechanisms currently scattered across `$1xx` and `$2xx` are
really system-level concerns:

| Current location | What | Should be |
|------------------|------|-----------|
| `$0108` (clip) `kClipDbgLog` | Debug logging | `$0xx` — single system log |
| `$0220` (drive) `kExtFSGuestCmd` | Launch, ExitToShell, Shutdown | `$0xx` — system commands |
| jGNEFilter polls `PollMounts` + `PollGuestCmd` separately | Host→guest channel | `$0xx` — single poll, dispatch by command type |

Target architecture:
- **Single system log** in `$0xx`, used by system, clip, and drive.
- **Single host→guest command channel** in `$0xx`.  The guest polls
  one command, gets a command code + payload, and dispatches to the
  appropriate subsystem (system, clip, drive).
- **System commands** (launch, ExitToShell, shutdown) move to `$0xx`.
- `$1xx` and `$2xx` become pure feature protocols with no system
  plumbing mixed in.

### Auto-update

The INIT's file spec (vRefNum, dirID, name) is already captured at
boot.  A future "copy file to guest" command could overwrite the INIT
file on the user's boot disk and trigger a reboot.  The INIT file is
not open after boot (the System closes it after loading resources), so
overwriting it is safe.

### `vers` resource self-stamping

The INIT could write a `vers` resource into its own file at first boot
using the captured `HomeResFile` refnum, so Finder's Get Info shows
the correct version.  Since `kInitVersion` is already compiled in, no
additional data transfer is needed.

### Generic file transfer

A "write file to guest path" command would enable drag-and-drop from
host to guest desktop.  This is a separate feature that shares the
chunked register-block transfer mechanism but differs in targeting
(arbitrary path vs. INIT's own file).
