# StateRecorder — Detailed Design

A self-contained `.hpp`/`.cpp` pair that records or verifies emulator
execution traces.  Pluggable into both the main and reference branches
with minimal integration work (two call sites: CPU loop + I/O dispatch).

---

## 1. Naming

**`StateRecorder`** — describes what it does, not the mechanism.  The file
pair is `state_recorder.hpp` / `state_recorder.cpp`.  Location: `src/core/`
(it depends on nothing platform-specific; the CPU and the I/O dispatcher
are both in `src/core/` or `src/cpu/`).

---

## 2. Public Interface (state_recorder.hpp)

```cpp
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

// ── Wire format ────────────────────────────────────────

// Byte order: always big-endian in the file (matches the 68000).

struct GoldenHeader {
    uint32_t magic;              // 0x474F4C44 ("GOLD")
    uint32_t version;            // 1
    uint32_t snapshotInterval;   // N (instructions between CPU snapshots)
    uint32_t maxInstructions;    // instruction budget
    uint32_t snapshotCount;      // number of CpuSnapshot records following
    uint32_t modelId;            // MacModel enum cast to uint32_t
    uint8_t  romHash[16];       // MD5 of the ROM file
    uint8_t  diskHash[16];      // MD5 of the first disk image (0 if none)
    uint32_t ioCrc;              // expected CRC32 of ALL IoRecord data (0 in v1)
    uint32_t reserved[3];        // pad to 80 bytes, zero-filled
};
static_assert(sizeof(GoldenHeader) == 80);

struct CpuSnapshot {             // 48 bytes
    uint32_t instructionCount;
    uint32_t pc;
    uint16_t sr;
    uint16_t pad;                // zero
    uint32_t d[8];
    uint32_t a7;                 // SP — the most diagnostic address register
};
static_assert(sizeof(CpuSnapshot) == 48);

// Not stored in the golden file.  Only used for the optional text log
// and for computing the rolling I/O CRC.
struct IoRecord {
    uint32_t instructionCount;
    uint32_t address;
    uint8_t  data;               // byte read or written
    uint8_t  direction;          // 'R' or 'W'
    char     device[6];          // e.g. "VIA1\0\0", null-padded
};
static_assert(sizeof(IoRecord) == 16);


// ── Modes ──────────────────────────────────────────────

enum class RecorderMode {
    Off,           // do nothing
    Record,        // write golden binary file
    Verify,        // read golden binary file and compare
};

enum class OnMismatch {
    ExitNonZero,   // print mismatch, exit(1)  — for CI / test.sh
    Print,         // print mismatch, keep running — for interactive debugging
};

enum class TextLog {
    None,          // no text output
    CpuOnly,       // text CPU snapshots only
    CpuAndIo,      // text CPU snapshots + every I/O access
};


// ── The class ──────────────────────────────────────────

class StateRecorder {
public:
    // ── Construction ──

    // Does nothing until init() is called.
    StateRecorder() = default;
    ~StateRecorder();

    StateRecorder(const StateRecorder&) = delete;
    StateRecorder& operator=(const StateRecorder&) = delete;

    // ── Initialization ──

    struct Config {
        RecorderMode  mode            = RecorderMode::Off;
        std::string   goldenPath;       // path for binary golden file
        std::string   textPath;         // path for text log ("" = stderr, unused if textLog == None)
        TextLog       textLog         = TextLog::None;
        OnMismatch    onMismatch      = OnMismatch::ExitNonZero;
        uint32_t      snapshotInterval = 100'000;
        uint32_t      maxInstructions  = 50'000'000;

        // Filled in by the caller (main.cpp) from the running config:
        uint32_t      modelId         = 0;
        uint8_t       romHash[16]     = {};
        uint8_t       diskHash[16]    = {};
    };

    // Opens files, writes/reads header.  Returns false on error (prints to stderr).
    bool init(const Config& cfg);

    // ── Per-instruction hook (called from the CPU loop) ──

    // Call BEFORE executing the instruction (i.e., at the point where we
    // already have the decoded PC but haven't modified any register yet).
    //
    // This must be as cheap as possible when it's a non-snapshot instruction.
    // Inlined in the header so the compiler can see the fast path.
    inline void cpu(uint32_t instructionCount,
                    uint32_t pc, uint16_t sr,
                    const uint32_t d[8], uint32_t a7);

    // ── Per-I/O-access hook (called from MMDV_Access) ──

    // Call AFTER the device access returns (Data contains the read result
    // or is the value that was written).
    inline void io(uint32_t instructionCount,
                   uint32_t address, uint8_t data,
                   bool write, const char* deviceName);

    // ── Query ──

    bool active() const { return mode_ != RecorderMode::Off; }

private:
    void cpuSlow(uint32_t instructionCount,
                 uint32_t pc, uint16_t sr,
                 const uint32_t d[8], uint32_t a7);
    void ioSlow(uint32_t instructionCount,
                uint32_t address, uint8_t data,
                bool write, const char* deviceName);
    void writeCpuSnapshot(const CpuSnapshot& snap);
    bool verifyCpuSnapshot(const CpuSnapshot& snap);
    void printMismatch(const CpuSnapshot& expected, const CpuSnapshot& actual);
    void textCpu(const CpuSnapshot& snap);
    void textIo(const IoRecord& rec);
    void finish();

    RecorderMode  mode_             = RecorderMode::Off;
    OnMismatch    onMismatch_       = OnMismatch::ExitNonZero;
    TextLog       textLog_          = TextLog::None;
    uint32_t      snapshotInterval_ = 100'000;
    uint32_t      maxInstructions_  = 50'000'000;
    uint32_t      nextSnapshot_     = 0;   // next instructionCount to snapshot
    uint32_t      snapshotIndex_    = 0;   // current index into golden records
    uint32_t      snapshotCount_    = 0;   // total records (from header or accumulated)

    uint32_t      ioCrc_            = 0;   // rolling CRC32 of IoRecord data

    FILE*         goldenFile_       = nullptr;
    FILE*         textFile_         = nullptr;   // nullptr → stderr

    // In verify mode: the golden records loaded into memory.
    CpuSnapshot*  goldenSnaps_      = nullptr;
    GoldenHeader  goldenHeader_     = {};
};


// ── Inline fast paths ──────────────────────────────────

inline void StateRecorder::cpu(uint32_t instructionCount,
                               uint32_t pc, uint16_t sr,
                               const uint32_t d[8], uint32_t a7)
{
    // Fast path: not a snapshot boundary → do nothing.
    // This compiles to a single compare + branch-not-taken.
    if (instructionCount != nextSnapshot_) [[likely]]
        return;
    cpuSlow(instructionCount, pc, sr, d, a7);
}

inline void StateRecorder::io(uint32_t instructionCount,
                              uint32_t address, uint8_t data,
                              bool write, const char* deviceName)
{
    // Fast path: no I/O logging and no I/O CRC → do nothing.
    if (textLog_ < TextLog::CpuAndIo && ioCrc_ == 0) [[likely]]
        return;
    ioSlow(instructionCount, address, data, write, deviceName);
}
```

