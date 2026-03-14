# Emulator Debugging Insights

Hard-won lessons from debugging a Mac Plus emulator (maxivmac). Intended as
a foundation for a reusable emulator debugging skill.

---

## 1. Architecture-Layer Thinking

An emulator crash is never "just a crash." Every symptom exists at one of
these layers, and the fix lives at exactly one of them:

| Layer | Example | Typical Evidence |
|-------|---------|-----------------|
| **CPU emulation** | Wrong flag computation, bad address mode | Divergence from known-good emulator on same ROM |
| **Device emulation** | Stub returns wrong value, missing state machine | ROM code enters error path it wouldn't on real hardware |
| **Driver patch / extension** | Replacement driver doesn't intercept a code path | ROM falls through to low-level hardware access |
| **Memory mapping (ATT)** | Address region not mapped for this model | Reads return 0 or bus error where data should exist |
| **Guest OS behavior** | Emulated OS crashes "correctly" due to upstream bug | Crash trace shows valid instruction sequence leading to a legitimate trap |

**Rule:** Before writing any instrumentation, hypothesize which layer the
bug lives in. Write that hypothesis down. When evidence contradicts it,
update the hypothesis *before* adding more instrumentation.

## 2. Instrumentation Strategy

### Start with the lightest touch

1. **PC ring buffer** — A 64K circular buffer of recent PCs costs almost
   nothing at runtime and tells you exactly where the CPU was before a
   crash. This is the single highest-value instrumentation.

2. **Conditional logging** — Only log when PC enters a suspicious range
   (e.g., vector table area < 0x400). Don't trace every instruction
   unless doing a differential comparison.

3. **Per-handler logging** — When you know *which* instruction type causes
   the crash (RTS, JMP, RTE), add logging to that specific handler only.
   Much cheaper than full disasm tracing.

### Avoid these traps

- **Full disasm tracing** slows emulation 100x+ on the host and produces
  tens of millions of log lines (a Mac Plus executes ~8M instructions per
  emulated second). In a deterministic emulator with cycle-counted timers
  and no user input, adding logging **does not change guest behavior** —
  the CPU trace is fully reproducible. But the sheer volume makes it
  impractical to analyze without diffing against a known-good emulator's
  output. Use it only for that purpose.

- **Adding logging to the wrong layer** wastes hours. If the bug is in
  device emulation, logging CPU instructions generates millions of
  irrelevant lines. Narrow the layer first.

- **Static counters and "first N only" guards** are essential. Without
  them, a hot code path produces gigabytes of logs. Always add
  `if (count < N)` or `if (count % 10000 == 0)` guards.

## 3. The Stub Device Problem

Emulators commonly stub out hardware they don't fully emulate. This is
fine *until guest code actually talks to that hardware*. The failure mode
is insidious:

1. Guest code reads from stub device → gets 0x00 (or whatever the
   default is)
2. Guest code interprets 0x00 as valid data
3. Guest code writes that "data" to RAM, corrupting buffers or stack
4. Much later, a seemingly unrelated instruction (RTS, JMP) uses the
   corrupted data and crashes

**The crash site is far from the root cause.** You can spend hours
analyzing the crash instruction when the real bug is thousands of
instructions earlier, in a device read that returned the wrong value.

**Key insight:** On real hardware, an absent or disconnected device
usually reads as 0xFF (all pull-ups high), not 0x00. A stub returning
0x00 can look like "valid data" to guest code, while 0xFF would be
interpreted as "nothing there" and trigger a clean error path. This
single-bit difference can be the difference between a crash and clean
error handling.

## 4. The Error Path Divergence Problem

Even when the primary I/O path works perfectly (replacement driver
handles all normal reads/writes), **error paths can bypass the
replacement driver** and fall through to ROM hardware-access code.

This happens because:
- The replacement driver only patches the *happy path* (Open, Prime,
  Control, Status, Close)
- Mac OS has secondary code paths (disk verification, track cache,
  format operations) that call ROM low-level routines directly
- Error handlers in the ROM may retry operations using low-level
  hardware access, bypassing the driver entirely
- Code loaded from disk (INITs, applications) may call ROM disk
  primitives directly, not through the driver

**Debugging approach:** Log every call to the replacement driver AND
every access to the raw hardware device. If you see hardware accesses
that aren't preceded by a driver call, something is bypassing the
driver.

