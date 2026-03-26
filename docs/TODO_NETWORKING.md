# TCP/IP via SLIP (MacTCP on System 6)

Get MacTCP working so the emulated Mac can talk to TCP/UDP servers on
the host network or the internet.

## Architecture

MacTCP on System 6 supports SLIP over serial.  The Mac side is
standard, well-documented, and requires no custom driver — just
MacTCP + a SLIP configuration pointing at the modem/printer port.

The emulator side needs:

1. **SCC serial data path** — currently, outside of `#if EmLocalTalk`,
   bytes written to the SCC transmit buffer (`SCC_TxBuffPut`) are
   discarded.  A serial-port backend is needed: when the Mac writes
   bytes to SCC channel A or B, they flow to a host-side handler.

2. **SLIP deframing** — trivial protocol (RFC 1055, 4 special bytes).
   Extract IP datagrams from the SLIP-framed serial byte stream.

3. **Host-side IP routing** — two options:

   - **libslirp (user-mode NAT):** the library QEMU, libvirt, and
     others use.  Provides a user-space TCP/IP stack with NAT, DHCP,
     DNS forwarding, and ICMP.  No root/admin privileges needed.  The
     emulator feeds it raw IP packets; it handles routing to the host
     network.  This is the recommended approach.

   - **TUN device:** create a kernel TUN interface, inject/extract IP
     packets.  The host routes them.  Simpler conceptually but requires
     elevated privileges on most platforms.

4. **Reverse path:** IP packets from the network arrive via libslirp
   (or TUN), get SLIP-framed, and fed into the SCC receive path.  The
   SCC raises an interrupt; MacTCP reads the data.

## SLIP framing (RFC 1055)

```
END  = 0xC0    Frame delimiter
ESC  = 0xDB    Escape byte
ESC_END = 0xDC ESC + ESC_END  = literal 0xC0 in data
ESC_ESC = 0xDD ESC + ESC_ESC  = literal 0xDB in data
```

A packet is: `[END] <escaped payload> END`.  That's the entire
protocol.

## Mac-side setup

- Install **MacTCP 2.0.x** (runs on System 6.0.4+).
- In the MacTCP control panel, select the serial port and configure
  SLIP.
- Set a static IP address (libslirp can provide DHCP, but MacTCP's
  SLIP mode typically uses static config).
- MacTCP handles everything above the serial port: IP, TCP, UDP, DNS.

## SCC backend design

The SCC device (`src/devices/scc.cpp`) manages two channels (A and B).
The new serial backend should:

- Be selectable per-channel at runtime (e.g., `--serial-a=slip`).
- Abstract the byte-level I/O: `void putByte(uint8_t)` for transmit,
  `bool getByte(uint8_t&)` for receive, polled from the SCC interrupt
  logic.
- The SLIP handler sits between the byte-level serial interface and
  the IP packet interface (libslirp or TUN).

## Incremental plan

1. **Milestone 1 — SCC serial byte loopback.**
   Add a serial backend interface to SCC.  Implement a "loopback"
   backend that echoes transmitted bytes back to the receive side.
   Verify with a Mac terminal emulator (ZTerm, etc.) — typed
   characters should echo.

2. **Milestone 2 — SLIP framing + libslirp integration.**
   Implement the SLIP encoder/decoder (~100 lines).  Link libslirp.
   Configure libslirp with a 10.0.2.0/24 network (QEMU's default).
   The emulated Mac gets 10.0.2.15; the gateway is 10.0.2.2.

3. **Milestone 3 — MacTCP ping.**
   With MacTCP configured for SLIP, verify ICMP echo (MacTCP Ping or
   similar tool).  This proves the full path: Mac → SCC → SLIP →
   libslirp → host network → reply → libslirp → SLIP → SCC → Mac.

4. **Milestone 4 — TCP connections.**
   Test with NCSA Telnet, Fetch (FTP client), or a web browser
   (MacWeb, MacLynx).  At this point MacTCP is fully functional.

## Reference material

- **RFC 1055** — SLIP specification (2 pages).
- **libslirp** — `https://gitlab.freedesktop.org/slirp/libslirp`.
  C library, MIT-licensed.  Well-documented API: `slirp_new()`,
  `slirp_input()` (feed IP packet in), `slirp_pollfds_fill/poll()`
  (drive the event loop), callback `slirp_output()` (receive IP
  packet out).
- **QEMU's SLIP/SLIRP integration** — `qemu/net/slirp.c` for how
  QEMU wires libslirp to a virtual NIC.
- **MacTCP Programmer's Guide (Apple, 1993)** — documents the API
  that applications use; useful for testing.
- **MacTCP Admin Guide 2.0** — SLIP configuration details.
- **Zilog SCC (Z8530) datasheet** — the hardware the emulator
  models; relevant for understanding interrupt and DMA behavior that
  MacTCP's SLIP driver expects.

---

## What was removed

The `src/unused/` directory contained four orphaned files from the
original minivmac, all deleted:

| File | Lines | Purpose | Why removed |
|------|-------|---------|-------------|
| `SGLUALSA.h` | ~300 | ALSA audio backend (Linux) | SDL handles audio |
| `SGLUDDSP.h` | ~100 | OSS `/dev/dsp` audio backend | SDL handles audio |
| `LTOVRUDP.h` | 457 | LocalTalk over UDP multicast | Emulator-to-emulator AppleTalk only; doesn't help with file sharing or TCP/IP |
| `LTOVRBPF.h` | 383 | LocalTalk over BSD Packet Filter | macOS-only, requires root, raw Ethernet AppleTalk frames; niche and fragile |

The LocalTalk transport code (`LT_TransmitPacket`, `LT_ReceivePacket`,
`InitLocalTalk`, etc.) was called from `scc.cpp` but only inside
`#if EmLocalTalk` blocks, which is `#define 0`.  The calling code in
`scc.cpp` remains (guarded by `EmLocalTalk`) and could be re-enabled
in the future if LocalTalk emulation is desired, but it would need a
new transport backend.
