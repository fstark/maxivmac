# LOG — Subsystem Logging

## Convention

Each subsystem can emit diagnostic trace lines to stderr, gated by a
compile-time flag.  All lines are prefixed with a three-letter tag in
square brackets so the output can be grepped per-subsystem:

```
[VID] init: boot=640x480 bootDepth=3 bootResMaxDepth=5 vidMem=6291456 numRes=8
[VID] SwitchMode mode=0x83 modeID=100 page=0
```

### Flag naming

Each subsystem defines a `XXX_dolog` macro at the top of its
implementation file:

```c
#define VID_dolog 1   /* 1 = enabled, 0 = disabled */
```

### LOG macro

A matching `XXX_LOG` macro emits a single stderr line:

```c
#if VID_dolog
#define VID_LOG(fmt, ...) std::fprintf(stderr, "[VID] " fmt "\n", ##__VA_ARGS__)
#else
#define VID_LOG(fmt, ...) ((void)0)
#endif
```

All trace output in the subsystem should use the macro.  Setting
`XXX_dolog` to 0 compiles out every trace with zero runtime cost.

### Subsystem tags

| Tag | Subsystem | File | Flag |
|:---:|---|---|---|
| SER | Serial backends | `src/devices/scc.cpp` | `SER_dolog` |
| VID | Video card / display | `src/devices/video.cpp` | `VID_dolog` |

Note: backend attachment lines (`[SER] ch0: PTY backend -> ...`) are
always printed regardless of `SER_dolog`, so the user always sees which
PTY to connect to.

New subsystems should follow the same pattern: three uppercase letters,
a `_dolog` flag, and a `_LOG` macro.

## Relationship to dbglog

The internal `dbglog_WriteNote` / `dbglog_writelnNum` functions write to
the emulator's own debug log file (`dbglog.txt`).  These are a separate
channel from the `[XXX]` stderr trace lines; both may coexist inside the
same `#if XXX_dolog` guard.

## Filtering

```bash
# All video trace:
./maxivmac --model=MacII disk.hfs 2>&1 | grep '^\[VID\]'

# Multiple subsystems:
./maxivmac ... 2>&1 | grep -E '^\[(VID|SCC|VIA)\]'
```