### 2.1  Why inline the fast path?

The `cpu()` call sits inside the hottest loop in the emulator (every
instruction).  If the recorder is active, the fast path is a single
`cmp reg, [mem]; jne` — essentially free on a modern CPU (branch
predictor will learn it's almost always not-taken).  The slow path
(`cpuSlow`) lives in the `.cpp` and is never inlined.

The `io()` call is in `MMDV_Access`, which runs orders of magnitude less
often.  Still worth inlining the early exit for symmetry.

---

## 3. Implementation Sketch (state_recorder.cpp)

### 3.1 `init()`

```
Record mode:
  1. Open goldenPath for writing.
  2. Write a placeholder GoldenHeader (snapshotCount = 0).
  3. Set nextSnapshot_ = snapshotInterval_ (first snap at instruction N, not 0).
  4. If textLog != None: open textPath (or use stderr).

Verify mode:
  1. Open goldenPath for reading.
  2. Read + validate GoldenHeader:
     - magic == 0x474F4C44
     - version == 1
     - modelId matches cfg.modelId
     - romHash matches cfg.romHash
     - diskHash matches cfg.diskHash (if provided)
     On any mismatch: print what's wrong, return false.
  3. Allocate goldenSnaps_ = new CpuSnapshot[header.snapshotCount].
  4. Read all records into memory.  (At ~48 bytes × 500 = 24 KB, this is trivial.)
  5. Close the golden file (don't hold the fd open).
  6. Set nextSnapshot_ = header.snapshotInterval.
  7. If textLog != None: open textPath.
```

### 3.2 `cpuSlow()`

```
Build CpuSnapshot from the arguments.

switch (mode_):
  Record:
    writeCpuSnapshot(snap);
    if textLog >= CpuOnly: textCpu(snap);
    nextSnapshot_ += snapshotInterval_;
    snapshotCount_++;
    if (instructionCount >= maxInstructions_):
      finish();  // rewrites header with final count, closes files
      exit(0);

  Verify:
    ok = verifyCpuSnapshot(snap);
    if textLog >= CpuOnly: textCpu(snap);
    if (!ok):
      printMismatch(goldenSnaps_[snapshotIndex_], snap);
      if onMismatch == ExitNonZero: exit(1);
    snapshotIndex_++;
    nextSnapshot_ += snapshotInterval_;
    if (snapshotIndex_ >= snapshotCount_):
      fprintf(textFile_ or stderr, "PASS (%u snapshots verified)\n", snapshotCount_);
      exit(0);
```

### 3.3 `ioSlow()`

```
Build IoRecord from the arguments (strncpy deviceName, null-pad).

Update ioCrc_ with CRC32 of the 16-byte IoRecord.

if textLog >= CpuAndIo:
  textIo(rec);
```

The I/O CRC is accumulated but **not verified per-access** — it's a
rolling hash.  In v1, we don't even store it (the `ioCrc` header field
stays 0).  This is the "future hook" for catching I/O divergences that
self-correct between CPU snapshots.  When we want it:

