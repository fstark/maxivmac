# Clipboard Sync

Automatic, transparent clipboard synchronization between the emulated
Mac and the host.  No user action required — cut/copy on either side
is visible on the other.

## I/O Register Interface

Two I/O blocks in the extension address space.  The legacy mechanism
(`extnBlockBase`, `$F0C000` on most models) keeps its 32 bytes
untouched.  The new register interface lives 32 bytes later at
`extnBlockBase + $20` (`$F0C020` on most models).  Both are backed
by the same ATT region; the handler dispatches by offset.

```
Legacy block (unchanged):   $F0C000 – $F0C01F
New register block:         $F0C020 – $F0C03F
```

New register layout:

```
Offset  Size  Name       Direction   Description
------  ----  ----       ---------   -----------
+$00    word  command    write(68k)  Function code; write triggers call
+$02    word  result     read(68k)   Return code (0 = ok)
+$04    long  p0         read/write  Parameter 0
+$08    long  p1         read/write  Parameter 1
+$0C    long  p2         read/write  Parameter 2
+$10    long  p3         read/write  Parameter 3
+$14    long  p4         read/write  Parameter 4
+$18    long  p5         read/write  Parameter 5
+$1C    long  p6         read/write  Parameter 6
```

The write to `+$00` is the trigger.  All other registers can be
written in any order before that.  Reads happen after the trigger
returns.

Both mechanisms coexist permanently.  The Sony driver and any legacy
Mac-side utilities keep using `$F0C000`.  New code (clipboard INIT,
ExtFS INIT) uses `$F0C020`.

## Clipboard Commands

```
Command       Code   Parameters                    Returns
-----------   ----   ----------                    -------
ClipVersion   $100   —                             p0 = version (1)
ClipExport    $101   p0 = guest buffer address     result
                     p1 = byte count
                     (text in Mac OS Roman)
ClipImport    $102   p0 = guest buffer address     result
                     p1 = buffer capacity           p1 = actual byte count
                     (fills buffer with Mac OS      (text in Mac OS Roman)
                     Roman text)
ClipHasData   $103   —                             p0 = 1 if host has text
ClipGetLen    $104   —                             p0 = byte count of host
                                                   clipboard text (in Mac
                                                   OS Roman)
ClipSeqNo     $105   —                             p0 = host clipboard
                                                   sequence number (bumped
                                                   on every host clipboard
                                                   change)
```

The C++ side does encoding conversion (UTF-8 ↔ Mac OS Roman) and
calls `SDL_GetClipboardText` / `SDL_SetClipboardText`.

No pbufs.  The 68k side allocates a buffer (`_NewPtr`), passes its
address.  The C++ side reads/writes guest RAM directly via
`get_vm_byte` / `put_vm_byte`.

## Mac Side — INIT

A small 68k INIT, loaded at boot from the System file (or injected
into the system heap by the emulator via a ROM patch).

### What it does

1. **At load time:**  patch `_ZeroScrap` and `_PutScrap` traps.

2. **`_PutScrap` patch:**  call through to the real `_PutScrap`.
   If the type is `'TEXT'`, read the scrap data and call
   `ClipExport` to push it to the host.

3. **Periodic host→Mac sync:**  hook into the main event loop via
   `_GetNextEvent` or `_WaitNextEvent` patch (or a jGNEFilter).
   On each call, check `ClipSeqNo`.  If it changed since last
   check:
   - Call `ClipGetLen` to get the size.
   - Allocate a buffer, call `ClipImport`.
   - Call `_ZeroScrap` (bypass our own patch to avoid re-export)
     then `_PutScrap` with type `'TEXT'`.

That's it.  No background tasks, no DA, no application.

### 68k code size estimate

The INIT is essentially:

- Trap patches for `_PutScrap`, `_ZeroScrap`, `_GetNextEvent` — a
  few `GetTrapAddress` / `SetTrapAddress` calls in the INIT body.
- The `_PutScrap` patch: ~20 instructions (check type, get scrap
  data, set up registers, trigger I/O, call through).
- The GNE filter: ~30 instructions (read `ClipSeqNo`, compare,
  get length, allocate, trigger I/O, call `PutScrap`, deallocate).

Total: well under 1 KB of 68k code.

### I/O calling convention (68k side)

```asm
; Example: ClipExport (push Mac clipboard to host)
;   a0 = pointer to text data
;   d0 = byte count

    move.l  a0, $F0C024       ; p0 = buffer address
    move.l  d0, $F0C028       ; p1 = byte count
    move.w  #$0101, $F0C020   ; command = ClipExport (triggers call)
    move.w  $F0C022, d0       ; read result
```

Four instructions.  Compare with the current ExtnGlue.i approach:
read SonyVarsPtr, chase pointer, validate checkval, fill 32-byte
parameter block, poke address, recheck checkval, read result.

## C++ Side

```cpp
// In the extension handler init:
extn_register(0x101, [](ExtnRegs& r) {
    uint32_t buf   = r.p0();
    uint32_t count = r.p1();

    // Read Mac OS Roman text from guest RAM
    std::string text(count, '\0');
    for (uint32_t i = 0; i < count; i++)
        text[i] = get_vm_byte(buf + i);

    // Convert Mac OS Roman → UTF-8, push to host
    std::string utf8 = mac_roman_to_utf8(text);
    SDL_SetClipboardText(utf8.c_str());

    r.set_result(0);
});
```

The `ExtnRegs` object holds the 7 parameter longs and the result
word.  ATT writes fill the fields; ATT reads return them.  The
command write invokes the registered handler synchronously.

## Incremental Plan

### Milestone 1 — Register I/O infrastructure

Add the register-backed I/O handler at `extnBlockBase + $20`,
alongside the existing extension mechanism at `extnBlockBase`.
The ATT handler dispatches by offset: writes below `+$20` go to the
legacy path, writes at `+$20` and above go to the new lambda
dispatch.  No functional change yet.

### Milestone 2 — ClipExport (Mac → host)

Implement `ClipExport` on the C++ side.  Write a tiny test: a Mac
program that calls `ClipExport` with a hardcoded string.  Verify it
appears in the host clipboard.

### Milestone 3 — ClipImport (host → Mac)

Implement `ClipImport`, `ClipHasData`, `ClipGetLen`, `ClipSeqNo` on
the C++ side.  Test with a Mac program that polls and prints.

### Milestone 4 — INIT with automatic sync

Write the INIT: patch `_PutScrap` for Mac→host, jGNEFilter for
host→Mac.  Install it in the System file on the boot disk (or
inject via ROM patch).  Clipboard is now transparent.
