---
name: trap-debugging
description: "Debug classic Mac OS trap-level issues using the maxivmac built-in debugger. Use when: diagnosing File Manager errors, tracing Toolbox trap calls, capturing what the guest OS does before an error dialog, setting breakpoints on traps like Alert or StopAlert, writing .dbg debug scripts, or investigating why a guest INIT or File Manager patch fails."
argument-hint: "Describe the symptom, e.g. 'Finder shows Unknown Error on Duplicate' or 'trace what happens after _Open'"
---

# Trap-Level Debugging with the maxivmac Debugger

Debug classic Macintosh system-level issues by tracing and breaking
on OS/Toolbox trap calls inside the emulator.

## Architecture

The emulator has a built-in debugger with a client-server model:

- **Server**: launched with `--debugserver` flag on the emulator binary.
  Listens on `/tmp/maxivmac-dbg-{PID}.sock`.
- **Client**: `./bld/macos/maxivmac debug` auto-discovers the socket.
- **Modes**: one-shot (`debug "cmd"`), interactive (`debug`),
  script (`debug --script=file.dbg`).

## Typical Workflow

### 1. Launch emulator with debug server

```sh
rm -f shared/Desktop shared/Desktop.rsrc /tmp/maxivmac-guest.log
./bld/macos/maxivmac 608.hfs --model=MacII --debugserver 2>/tmp/maxivmac-guest.log
```

Redirect stderr to a file — all `[GUEST]` and `[ExtFS]` log lines
go there.  The emulator window appears and boots normally.

### 2. Connect debugger and run a script

```sh
sleep 3 && ./bld/macos/maxivmac debug --script=my_debug.dbg > /tmp/trace.txt 2>&1
```

The `sleep` gives the emulator time to boot.  Script output
(trap trace lines) goes to the output file.

### 3. Perform the user action in the emulator

Click, drag, Cmd-D, whatever triggers the bug.  The debugger
captures traps in real time and stops at breakpoints.

### 4. Analyse the trace

```sh
# Find File Manager calls near the error
grep -n 'Open\|Close\|Read\|Write\|Create\|Delete\|FlushFile\|HFSDispatch\|Alert' /tmp/trace.txt | tail -30

# See the exact sequence around the crash
sed -n '657880,657960p' /tmp/trace.txt
```

## .dbg Script Syntax

One command per line.  Lines starting with `#` are comments.
Empty lines are ignored.

```
# Enable full trap tracing, break when Finder shows an error dialog
trace traps on
break StopAlert
break Alert
continue
```

### Key commands for scripts

| Command | Purpose |
|---------|---------|
| `trace traps on` | Log every Toolbox/OS trap with arguments and return values |
| `trace traps off` | Stop logging |
| `trace traps GetResource OpenRF` | Log only named traps |
| `break <trap>` | Break when a specific trap is called |
| `break #<N>` | Break at instruction count N |
| `continue` | Resume until next breakpoint |
| `run` | Start execution (use instead of `continue` if stopped at boot) |
| `step 50` | Execute 50 instructions with logging |

### Useful trap names for breakpoints

| Trap | When to use |
|------|-------------|
| `Alert` / `StopAlert` / `NoteAlert` / `CautionAlert` | Break when an error dialog appears |
| `ExitToShell` | App is quitting or crashing to Finder |
| `SysError` | System error / bomb dialog |
| `HFSDispatch` | Any HFS call (_GetCatInfo, _OpenWD, etc.) |

Find trap names with: `./bld/macos/maxivmac debug "info traps Alert"`

## Diagnosing Specific Issues

### "Unknown Error" dialogs

The Finder loads error strings from `STR# 10240` and calls `Alert`.
To capture the full trap sequence leading to the error:

```
# debug_error.dbg
trace traps on
break StopAlert
break Alert
continue
```

Then search the trace backwards from the Alert for the FM call
that returned an error.  Common pattern:

```
→ FlushFile?()        ← falls through to real FM, returns error
→ GetResource('STR#', 10240)   ← Finder loading error string
→ ParamText?()
→ Alert?()            ← BREAKPOINT HIT
```

### Unhandled trap pass-throughs

When a patched INIT doesn't intercept a trap, it falls through
to the real File Manager which doesn't understand our virtual FCBs
or VCBs.  Symptoms: mysterious errors, nsvErr (-35), rfNumErr (-51).

**Diagnosis**: enable trap tracing, look for FM traps that appear
in the trace *without* a corresponding `[GUEST] SD _Xxx` log line
in stderr.  That trap is not intercepted.

**Fix**: add a handler in `DispatchFlat` and `InstallFlatPatch`.

### Dead guest — no response after action

If the emulator freezes with the debugger attached:

```sh
./bld/macos/maxivmac debug "info insn"
```

If the instruction count is stuck, the CPU is in a tight loop or
waiting for an event.  Use `info reg` and `x/i pc` to see where.

## Examining State at a Breakpoint

When the debugger stops:

```
info reg              # CPU registers
x/i pc                # Current instruction
bt                    # Heuristic backtrace
x/4lx sp              # Top of stack
print (a0 + 16).w     # Read ioResult from param block
log 20                # Last 20 guest console log lines
log grep "SD _"       # Filter for SharedDrive INIT logs
```

## Guest Log Integration

The SharedDrive INIT writes debug logs via command `0x20D`, which
appear on stderr as `[GUEST] SD ...` lines.  The emulator's ExtFS
handler writes `[ExtFS] ...` lines.

When both trap trace and guest log are needed:

- Trap trace → stdout (redirect to file via script output)
- Guest log → stderr (redirect via `2>/tmp/maxivmac-guest.log`)

Correlate them by matching the operation sequence:
guest log shows `SD _Open vr=-32000 nm=README.txt`,
trap trace shows `→ Open(pb:$807C799C)` at a nearby instruction count.

## Memory Examination

```
x/16bx $0356          # VCB queue header
x/s $7E9748           # Pascal string at address
x/4lx sp              # Stack longwords
x/10i pc              # Disassemble 10 instructions at PC
find $400000 $500000 "README"   # Search for ASCII in ROM/RAM
```

## Watchpoints

Break on memory access — useful for tracking who corrupts a
low-memory global:

```
watch $0B06 4         # Break on write to ROMMapHndl (4 bytes)
awatch $0356 2        # Break on any access to VCB queue header
```

## Quick Reference: Common Trap Words

| Trap Word | Name | Category |
|-----------|------|----------|
| `$A000` | _Open | File Manager |
| `$A001` | _Close | File Manager |
| `$A002` | _Read | File Manager |
| `$A003` | _Write | File Manager |
| `$A009` | _Delete | File Manager |
| `$A00A` | _OpenRF | File Manager |
| `$A013` | _FlushVol | File Manager |
| `$A044` | _SetFPos | File Manager |
| `$A045` | _FlushFile | File Manager |
| `$A260` | _HFSDispatch | File Manager (HFS) |
| `$A985` | Alert | Dialog Manager |
| `$A986` | StopAlert | Dialog Manager |
| `$A9A0` | GetResource | Resource Manager |

Use `info traps <prefix>` to search for others.

Scripts examples:

```
# Break at a specific instruction count, log the traps for 50 instructions, then examine the low memory at 0x0AD4
break #28117100
run
trace traps on
step 50
trace traps off
x/1l 0x0AD4
```

```
# Run until the finde has launched then trace a few traps to understand a crashing loop
break #40000000
run
trace traps ExitToShell Launch InitFonts InitWindows OpenRFPerm CloseResFile
break #80000000
continue
```
