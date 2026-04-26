# Diagnostic Logging

## Overview

maxivmac has a runtime diagnostic trace system.  Each subsystem has a
named channel that can be turned on or off at runtime — no recompilation
needed.  Log lines go to stderr, prefixed with the channel tag in
square brackets:

```
[EXTFS] PbOpen dir=2 name="ReadMe"
[VID] SwitchMode mode=0x83 modeID=100 page=0
```

## Channels

| Tag | Subsystem | Description |
|:---:|---|---|
| EXTFS | Shared drive | Virtual filesystem operations, guest INIT traces, pass-through traps |
| CLIP | Clipboard | Clipboard extension messages |
| SER | Serial | SCC serial port activity |
| NET | Networking | libslirp / network backend |
| SLIP | SLIP | Serial Line IP codec |
| VID | Video | Video card emulation (Mac II) |

## Enabling channels

### Command line

Use `--diag=` with a comma-separated list of channel names (case-insensitive):

```bash
# Single channel:
./maxivmac --model=MacII --diag=extfs disk.hfs

# Multiple channels:
./maxivmac --model=MacII --diag=extfs,vid disk.hfs

# Everything:
./maxivmac --model=MacII --diag=all disk.hfs
```

### Debugger

The `diag` command controls channels at runtime:

```
diag              # list all channels and their state
diag extfs on     # enable EXTFS tracing
diag vid off      # disable VID tracing
diag all on       # enable all channels
diag all off      # disable all channels
```

## Filtering

```bash
# Show only EXTFS lines:
./maxivmac --diag=extfs ... 2>&1 | grep '^\[EXTFS\]'

# Multiple subsystems:
./maxivmac --diag=extfs,vid ... 2>&1 | grep -E '^\[(EXTFS|VID)\]'
```