1. Record mode: after `finish()`, seek back and write the final `ioCrc`
   into the header.
2. Verify mode: accumulate the same CRC during the run, compare it against
   `header.ioCrc` at the end (if header has nonzero `ioCrc`).  A mismatch
   means "something diverged in I/O even though the CPU snapshots matched"
   — then re-run with `TextLog::CpuAndIo` to find it.

Cost: one CRC32 update per I/O access.  On Plus boot that's ~2M accesses
at ~5 ns each = ~10 ms total.  Free.

### 3.4 `finish()`

```
Record mode:
  Seek to offset 0.
  Set goldenHeader_.snapshotCount = snapshotCount_.
  Set goldenHeader_.ioCrc = ioCrc_ (if we decide to enable it later).
  Write the header.
  Close goldenFile_.
  Close textFile_ (unless it's stderr).

Verify mode:
  Close textFile_.
  delete[] goldenSnaps_.
```

### 3.5 Text format

CPU snapshot (one line per snapshot):

```
CPU @4200000 PC=00409E12 SR=2700 D=00000000 FFFFFFFF 00000001 00000000 00000000 00000000 00000000 00000000 A7=00000FF0
```

I/O access (one line per access):

```
IO  @4200042 R VIA1 50F00000 FF
IO  @4200043 W SCC  9FFFF2   00
```

This format:
- Is dead simple to `grep`/`awk`
- Has a prefix (`CPU` or `IO`) for filtering
- Uses `@N` so instruction counts sort numerically
- Matches the spirit of the existing stderr trace

### 3.6 Destructor

Calls `finish()` if still active.  This handles the case where the
emulator exits normally (e.g., `ForceMacOff`) before reaching
`maxInstructions`.  In record mode, the golden file gets a valid header
with however many snapshots were written.  In verify mode, it means we
didn't reach the end — the destructor prints a warning ("only N of M
snapshots verified") but does NOT fail.  The explicit `exit(0)` in
`cpuSlow` is the success path.

