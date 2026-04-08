# Clipboard Sync Plan

**All 4 phases completed** on 4 April 2026.
**INIT cleaned up** on 9 April 2026 — stripped dead code, added
proper filter chaining, documented private-scrap limitation.

Commit range: `1f126a0..b74ef72`

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Register I/O infrastructure (extnBlockBase+$20) | Done |
| 2 | Clipboard commands (extn_clip.cpp + clipboard.h wrappers) | Done |
| 3 | THINK C console app (macsrc/clipsync/main.c) | Done |
| 4 | Documentation updates | Done |

Host-to-Mac clipboard transfer using a new register-based I/O interface.
No pbufs — the host writes directly to guest RAM.  The Mac side is a
one-shot THINK C console app (later extended to bidirectional and INIT).

Build gate: `cmake --build bld/macos-imgui && cmake --build bld/macos-headless`
Test gate:  `cd test && ./verify.sh`

---

## Phase 1 — Register I/O infrastructure

Add a new 32-byte register block at `extnBlockBase + $20`, alongside the
existing 32-byte legacy extension block at `extnBlockBase + $00`.
The ATT handler dispatches by word offset: offsets 0–15 go to the legacy
path, offsets 16–31 go to the new register handler.  No functional
change to existing code paths.

### Register layout (byte offsets relative to `extnBlockBase + $20`)

```
Offset  Size  Name       Direction   Description
------  ----  ----       ---------   -----------
+$00    word  command    write(68k)  Function code; write triggers dispatch
+$02    word  result     read(68k)   Return code (0 = ok)
+$04    long  p0         read/write  Parameter 0
+$08    long  p1         read/write  Parameter 1
+$0C    long  p2         read/write  Parameter 2
+$10    long  p3         read/write  Parameter 3
+$14    long  p4         read/write  Parameter 4
+$18    long  p5         read/write  Parameter 5
+$1C    long  p6         read/write  Parameter 6
```

Since the 68k bus is big-endian and word-addressed, each long occupies
two consecutive word offsets (high word first).  The ATT sees word-level
accesses.  The command write at +$00 is the trigger — all parameters
must be written before it.

### 1.1 — Increase extension block size for compact Macs

**File:** `src/core/machine_config.h`

Change `extnLn2Spc` default from `5` (32 bytes → offsets 0–15) to `6`
(64 bytes → offsets 0–31).  This gives the ATT enough address space for
both the legacy block and the new register block.

Mac II family already has 8 KB via `io32::kMask = 0xFF01E000`; no
change needed there.  Only the compact-Mac path in
`SetUp_address_compact()` uses `extnLn2Spc` for its ATT mask.

**Change:**
```cpp
// machine_config.h
uint8_t  extnLn2Spc    = 6;    // was 5 — 64 bytes (legacy + register blocks)
```

### 1.2 — Widen the address mask in kMMDV_Extn dispatch

**File:** `src/core/machine.cpp`, `kMMDV_Extn` case (~line 1379)

The current mask `(addr >> 1) & 0x0F` truncates to 4 bits (word offsets
0–15).  Change to `(addr >> 1) & 0x1F` (5 bits, offsets 0–31) so the
new register block is reachable.

Also allow reads for the new register area.  The current code rejects
all reads with an `AbnormalID`.  Change to:

```
word offset 0–15  (legacy):  write-only (existing behavior)
word offset 16–31 (new):     read AND write
```

**Before:**
```cpp
case kMMDV_Extn:
    if (byteSize) {
        ReportAbnormalID(AbnormalID::kMACH_Sony_byte, "access Sony byte");
    } else if ((addr & 1) != 0) {
        ReportAbnormalID(AbnormalID::kMACH_Sony_odd, "access Sony odd");
    } else if (! writeMem) {
        ReportAbnormalID(AbnormalID::kMACH_Sony_read, "access Sony read");
    } else {
        p->device->access(data, writeMem, (addr >> 1) & 0x0F);
    }
    break;
```

**After:**
```cpp
case kMMDV_Extn:
    if (byteSize) {
        ReportAbnormalID(AbnormalID::kMACH_Sony_byte, "access Sony byte");
    } else if ((addr & 1) != 0) {
        ReportAbnormalID(AbnormalID::kMACH_Sony_odd, "access Sony odd");
    } else {
        uint32_t wordOff = (addr >> 1) & 0x1F;
        if (!writeMem && wordOff < 16) {
            ReportAbnormalID(AbnormalID::kMACH_Sony_read,
                "access Sony read");
        } else {
            data = p->device->access(data, writeMem, wordOff);
        }
    }
    break;
```

### 1.3 — Pass writeMem through ExtnDevice; return read values