## 5. Stack Forensics

When a crash involves a bad return address (RTS popping garbage):

1. **Don't assume corruption.** The value on the stack might be a
   legitimate parameter that was *supposed* to be cleaned up before
   the RTS. Dump the stack at the same point during a *successful*
   execution to compare.

2. **Callee-pops conventions** are common in Mac ROM code. The called
   function pops its own parameters (e.g., `MOVEA.L (A7)+, A0` /
   `ADDA.W #N, A7` / `JMP (A0)`). If the function returns early via
   an error path that skips the cleanup, the caller's parameters are
   still on the stack and the return address is wrong.

3. **24-bit vs 32-bit addressing** on 68000: addresses are masked to
   24 bits. `0xE00250F6` and `0x000250F6` are the same address. Don't
   be confused by the high byte.

## 6. Differential Debugging

When you have access to a known-good implementation (e.g., the original
minivmac), differential debugging is the nuclear option:

- Run both emulators with identical ROM, disk, and input
- Log PC + registers at each instruction (or a hash thereof)
- Diff the output to find the first divergence

**When to use it:** Only after targeted instrumentation has narrowed
the bug to "the emulated system is doing something different" but you
can't figure out *what*. It's expensive (huge logs, slow execution)
but definitive.

**When NOT to use it:** If you already know *where* the crash happens
and just need to understand *why*. Targeted per-handler logging is
cheaper and faster.

## 7. Disk-Specific vs. Universal Bugs

Always test with multiple disk images early. If a bug is disk-specific:

- The bug is likely in the guest OS code loaded from that disk, not
  in the emulator itself
- The disk may trigger a code path that other disks don't (e.g.,
  INITs, Extensions, unusual file system structures)
- The emulator may be *correctly emulating* hardware that the guest
  code handles incorrectly when it encounters an unexpected device
  response

If a bug is universal (all disks crash):
- The bug is almost certainly in the emulator (CPU, device, or memory
  mapping)

## 8. Knowing When to Stop

This is the hardest skill. Signs you should stop iterating and
hand off your findings:

- **You've patched the same function 3+ ways** and none fixed it —
  your mental model of the code is wrong
- **Each fix attempt produces the exact same crash trace** — you're
  patching downstream of the root cause
- **You're modifying ROM code** to work around guest OS behavior —
  you've crossed from "fixing the emulator" to "fighting the
  software"
- **You've identified what happens but not why it happens** — write
  down the *what* and let someone with domain knowledge tackle the
  *why*

The most valuable output of a debugging session is often not a fix
but a **precise description of the failure mechanism** with evidence.
"RTS at address X pops value Y from the stack because function Z
doesn't clean up on error path" is actionable. "Something corrupts
the stack" is not.

## 9. Emulator-Specific Gotchas

### Memory-mapped I/O side effects
Reading a device register to "inspect" it can change the device state.
The disassembler and debugger must use a *separate* memory access path
that doesn't trigger device handlers. (In minivmac, `disasm.cpp` has
its own memory access functions for this reason.)

### Interrupt timing sensitivity
Many guest OS bugs are latent and only manifest with specific interrupt
timing. Adding debug logging changes timing, which can make bugs appear
or disappear. If a bug goes away when you add logging, it's a timing
bug.

### ROM checksums and patches
Many ROMs check their own checksum at boot. If you patch the ROM to
fix a bug, you must also disable the checksum check or the ROM will
refuse to boot. Look for the checksum skip early in the init sequence.

## 10. The Debugging Session Workflow

1. **Reproduce** — Confirm the crash is deterministic. Same ROM, same
   disk, same input sequence → same crash.
2. **Capture** — Get the PC, registers, and stack at crash time.
   Minimum viable instrumentation.
3. **Trace backward** — PC ring buffer or saved-PC dump. Find the
   instruction that *caused* the bad PC, not the instruction that
   *crashed*.
4. **Hypothesize the layer** — CPU bug? Device bug? Driver bug?
   Memory mapping? Write it down.
5. **Test the hypothesis** — One targeted experiment. Not three.
6. **Update or hand off** — If the hypothesis was wrong, update it
   with what you learned. If you've done 3 rounds without progress,
   document findings and hand off.
