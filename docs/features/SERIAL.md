# Serial Port Subsystem

## Goal

Give the emulated Macintosh a working, modular serial port implementation.
Each SCC channel (A = modem port, B = printer port) can be independently
attached to a pluggable backend at runtime via `--serial-a=MODE` /
`--serial-b=MODE`.

This is a prerequisite for networking (see `NETWORKING.md`) but is
independently valuable for vintage Mac development: serial debugging,
terminal access, file transfer, and direct hardware communication.

Implementation plan: `SERIAL_PLAN.md` (project root).

## Personas

- **Alice** doesn't care about serial ports — everything should work by
  default with no backend attached (current behaviour).
- **Beatrix** wants to use serial I/O for debugging (MacsBug over serial,
  `printf` to the modem port), talking to real hardware via `/dev/tty*`,
  and eventually SLIP networking.
- **Candice** wants a one-flag experience: `--serial-a=pty` gives her a
  host terminal connected to the Mac's modem port.
- **Dorothee** wants a clean abstraction that keeps backend code out of the
  SCC device and allows new backends without touching `scc.cpp`.

---

## Architecture

### Data Flow

```
┌──────────────────────────────────────────────────┐
│  Guest Mac                                       │
│                                                  │
│  Application / Driver ──► Serial Driver          │
│                             ▼                    │
│                    SCC Channel A or B             │
│                    WR8 (TX byte) / RR8 (RX byte) │
└─────────────┬───────────────────────┬────────────┘
              │ TX byte               │ RX byte
              ▼                       ▲
┌─────────────────────────────────────────────────┐
│  Emulator                                       │
│                                                 │
│  SCCDevice ──► SerialBackend interface           │
│                   │                             │
│        ┌──────────┼──────────┬─────────┐        │
│        ▼          ▼          ▼         ▼        │
│     Loopback    File       PTY      Device      │
│     Backend    Backend   Backend    Backend      │
│     (echo)   (log/replay) (host   (/dev/tty)    │
│                           pseudo-              │
│                           terminal)             │
│                                                 │
│  Future:   SlipBackend (NETWORKING.md)           │
│            ImGui terminal window                 │
└─────────────────────────────────────────────────┘
```

### SerialBackend Interface

Each backend implements a small interface:

- **txByte(byte)** — called when the guest writes to the SCC transmit
  buffer (WR8).
- **rxReady()** — returns true if at least one byte is available for the
  guest to read.
- **rxByte()** — dequeues and returns the next received byte.
- **poll()** — called once per 1/60s tick for periodic housekeeping
  (polling file descriptors, draining host buffers into the backend's
  internal receive queue).
- **name()** — human-readable identifier for logging.

Each SCC channel holds an optional backend pointer.  When null (the
default), behaviour is unchanged: TX bytes are discarded, RX returns
idle.  When set, the SCC routes bytes through the backend.

### SCC Integration

The SCC device routes data through the backend at three points:

- **TX path:** When the guest writes a byte to WR8 and a backend is
  attached, the byte is forwarded to the backend instead of being
  discarded.
- **RX path:** A per-tick poll checks each attached backend for
  available data and delivers bytes into the SCC's receive buffer,
  triggering interrupts according to the channel's configured mode.
  Multiple bytes per tick may be delivered to sustain realistic baud
  rates (see Design Considerations).
- **Status bits:** When a backend is attached, DCD (bit 3) and CTS
  (bit 5) are asserted in RR0, signalling to the guest that a device
  is connected.  This is required by serial software such as MacTCP
  SLIP.

Fields currently gated behind `#if EmLocalTalk` (receive buffer, receive
interrupt mode) must be available unconditionally for the serial backend
to function.

---

## Backends

### NullBackend (default)

Discards TX.  Never has RX data.  This is the implicit default when no
`--serial-X=` flag is given — the SCC behaves exactly as it does today.
No actual backend object is created; a null pointer means "null backend".

### LoopbackBackend

Echoes every TX byte back as RX.  Useful for verifying that the
SCC-to-backend data path works end to end.

```
--serial-a=loopback
```

### FileBackend

TX bytes are appended to a host file.  RX bytes are read from a
(separate) host file.  Useful for:

- Capturing all serial output to a log file (e.g., MacsBug serial
  output, debug `printf` from guest code).
- Replaying a byte sequence into the guest (testing serial protocols).