**File:** `src/core/machine.cpp`, `ExtnDevice::access()` and `extnAccess()`

Currently `ExtnDevice::access()` ignores `writeMem` and always calls
`extnAccess(data, addr)`.  Change `extnAccess` signature to accept
`writeMem` and return a `uint32_t` (the value for reads).

```cpp
// ExtnDevice
uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override {
    return extnAccess(data, writeMem, addr);
}

// extnAccess signature change
static uint32_t extnAccess(uint32_t data, bool writeMem, uint32_t addr)
```

Legacy offsets (0–15) ignore `writeMem` (they're always writes per the
kMMDV_Extn guard).  The function returns `data` for those.

### 1.4 — Add register state and dispatch for offsets 16–31

**File:** `src/core/machine.cpp`, near `extnAccess()`

Add static register file:
```cpp
static uint16_t s_regResult;
static uint32_t s_regParam[7];   // p0–p6
```

In `extnAccess()`, route word offsets 16–31 to a new handler:

```cpp
if (addr >= 16) {
    return regBlockAccess(data, writeMem, addr - 16);
}
// ... existing legacy dispatch for addr 0–15
```

`regBlockAccess(data, writeMem, regOff)`:
- `regOff 0` (byte $20, command): **write** triggers `regDispatch(data)`.
  Read returns 0.
- `regOff 1` (byte $22, result): **read** returns `s_regResult`.
  Write is ignored.
- `regOff 2..15` (byte $24..$3E, param words): read/write the
  corresponding half of `s_regParam[]`.  regOff 2 = p0 high, regOff 3 =
  p0 low, regOff 4 = p1 high, etc.

The mapping from word-offset to param index:
```
paramIdx = (regOff - 2) / 2;   // 0..6
isLow    = (regOff - 2) % 2;   // 0 = high word, 1 = low word
```

`regDispatch(cmd)` is a switch on the command code.  For Phase 1 it
can be a stub that sets `s_regResult = 0xFFFF` (unimplemented).

### 1.5 — Gate

Build.  Run `selftest.sh` and golden tests.  Legacy clipboard DAs must
still work (they only touch offsets 0–15).

---

## Phase 2 — Clipboard commands (C++ side)

Implement the clipboard command handlers called by `regDispatch()`.

### Command codes

```
Code   Name          Parameters                    Returns
----   ----          ----------                    -------
$100   ClipVersion   —                             p0 = protocol version (1)
$101   ClipExport    p0 = guest buffer addr        result = 0
                     p1 = byte count
                     (text in Mac OS Roman)
$102   ClipImport    p0 = guest buffer addr        result = 0
                     p1 = buffer capacity           p1 = actual byte count
                     (host fills buffer with
                     Mac OS Roman text)
$103   ClipHasData   —                             p0 = 1 if host has text
$104   ClipGetLen    —                             p0 = byte count of host
                                                   text (in Mac OS Roman)
$105   ClipSeqNo     —                             p0 = sequence number
```

### 2.1 — Architecture: extn_clip.cpp + clipboard.cpp wrappers

The clipboard extension handler lives in a new file
`src/core/extn_clip.cpp` (with header `src/core/extn_clip.h`).  This
establishes a pattern for future register-block extensions — each gets
its own `extn_*.cpp` file under `src/core/`.

`extn_clip.cpp` does **not** include SDL.  It delegates platform
clipboard access to thin wrappers declared in a new header
`src/platform/common/clipboard.h` and implemented in the existing
`clipboard.cpp` (which already includes SDL):

```cpp
// clipboard.h — SDL-free interface
bool     hostClipHasText();              // wraps SDL_HasClipboardText()
std::string hostClipGetTextMacRoman();   // wraps SDL_GetClipboardText() + UTF-8→MacRoman
void     hostClipSetText(const uint8_t *macRoman, uint32_t len);  // MacRoman→UTF-8 + SDL_SetClipboardText()
```

`extn_clip.cpp` exports a single dispatch entry point called from
`regDispatch()` in `machine.cpp`:

```cpp
// extn_clip.h
void extnClipDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult);
```

The extension file owns the clipboard cache (`static std::string`) and
sequence number.  It reads/writes guest RAM via `get_vm_byte()` /
`put_vm_byte()` (from `m68k.h`, no SDL dependency).

### 2.2 — ClipHasData, ClipGetLen, ClipSeqNo

**File:** `src/core/extn_clip.cpp`

- **ClipHasData ($103):** Call `hostClipHasText()`.  Set `regParam[0] = 1 or 0`, `regResult = 0`.
- **ClipGetLen ($104):** Call `hostClipGetTextMacRoman()`.  Cache the
  result in `static std::string s_clipCache`.  Set
  `regParam[0] = cached length`, `regResult = 0`.