---

## 4. Integration Points

### 4.1 CPU loop hook — `m68k.cpp`

Current code (line ~830):

```cpp
/* Log instructions in [g_LogStart, g_LogEnd) to stderr */
{
    uint32_t pc = m68k_getpc() - 2;
    if (g_LogEnd > 0) {
        ...
    }
    g_InstructionCount++;
}
```

New code:

```cpp
{
    uint32_t pc = m68k_getpc() - 2;

    // Existing text trace (unchanged)
    if (g_LogEnd > 0) {
        ...
    }

    // StateRecorder hook
    if (g_recorder.active()) {
        uint16_t sr = m68k_getSR();
        uint32_t dregs[8] = {
            m68k_dreg(0), m68k_dreg(1), m68k_dreg(2), m68k_dreg(3),
            m68k_dreg(4), m68k_dreg(5), m68k_dreg(6), m68k_dreg(7)
        };
        g_recorder.cpu(g_InstructionCount, pc, sr, dregs, m68k_areg(7));
    }

    g_InstructionCount++;
}
```

**Cost analysis of the `active()` guard:**  `active()` is `mode_ != Off`,
i.e. a load + compare.  When the recorder is off (normal interactive use),
this is a single predictable branch per instruction — negligible.

**Cost of the slow path (snapshot fire):**  `m68k_getSR()` calls
`NeedDefaultLazyAllFlags()` which forces lazy flag evaluation.  This is
cheap but not free.  It only runs once per 100K instructions, so the
amortized cost is zero.  If we later want a rolling CRC (every instruction),
we'd need to evaluate SR every time — that's the real design tension.
For now, SR only at snapshot boundaries.

**Problem:** `m68k_getSR()` is `static` in m68k.cpp.  It can't be called
from outside. Two options:

1. **Keep `g_recorder` as an `extern` and call `cpu()` directly from inside
   m68k.cpp** — this is the simplest.  `m68k.cpp` already includes global
   headers and has full access to the register file.  We just add
   `#include "core/state_recorder.hpp"` and reference an extern.

2. Make `m68k_getSR()` non-static.  But this feels wrong — it touches the
   lazy flag machinery which is deeply internal.

**Recommendation:**  Option 1.  The `g_recorder` extern lives in
`state_recorder.hpp`.  The call site is inside m68k.cpp where everything is
visible.  This is exactly how `g_LogStart`/`g_LogEnd`/`g_InstructionCount`
already work.

### 4.2 I/O hook — `machine.cpp`

Current code (line ~1586):

```cpp
if (g_LogEnd > 0 && g_InstructionCount >= g_LogStart && g_InstructionCount < g_LogEnd) {
    if (WriteMem)
        fprintf(stderr, "%u IOW %s %08X %02X\n", ...);
    else
        fprintf(stderr, "%u IOR %s %08X %02X\n", ...);
}
```

New code (added after the existing block):

```cpp
if (g_recorder.active()) {
    g_recorder.io(g_InstructionCount, addr,
                  WriteMem ? (origData & 0xFF) : (Data & 0xFF),
                  WriteMem, mmdv_name(p->MMDV));
}
```

The `io()` inline fast path exits immediately if text I/O logging is off
and no CRC is being computed.  In the common "binary verify, no text"
mode, this is a load + compare + branch-not-taken — negligible against
the cost of the device access itself.

### 4.3 CLI flags — `config_loader.cpp` + `main.cpp`

New flags:

| Flag | Effect |
|------|--------|
| `--record=<path>` | RecorderMode::Record, goldenPath = path |
| `--verify=<path>` | RecorderMode::Verify, goldenPath = path |
| `--trace=<path>` | TextLog::CpuAndIo, textPath = path (default stderr) |
| `--trace-cpu=<path>` | TextLog::CpuOnly, textPath = path |
| `--max-instructions=N` | Override default 50M instruction budget |
| `--snapshot-interval=N` | Override default 100K |

