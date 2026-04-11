# Serial Implementation Plan

## Status: Phases 1–6 complete

Companion doc: `docs/features/SERIAL.md` — architecture, rationale, backend descriptions.

## Overview

Implement the serial backend subsystem described in `docs/features/SERIAL.md`.
After this plan is complete, each SCC channel can be independently attached to
a pluggable backend at runtime via `--serial-a=MODE` / `--serial-b=MODE`.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `SerialBackend` interface + SCC wiring + `LoopbackBackend` | Done (c973ac3) |
| 2 | `[SER]` + `[SCC]` LOG infrastructure | Done (f3d21ac) |
| 3 | `FileBackend` | Done (f3d21ac) |
| 4 | `PtyBackend` | Done (f3d21ac) |
| 5 | Command-line parsing (`--serial-a=MODE`) | Done (c973ac3) |
| 6 | RR0 DCD/CTS status bits | Done (c973ac3) |
| 7 | `DeviceBackend` (stretch — real `/dev/tty` passthrough) | Not started |

Build gate: `cmake --build bld/macos && cmake --build bld/macos-headless`
Test gate:  `cd test && ./verify.sh`

---

## Phase 1 — SerialBackend Interface + SCC Wiring + LoopbackBackend

**Goal:** Define the backend interface, wire it into the SCC device, and
prove the data path with a loopback backend.

### 1.1 — SerialBackend interface

**New file:** `src/devices/serial_backend.h`

```cpp
class SerialBackend {
public:
    virtual ~SerialBackend() = default;

    // Called when the guest writes a byte to WR8 (transmit buffer).
    virtual void txByte(uint8_t byte) = 0;

    // Returns true if at least one byte is available for the guest to read.
    virtual bool rxReady() = 0;

    // Dequeue and return the next received byte.  Only call when rxReady().
    virtual uint8_t rxByte() = 0;

    // Called once per emulator tick.  The backend should do any periodic
    // housekeeping here (poll file descriptors, etc.).
    virtual void poll() = 0;

    // Human-readable name for logging.
    virtual const char* name() const = 0;
};
```

### 1.2 — SCC device: backend pointer per channel

Add `SerialBackend* backend_[2]` (one per channel) to the SCC device.
File-scope static for performance (matches existing SCC state pattern).
A public `setBackend()` method on `SCCDevice` manages ownership.

### 1.3 — SCC device: TX path (SCC_PutWR8)

When `backend_` is non-null, call `backend_->txByte(byte)` instead of
discarding the byte (or instead of the EmLocalTalk path for that
channel).

### 1.4 — SCC device: RX path (serialTick)

A new `serialTick()` method, called once per 1/60s tick for each
channel with a backend.  While `RxEnable` is set, no byte is pending
(`!RxChrAvail`), and `backend_->rxReady()`, dequeue one byte into
`RxBuff`, set `RxChrAvail`, and fire the SCC interrupt per `RxIntMode`.
Multiple bytes per tick may be needed to sustain baud rates above
~480 baud (see design consideration #3 in `SERIAL.md`).

### 1.5 — Move fields out of `#if EmLocalTalk`

`RxBuff`, `RxChrAvail`, `RxIntMode`, and `FirstChar` must exist
unconditionally in `Channel_Ty`, not only under `#if EmLocalTalk`.
Similarly, `RR0` must return `RxChrAvail` (bit 0) unconditionally.

### 1.6 — SCC device: RR8 read path

When a backend is present and `RxChrAvail` is set, `SCC_GetRR8()`
returns `RxBuff` and clears `RxChrAvail`.  The next byte is delivered
when the guest reads again and `serialTick()` refills from the backend.
Falls through to existing behaviour when no backend.

### 1.7 — LoopbackBackend

**New file:** `src/devices/serial_loopback.h`

Echoes every TX byte back as RX via a `std::queue<uint8_t>`.

**Test:** Boot System 6/7, open a terminal emulator (ZTerm, etc.) on the
modem port.  Typed characters echo back.

---

## Phase 2 — LOG Infrastructure

**Goal:** Add `[SER]` and `[SCC]` log tags for serial backend lifecycle
and low-level byte tracing.

```cpp
#define SER_trace 1

#if SER_trace
#define SER_LOG(fmt, ...) std::fprintf(stderr, "[SER] " fmt "\n", ##__VA_ARGS__)
#else
#define SER_LOG(fmt, ...) ((void)0)
#endif
```

Key log points:
- `[SER] ch0: loopback backend attached`
- `[SER] ch0: PTY backend → /dev/ttys003`
- `[SER] ch0: file backend tx=/tmp/serial.log`
- `[SER] ch0: device backend /dev/tty.usbserial-1420`

---

## Phase 3 — FileBackend

**New file:** `src/devices/serial_file.h`

TX bytes are appended to a host file.  RX bytes are read from a
(separate) host file.

When only `tx=` is given, receive is always empty.  When `rx=` is given,
bytes are read sequentially; when EOF is reached, no more RX data.

---

## Phase 4 — PtyBackend

**New file:** `src/devices/serial_pty.h`

Creates a host pseudo-terminal (PTY) pair.  The emulator holds the
master side; the slave device path is printed to stderr at startup.

**Implementation:** `posix_openpt()` / `grantpt()` / `unlockpt()` /
`ptsname()`.  The `poll()` method uses `poll(2)` with timeout 0
(non-blocking) to check for data from the host terminal.

**Platform:** POSIX only (macOS, Linux).

---

## Phase 5 — Command-Line Parsing

Add `--serial-a=MODE` and `--serial-b=MODE` to `config_loader.cpp`.

Backend objects are created and attached after Machine initialization.

---

## Phase 6 — RR0 DCD/CTS Status Bits

When a backend is attached, assert DCD (bit 3) and CTS (bit 5) in
`SCC_GetRR0()`.  This signals to the guest that a device is connected
and ready.  Some serial software (including MacTCP SLIP) checks these
before transmitting.

---

## Phase 7 — DeviceBackend (stretch)

**New file:** `src/devices/serial_device.h`

Opens a real host serial device (e.g., `/dev/tty.usbserial-1420`).
`open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)` + `tcsetattr()` for raw
mode, 8N1.  Baud rate is set from the SCC's configured rate (WR12/WR13),
or a default if the guest hasn't configured it yet.