- **ClipSeqNo ($105):** Call `hostClipGetTextMacRoman()`, compare with
  last cached text.  If different, bump `static uint32_t s_clipSeqNo`
  and update cache.  Set `regParam[0] = s_clipSeqNo`, `regResult = 0`.

### 2.3 — ClipImport ($102)

**File:** `src/core/extn_clip.cpp`

Read cached Mac Roman text (from last `ClipGetLen` or `ClipSeqNo` call).
`p0` = guest buffer address, `p1` = capacity.  Write up to
`min(cachedLen, capacity)` bytes into guest RAM via `put_vm_byte()`.
Set `regParam[1] = actualCount`, `regResult = 0`.

If no cached text, call `hostClipGetTextMacRoman()` first (so a
standalone `ClipImport` without prior `ClipGetLen` works).

### 2.4 — ClipExport ($101)

**File:** `src/core/extn_clip.cpp`

Read `p1` bytes from guest RAM at address `p0` via `get_vm_byte()`.
Call `hostClipSetText(buffer, len)` — the wrapper in `clipboard.cpp`
handles Mac Roman → UTF-8 conversion and `SDL_SetClipboardText()`.
Set `regResult = 0`.

### 2.5 — ClipVersion ($100)

Set `regParam[0] = 1`, `regResult = 0`.

### 2.6 — Clipboard.cpp wrappers

**File:** `src/platform/common/clipboard.cpp`

Add the three wrapper functions declared in `clipboard.h`.  These are
thin — each is a few lines around the existing SDL + MacRoman helpers:

```cpp
bool hostClipHasText() {
    return SDL_HasClipboardText();
}

std::string hostClipGetTextMacRoman() {
    char *utf8 = SDL_GetClipboardText();
    if (!utf8) return {};
    uint32_t len;
    if (UniCodeStrLength(utf8, &len) != tMacErr::noErr) { SDL_free(utf8); return {}; }
    std::string result(len, '\0');
    UniCodeStr2MacRoman(utf8, result.data());
    SDL_free(utf8);
    return result;
}

void hostClipSetText(const uint8_t *macRoman, uint32_t len) {
    uint32_t sz = MacRoman2UniCodeSize(macRoman, len);
    std::string utf8(sz, '\0');
    MacRoman2UniCodeData(macRoman, len, utf8.data());
    SDL_SetClipboardText(utf8.c_str());
}
```

The headless build guards these with `#ifdef HAVE_SDL` (stubs return
empty/false).

### 2.7 — Gate

Build.  Golden tests pass.  Manual test: set host clipboard text, use
a debugger or trace log to verify the register reads return correct
values.

---

## Phase 3 — THINK C console app (Mac side)

Write `macsrc/clipsync/main.c` — a one-shot ANSI C program that copies
the host clipboard into the Mac scrap.  The user creates the THINK C
project file and builds it.

### 3.1 — Discover register base address

The THINK C app finds the extension base the same way the legacy
DAs do: read `SonyVarsPtr` ($0134) to get the `MyDriverDat_R` struct,
validate `checkval`, get `pokeaddr`.  The new register block is at
`pokeaddr + $20`.

Validation: if `SonyVarsPtr` is NULL, or the checkval doesn't match
`$841339E2`, print an error and exit.

### 3.2 — Register access helpers

```c
#define REG_COMMAND  0x00
#define REG_RESULT   0x02
#define REG_P0       0x04
#define REG_P1       0x08
#define REG_P2       0x0C

static void reg_set_p0(char *base, unsigned long v)
{
    *(unsigned long *)(base + REG_P0) = v;
}

static unsigned long reg_get_p0(char *base)
{
    return *(unsigned long *)(base + REG_P0);
}

static void reg_set_p1(char *base, unsigned long v)
{
    *(unsigned long *)(base + REG_P1) = v;
}

static unsigned long reg_get_p1(char *base)
{
    return *(unsigned long *)(base + REG_P1);
}

static void reg_command(char *base, unsigned short cmd)
{
    *(unsigned short *)(base + REG_COMMAND) = cmd;
}

static unsigned short reg_result(char *base)
{
    return *(unsigned short *)(base + REG_RESULT);
}
```

No `volatile` — THINK C for 68k doesn't support it, and on the
original 68000 there's no out-of-order execution or write buffering
to worry about.  Long writes to MMIO are fine: the 68000 bus naturally
splits them into two word-sized bus cycles (high word first), and only
the command word triggers dispatch — so all params are in place before
the trigger.

### 3.3 — Main logic