`--record` and `--verify` are mutually exclusive.  `--trace*` can combine
with either, or stand alone (for debugging without golden files).

In `main.cpp`, after `BuildMachineConfig()`:

```cpp
StateRecorder::Config rc;
rc.mode = ...;           // from CLI
rc.goldenPath = ...;
rc.textPath = ...;
rc.textLog = ...;
rc.snapshotInterval = ...;
rc.maxInstructions = ...;
rc.modelId = static_cast<uint32_t>(machineConfig.model);
md5_file(romPath, rc.romHash);
if (!diskPaths.empty())
    md5_file(diskPaths[0], rc.diskHash);

if (!g_recorder.init(rc)) {
    return 1;
}
```

### 4.4 The global instance

```cpp
// state_recorder.hpp (at the bottom, after the class)
extern StateRecorder g_recorder;

// state_recorder.cpp (at the top)
StateRecorder g_recorder;
```

Yes, it's a global.  This is pragmatic: it needs to be called from two
different compilation units (m68k.cpp and machine.cpp) in the inner loop.
Passing it through every function call in the emulator's hot path would be
invasive and pointless — there's exactly one recorder instance.  When we
later move globals into Machine (Step 3 of CLEANUP.md), this moves with
them.

---

## 5. MD5 Hashing

We need MD5 for ROM and disk hashes.  Options:

1. **Public domain single-header MD5** — e.g., the ~200-line
   implementation from RFC 1321, or a CC0 one from GitHub.  Drop it in
   `src/core/md5.h`.  No dependency, no build system changes.
2. **CommonCrypto** (macOS) / OpenSSL (Linux).  Platform-specific includes,
   adds a dependency.

**Recommendation:** Option 1.  A single `md5.h` with a `void md5_file(const char* path, uint8_t out[16])` wrapper.  It's used exactly twice (at startup), performance doesn't matter.

For CRC32, use a simple table-based implementation (~50 lines) or
`<zlib.h>` which CMake can find trivially.  CRC32 runs per I/O access
so it should be fast, but even a naive table lookup at 2M calls is
sub-millisecond.

---

## 6. Byte Order

The golden file stores everything big-endian, matching the 68000's native
byte order.  This means:

- On a big-endian host: `CpuSnapshot` and `GoldenHeader` can be
  written/read with raw `fwrite`/`fread` (with `static_assert` on sizes).
- On a little-endian host (x86, ARM64): byte-swap each field on
  write/read.

Since we control both sides and the format is tiny, this is trivial.
Use `__builtin_bswap32` / `__builtin_bswap16` or the `<bit>` header
(C++23 `std::byteswap`, or our existing `src/core/endian.h` utilities).

The text format is always human-readable (hex with `%08X`), so endianness
doesn't apply.

---

## 7. File Layout

```
src/core/
    state_recorder.hpp    // class + inline fast paths + structs
    state_recorder.cpp    // slow paths, init, finish, text formatting
    md5.h                 // public-domain MD5 (header-only)
```

Add `state_recorder.cpp` to `CMakeLists.txt` in the existing source list.
`md5.h` is header-only, no build changes needed.

---

## 8. Modes of Operation — Summary Matrix

| Mode | Binary golden | Text log | I/O CRC | Use case |
|------|:---:|:---:|:---:|---|
| `--record=f` | write | — | accumulate | Generate golden file |
| `--record=f --trace=t` | write | CPU+IO → t | accumulate | Generate + full text trace |
| `--verify=f` | read+compare | — | (v2) | CI / test.sh regression check |
| `--verify=f --trace=t` | read+compare | CPU+IO → t | (v2) | Debug a mismatch |
| `--trace=t` | — | CPU+IO → t | — | Standalone text trace (replaces `--log-start/--log-count` eventually) |
| `--trace-cpu=t` | — | CPU only → t | — | Lightweight text trace |
| (none) | — | — | — | Normal interactive use |

(v2) = I/O CRC verification deferred to format version 2.

