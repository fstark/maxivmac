# Trap Tracing

Hierarchical tracing of Mac OS A-line traps with decoded parameters.

## What

Replace the flat `[TRAP] $A9A0 GetResource` one-liner with rich,
nested output showing entry/exit, parameters, caller, timing, and
current application:

```
→ 1230000 [3] GetResource(resType:'DRVR', resID:$0008) [caller:$00412E]
  → 1230010 [3] NewHandle(size:$00000100) [caller:$013E02]
  ← 1230050 [3] NewHandle → h:$00F234→$050000 err:0  (+40)
← 1230100 [3] GetResource → rsrc:$00F234→$050000  (+100)
```

## Key Concepts

- **External trap definitions** (`assets/traps.def`) — trap signatures
  (name, convention, typed in/out params) in a plain-text file, editable
  without recompiling.
- **TrapDefs class** — loads the def file at startup, lookup by trap word.
- **TrapTracer class** — nesting stack, parameter formatting, entry/exit
  emission. Called from `DoCodeA()` and the instruction loop.
- **Return detection** — save return PC on entry, match against current PC
  each instruction.
- **Non-returning traps** (ExitToShell, Launch) — flush the nesting stack
  with `[abandoned]` markers.
- **MultiFinder awareness** — detect `CurApRefNum` changes, flush stale
  frames, emit context-switch separator. AppId shown on every line.

## Design

See [TRACING_DESIGN.md](TRACING_DESIGN.md).