```
--serial-a=file:tx=/tmp/mac-serial.log
--serial-a=file:tx=/tmp/out.bin,rx=/tmp/in.bin
```

When only `tx=` is given, there is no RX source (receive is always
empty).  When `rx=` is given, bytes are read sequentially; at EOF, no
more RX data is produced.

### PtyBackend

Creates a host pseudo-terminal (PTY) pair.  The emulator holds the
master side; the slave device path (e.g., `/dev/ttys003`) is printed to
stderr at startup.  The user can then `screen /dev/ttys003 9600` or
`minicom -D /dev/ttys003` to get a live terminal session with the
guest Mac's serial port.

```
--serial-a=pty
```

POSIX only (macOS, Linux).  A Windows equivalent (named pipes, ConPTY)
is out of scope for now.

### DeviceBackend

Opens a real host serial device (e.g., `/dev/tty.usbserial-1420`) and
passes bytes bidirectionally between the Mac serial port and physical
hardware.

```
--serial-a=device:/dev/tty.usbserial-1420
```

The host serial port is opened in raw mode (8N1).  Baud rate is derived
from the SCC's WR12/WR13 registers, or a sensible default if the guest
hasn't configured them yet.

**Use case:** Connecting the emulated Mac to real vintage hardware
(serial printers, modems, other Macs via null-modem cable).

### SlipBackend (see NETWORKING.md)

A specialized backend that wraps SLIP framing + libslirp to provide
TCP/IP networking.  Defined in `NETWORKING.md`; depends on the serial
infrastructure defined here.

```
--serial-a=slip
```

### ImGui Terminal (future)

A serial terminal window inside the ImGui developer UI, acting as a
built-in VT100 wired to a SCC channel.  Out of scope for the initial
serial implementation but the architecture supports it naturally.

---

## Command-Line Interface

```
--serial-a=MODE    # Modem port (SCC channel A)
--serial-b=MODE    # Printer port (SCC channel B)
```

| Mode | Description |
|------|-------------|
| *(empty / not specified)* | No backend (default, current behaviour) |
| `loopback` | Echo TX → RX |
| `file:tx=PATH[,rx=PATH]` | Log TX to file, optionally read RX from file |
| `pty` | Host pseudo-terminal |
| `device:PATH` | Real serial device passthrough |
| `slip` | SLIP + libslirp networking (requires `HAVE_SLIRP` build) |

---

## Design Considerations

1. **Baud rate.**  The SCC's WR12/WR13 configure the baud rate divisor.
   For virtual backends (loopback, PTY, SLIP) baud rate is irrelevant —
   bytes flow instantly.  For `DeviceBackend`, the host serial port must
   match the SCC's configured rate, which means watching WR12/WR13
   writes and calling `cfsetspeed()` accordingly.

2. **Flow control.**  RTS/CTS hardware flow control is common on real
   serial links.  The SCC can be programmed for auto-enable RTS (WR3
   bit 5) and auto-CTS (WR5 bit 5).  For virtual backends, CTS is
   always asserted (infinite-speed link).  For `DeviceBackend`,
   RTS/CTS should be wired through to `tcsetattr()` flags.

3. **Byte rate limiting.**  On a real Mac, the serial port runs at a
   fixed baud rate.  If we deliver RX bytes at one-per-tick (60 Hz),
   that's only 60 bytes/sec — far below even 1200 baud.  The receive
   path may need to deliver *multiple* bytes per tick based on the
   configured baud rate and the tick interval.

4. **EmLocalTalk coexistence.**  `EmLocalTalk` only uses channel B
   (printer port).  The serial backend can coexist: if a backend is
   attached, it takes priority; otherwise the EmLocalTalk path runs.
   Attaching a backend to channel B while EmLocalTalk is enabled should
   produce a warning.

5. **Windows PTY.**  `posix_openpt()` doesn't exist on Windows.  The
   `PtyBackend` is POSIX-only.  A Windows equivalent could use named
   pipes or `ConPTY`, but that's deferred.

---

## References

- Zilog Z8530 SCC Technical Manual
- maxivmac SCC implementation — `src/devices/scc.cpp`
- POSIX pseudo-terminal API — `posix_openpt(3)`, `grantpt(3)`, `ptsname(3)`
- `NETWORKING.md` — builds on top of this serial infrastructure
- `SERIAL_PLAN.md` — implementation plan