---

## 9. Standalone Text Trace vs. Existing `--log-start`/`--log-count`

The existing trace is windowed (start + count) and writes to stderr.
The new `--trace` flag writes to a file and covers the full
`[0, maxInstructions)` range.  They serve different purposes:

- `--log-start`/`--log-count`: quick surgical trace around a known
  instruction range.  Stays as-is.
- `--trace`: full run trace, used alongside `--record`/`--verify` or
  standalone.

Both can coexist.  The existing mechanism keeps its format (it includes
the cycle counter and opcode, which the new format deliberately omits).

Long term, `--log-start`/`--log-count` becomes:
`--trace --trace-start=N --trace-count=M` — but that's a later cleanup.

---

## 10. What's NOT in v1

- **I/O CRC verification.**  The CRC is accumulated and stored in the
  header, but verify mode ignores it (`ioCrc` field = 0 means "don't
  check").  Enable it in v2 once we're confident the format is stable.
- **Rolling CPU hash between snapshots.**  Same idea — accumulate a CRC of
  (PC, SR, D0–D7) on every instruction and store it in the snapshot
  record.  This catches divergences that self-correct.  Deferred because
  it requires evaluating `m68k_getSR()` every instruction (forces lazy
  flags), which has a performance cost.  Measure first.
- **Multiple disk hashes.**  The header stores the hash of the first disk
  only.  Multi-disk golden files are a v2 feature.
- **A7 vs. all address registers.**  We only capture A7 (SP).  If needed,
  expand the snapshot to include A0–A6 (adds 24 bytes per record, ~12 KB
  per golden file — still fine).
- **Opcode / cycle count.**  Deliberately omitted from the binary format
  to keep it small and less sensitive to internal refactoring.  Available
  in the text trace if needed.

---

## 11. Implementation Order

1. **Create `state_recorder.hpp`** with the structs, enum, class
   declaration, and inline fast paths.  Compiles but does nothing.
2. **Create `state_recorder.cpp`** with `init()`, `cpuSlow()`,
   `writeCpuSnapshot()`, `verifyCpuSnapshot()`, `printMismatch()`,
   `textCpu()`, `finish()`.  Skip `ioSlow()` and `textIo()` initially.
3. **Add `md5.h`** — grab a PD implementation, test it on a known ROM.
4. **Wire CLI flags** in `config_loader.cpp` + `main.cpp`.
5. **Hook the CPU loop** in `m68k.cpp` — the one-line `g_recorder.cpu()`
   call behind the `active()` guard.
6. **Test record mode:** `--record=test.golden --model plus --rom=...
   extras/disks/608.hfs` — should produce a small binary file, then
   `exit(0)`.
7. **Test verify mode:** run the same command with `--verify=test.golden`
   — should print `PASS`.
8. **Hook the I/O dispatch** in `machine.cpp` — `g_recorder.io()`.
9. **Implement `ioSlow()` + `textIo()` + the CRC accumulation.**
10. **Test `--trace`:** verify the text output matches expectations.
11. **Generate the real golden files** for Plus+608 and MacII+608.
12. **Write `test.sh`** and verify the full workflow.

Steps 1–7 are the MVP.  Steps 8–10 add I/O awareness.  Steps 11–12
close the loop with CLEANUP.md Step 1.

---

## 12. Portability Notes

- **No platform headers.**  `state_recorder.cpp` uses only `<cstdio>`,
  `<cstdint>`, `<cstring>`, `<cstdlib>`.  It compiles on any C++17
  target.
- **No threads.**  The emulator is single-threaded.  No locking needed.
- **No exceptions.**  Error handling is return codes + `fprintf(stderr)`.
- **No allocations in the hot path.**  The only `new` is in `init()`
  (for `goldenSnaps_` in verify mode).  `cpu()` and `io()` never allocate.
- **Minimal includes in the header.**  The `.hpp` has no dependency on
  emulator internals — it takes raw `uint32_t` values.  This is what
  makes it pluggable into the reference branch.