```c
int main(void)
{
    char *base = find_reg_base();        /* 3.1 */
    if (!base) {
        printf("No emulator extension found\n");
        return 1;
    }

    /* ClipHasData */
    reg_command(base, 0x0103);
    if (reg_get_p0(base) == 0) {
        printf("No clipboard data on host\n");
        return 0;
    }

    /* ClipGetLen */
    reg_command(base, 0x0104);
    long len = (long)reg_get_p0(base);
    if (len == 0) {
        printf("Empty clipboard\n");
        return 0;
    }

    /* Allocate */
    Ptr buf = NewPtr(len);
    if (buf == NULL) {
        printf("Out of memory (%ld bytes)\n", len);
        return 1;
    }

    /* ClipImport */
    reg_set_p0(base, (unsigned long)buf);
    reg_set_p1(base, (unsigned long)len);
    reg_command(base, 0x0102);
    if (reg_result(base) != 0) {
        printf("Import failed (err %d)\n", reg_result(base));
        DisposPtr(buf);
        return 1;
    }
    long actual = (long)reg_get_p1(base);

    /* Put into Mac scrap */
    ZeroScrap();
    PutScrap(actual, 'TEXT', buf);
    printf("Imported %ld bytes into clipboard\n", actual);

    DisposPtr(buf);
    return 0;
}
```

### 3.4 — Includes

```c
#include <stdio.h>
#include <Memory.h>
#include <Scrap.h>
```

THINK C ANSI project: uses ANSI library for `printf`, plus Mac Toolbox
headers for `NewPtr`, `ZeroScrap`, `PutScrap`.

### 3.5 — Gate

Build in THINK C on the emulated Mac (manual).  Copy text to host
clipboard, run app inside emulated Mac, paste in TeachText — verify
text matches.  Test with: empty clipboard, short text, paragraph with
newlines, accented characters (é, ü — valid in Mac Roman).

---

## Phase 4 — Documentation and cleanup

### 4.1 — Update `docs/TODO_CLIPBOARD.md`

Replace speculative design with reference to `CLIPBOARD_PLAN.md` and
document what was actually implemented.

### 4.2 — Update `macsrc/AGENTS.md`

Add entry for `clipsync/`.

### 4.3 — Gate

Build.  Tests pass.  Commit.

---

## Files modified or created

| File | Phase | Change |
|------|-------|--------|
| `src/core/machine_config.h` | 1.1 | `extnLn2Spc` 5 → 6 |
| `src/core/machine.cpp` | 1.2–1.4 | kMMDV_Extn mask, read support, `extnAccess` signature, register state + dispatch |
| `src/core/extn_clip.h` | 2.1 | New file — declares `extnClipDispatch()` |
| `src/core/extn_clip.cpp` | 2.1–2.5 | New file — clipboard extension handler (no SDL dependency) |
| `src/platform/common/clipboard.h` | 2.6 | New file — SDL-free clipboard wrapper declarations |
| `src/platform/common/clipboard.cpp` | 2.6 | Add `hostClipHasText()`, `hostClipGetTextMacRoman()`, `hostClipSetText()` wrappers |
| `macsrc/clipsync/main.c` | 3 | New file — THINK C console app |
| `docs/TODO_CLIPBOARD.md` | 4.1 | Update to reflect implementation |
| `macsrc/AGENTS.md` | 4.2 | Add clipsync entry |

## Design decisions

- **New register I/O, no pbufs:** Host writes directly to guest RAM via
  `put_vm_byte()`.  Simpler Mac-side code (no pbuf allocation dance).
- **Extension-per-file pattern:** Each register-block extension gets
  its own `src/core/extn_*.cpp` file.  The extension handler has no
  SDL dependency — it delegates platform clipboard access to wrappers
  in `clipboard.cpp` via a clean `clipboard.h` interface.
- **Command-write trigger:** Writing the command word at offset +$00
  triggers synchronous dispatch.  All params must be set first.
- **No volatile on Mac side:** THINK C doesn't support `volatile`.
  The 68000 has no out-of-order execution or write buffering — plain
  pointer dereferences produce the correct bus cycles.  Long writes
  split naturally into two word bus cycles (high word first).
- **Clipboard caching:** `extn_clip.cpp` caches the Mac Roman conversion
  between `ClipGetLen`/`ClipSeqNo` and `ClipImport`.  Single-threaded
  emulator makes this safe.
- **One-shot console app for Phase 3:** Simplest possible Mac program.
  Bidirectional sync and INIT are future phases.
- **Both mechanisms coexist:** Legacy extension block ($00–$1F) is
  completely unchanged.  Existing DAs, Sony driver, etc. keep working.
