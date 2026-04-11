# Networking — Get a Classic Mac on the Internet

## Goal

Enable the emulated Macintosh to reach the internet (HTTP, FTP, Telnet, etc.)
using the oldest feasible system software. The primary real-world target is
software development for a **Macintosh SE/30 with an Asante Ethernet card**,
so the emulator must produce disk images whose networking stack works
identically on real hardware.

## Personas

- **Alice** wants to browse the retro web and use FTP/Telnet from her emulated Mac.
- **Beatrix** wants to cross-develop and test internet-enabled software destined
  for a real SE/30.
- **Candice** wants a one-click "networking: on" experience, no manual host
  configuration.
- **Dorothee** wants the simplest possible implementation with the fewest new
  lines of code.

---

## Part 1 — What Was Possible (Historical Context)

### The Macintosh TCP/IP Story

| Era | Stack | Minimum System | Link Layers | Notes |
|-----|-------|----------------|-------------|-------|
| 1988–1994 | **MacTCP 1.x–2.0.6** | System 6.0.4, 68000+ (1 MB RAM) | SLIP, Ethernet | Apple's first TCP/IP. SLIP over serial built-in. |
| 1991–1994 | **MacPPP 2.0.1** / **InterSLIP 1.0** | System 6.0.4+ | PPP/SLIP over serial | Third-party serial link drivers that work with MacTCP. |
| 1994 | **MacTCP 2.0.6** | System 6.0.4+ | SLIP, Ethernet | Final MacTCP. Free with System 7.5. |
| 1995+ | **Open Transport 1.0+** | System 7.5.2, 68030+ (5 MB) | Ethernet, PPP, SLIP | Replaced MacTCP. Much heavier. Needs 68030 minimum. |
| 1997+ | **FreePPP 2.6** | System 7.1+ | PPP over serial | Popular free PPP, but post–System 7. |

### The Oldest Viable Configuration

**MacTCP 2.0.6 + SLIP over the modem port, running on System 6.0.8.**

This is the lightest stack that works. It runs on a Mac Plus (68000, 1 MB RAM).
MacTCP handles IP, TCP, UDP, and DNS. SLIP is built into MacTCP's control panel
— no extra INIT or extension required. The user selects "Serial Line (SLIP)"
as the connection method and points it at the modem or printer port.

PPP is heavier (requires MacPPP INIT) and adds complexity for no benefit
inside an emulator where the serial link is lossless.

### Why Not AppleTalk / EtherTalk?

AppleTalk is a proprietary Layer 2–7 stack. It cannot route IP traffic.
EtherTalk can *coexist* with TCP/IP on the same wire but adds nothing for
internet access. The existing `EmLocalTalk` code in the SCC device is
irrelevant for TCP/IP and will not be reused.

### Why Not a Emulated NuBus Ethernet Card?

An emulated Ethernet NIC (e.g., Asante MacCon) would be the "cleanest" solution
for the guest but is by far the most complex:

- Requires emulating a NuBus slot, NuBus DMA, and an Ethernet controller
  (e.g., National Semiconductor DP8390 / AMD Am7990 LANCE).
- Requires a declaration ROM with driver code.
- Only works on Mac II-class machines with NuBus (not Plus, SE, SE/30 PDS).
- SE/30 Ethernet cards use the PDS slot, which is a different bus.

This is a possible future project but is **out of scope** for the initial
networking milestone. The SLIP approach works on *every* model the emulator
supports (Plus through IIx) and requires zero new hardware emulation.

### Why SLIP, Not PPP?

| | SLIP | PPP |
|---|------|-----|
| **Spec** | RFC 1055, 6 paragraphs | RFC 1661 + RFC 1662 + LCP + IPCP + PAP/CHAP |
| **Code** | ~30 lines (frame/deframe) | ~2,000+ lines (state machine, negotiation) |
| **Guest software** | Built into MacTCP control panel | Requires separate MacPPP/FreePPP INIT |
| **Error detection** | None (not needed on a virtual link) | CRC-16 (unnecessary overhead here) |
| **IP config** | Static (fine — we control both ends) | IPCP negotiation |

SLIP is comically simple. PPP is a real protocol suite. For a virtual serial
link inside an emulator, SLIP is the correct choice.

---

## Part 2 — Guest-Side Setup

### Software to Install in the Emulated Mac

1. **System 6.0.8** (or System 7.x — any version works)
2. **MacTCP 2.0.6** control panel
   - Freely distributable with System 7.5; also available standalone.
   - Place in the `System Folder` (System 6) or `Control Panels` (System 7).
3. No other networking software required.

### MacTCP Configuration

1. Open the **MacTCP** control panel.
2. Click the **serial port icon** (modem port or printer port).
3. Click **More...** to open the detailed configuration dialog:
   - **Obtain Address:** Manually
   - **IP Address:** `10.0.2.15` (matches libslirp default)
   - **Subnet Mask:** `255.255.255.0` (Class C)
   - **Gateway Address:** `10.0.2.2` (libslirp gateway)
   - **Domain Name Server:** `10.0.2.3` (libslirp DNS forwarder)
4. Select **SLIP** as the connection method (not "EtherTalk" or "LocalTalk").
5. Close and restart.

### Guest Applications for Testing

| Application | Purpose | Min System | Source |
|-------------|---------|------------|--------|
| **MacTCP Ping** | ICMP echo test | System 6 | Macintosh Garden |
| **NCSA Telnet 2.7** | Telnet client & VT102 | System 6 | NCSA archive |
| **Fetch 2.1.2** | FTP client | System 6 | Dartmouth |
| **MacWeb 2.0** | HTTP/1.0 browser | System 7 | EI@UMN |
| **MacLynx** | Text-mode HTTP browser | System 6 | Macintosh Garden |
| **Eudora 1.5.4** | POP3 email | System 6 | UIUC archive |
| **MacTCP Watcher** | DNS/TCP/UDP diagnostic | System 7 | Peter Lewis |

---

## Part 3 — Host-Side Architecture

### Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│  Guest Mac                                                      │
│                                                                 │
│  MacTCP ──► Serial Driver ──► SCC Channel A (Modem Port)        │
│                                WR8 (transmit byte)              │
└───────────────────────┬─────────────────────────────────────────┘
                        │  raw bytes
                        ▼
┌───────────────────────────────────────────────────────────────┐
│  Emulator (host)                                              │
│                                                               │
│  SCC Device ──► SerialBackend interface                       │
│                    │                                          │
│                    ▼                                          │
│             SlipBackend                                       │
│               │ SLIP deframe (END/ESC decode)                 │
│               ▼                                               │
│          Raw IP packet                                        │
│               │                                               │
│               ▼                                               │
│          libslirp  ──────►  Host network / Internet           │
│               │                                               │
│               │  IP packet from network                       │
│               ▼                                               │
│          SLIP frame (END/ESC encode)                          │
│               │                                               │
│               ▼                                               │
│          SerialBackend ──► SCC RR8 (receive byte) + interrupt │
└───────────────────────────────────────────────────────────────┘
```

### Component Breakdown

#### 1. Serial Port Subsystem (see `SERIAL.md`)

The `SerialBackend` interface, SCC device wiring, and general-purpose
backends (loopback, file, PTY, device passthrough) are defined in
`SERIAL.md` and implemented via `SERIAL_PLAN.md`.  The networking
feature builds on top of that infrastructure — the `SlipBackend` is
just another serial backend.

#### 2. SLIP Codec (~50 lines, new file)

```
src/devices/slip.h / slip.cpp

RFC 1055 framing:
  END  = 0xC0
  ESC  = 0xDB
  ESC_END = 0xDC
  ESC_ESC = 0xDD

SlipEncoder: given an IP packet, emit END + escaped bytes + END.
SlipDecoder: accumulate bytes; on END, deliver complete IP packet.
```

This is a stateless byte-in/byte-out codec. ~50 lines total.

#### 3. libslirp Integration (~200 lines, new file)

```
src/platform/slirp_backend.h / slirp_backend.cpp
```

- Wraps `libslirp` (the same user-mode NAT library used by QEMU, Basilisk II,
  and virtually every other emulator with networking).
- Implements `SerialBackend`: TX bytes → SLIP decode → `slirp_input()`;
  `slirp_output()` callback → SLIP encode → RX buffer.
- Drives the libslirp event loop from the emulator's main tick
  (`slirp_pollfds_fill()` / `slirp_pollfds_poll()`).
- Default network: `10.0.2.0/24`, gateway `10.0.2.2`, DNS `10.0.2.3`.
- No root/admin privileges required. No TUN device. No kernel modules.
- CMake: `find_package(PkgConfig)` + `pkg_check_modules(SLIRP libslirp)`,
  or `FetchContent` from the GitLab repo.

**Why libslirp?**
- Battle-tested in QEMU since 2001, standalone library since 2019.
- MIT-licensed, C API, ~15 function calls.
- Provides NAT, DNS forwarding, DHCP, ICMP — everything needed.
- No configuration required on the host. No network interfaces created.
- Basilisk II uses the exact same library for its "slirp" Ethernet mode.

#### 4. SCC Modifications (see `SERIAL.md` Part 3)

All SCC device changes (backend pointers, TX/RX routing, `Channel_Ty`
field promotion, RR0 DCD/CTS bits) are part of the serial subsystem.
See `SERIAL.md` for details.

#### 5. Command-Line Interface (see `SERIAL.md` Part 4)

```
--serial-a=slip    # Modem port → SLIP/libslirp networking
```

The `--serial-a=MODE` / `--serial-b=MODE` mechanism is defined in
`SERIAL.md`.  The `slip` mode is the networking-specific backend.

---

## Part 4 — Implementation Plan

### Prerequisite — Serial Port Subsystem

The serial backend interface, SCC wiring, and base backends (loopback,
file, PTY) are implemented first via **`SERIAL_PLAN.md`**.  The
networking plan resumes from Milestone 1 below, which assumes a working
`SerialBackend` infrastructure and `--serial-a=MODE` CLI.

### Milestone 1 — SLIP Framing

**Goal:** SLIP encode/decode works correctly as a standalone library.

1. Implement `SlipEncoder` and `SlipDecoder` (RFC 1055).
2. Unit tests: round-trip arbitrary byte sequences including 0xC0 and 0xDB.
3. No emulator integration yet — pure library code.

### Milestone 2 — libslirp Integration

**Goal:** The emulated Mac can resolve DNS and ping the gateway.

1. Add libslirp as a CMake dependency (FetchContent or system package).
2. Implement `SlirpBackend : SerialBackend`.
3. Wire SLIP codec between the byte-level serial interface and libslirp's
   packet interface.
4. Drive libslirp's poll loop from the emulator tick.
5. Add `--serial-a=slip` command-line option.
6. **Test:** Boot System 6 + MacTCP + MacTCP Ping. Ping `10.0.2.2` (gateway).
   This proves: Mac → SCC → SLIP → libslirp → reply → SLIP → SCC → Mac.

### Milestone 3 — DNS and TCP

**Goal:** Full internet access from the guest.

1. MacTCP Watcher: verify DNS resolution (`slirp` forwards queries to host
   DNS via `10.0.2.3`).
2. NCSA Telnet: connect to a telnet server.
3. Fetch: download a file via FTP.
4. MacWeb or MacLynx: load an HTTP page.

### Milestone 4 — Polish

1. Auto-detect MacTCP SLIP baud rate negotiation (if any) and handle
   gracefully.
2. Port forwarding (`--slip-redir=tcp:8080:10.0.2.15:80`) to expose guest
   services to the host.
3. Document guest setup in a user-facing README.
4. CI test: headless boot with MacTCP configured, ping gateway, check exit.

---

## Part 5 — Why This Approach

### Comparison of All Options

| Approach | Guest SW | Host Complexity | Models | Privileges | Lines of Code |
|----------|----------|-----------------|--------|------------|---------------|
| **SLIP over SCC + libslirp** | MacTCP only | Low | All (Plus→IIx) | None | ~300 |
| PPP over SCC + libslirp | MacTCP + MacPPP | Medium | All | None | ~2,500 |
| Emulated NuBus Ethernet | MacTCP or OT | Very High | Mac II only | None | ~5,000+ |
| Emulated PDS Ethernet (SE/30) | MacTCP or OT | Very High | SE/30 only | None | ~5,000+ |
| TUN device + SLIP | MacTCP only | Low | All | **Root** | ~200 |
| Host serial port passthrough | MacTCP + modem | Low | All | Device access | ~100 |

**SLIP + libslirp wins** on every dimension except "identical to a real
Ethernet card." For Beatrix's SE/30 use case, the guest networking stack
(MacTCP + TCP/IP applications) is identical whether the link layer is SLIP or
Ethernet — applications use the MacTCP API and never see the link layer.

### What Beatrix Gets

Software developed and tested under MacTCP/SLIP in the emulator will work
identically on a real SE/30 with an Asante card and MacTCP/Ethernet, because:

1. MacTCP presents the same API regardless of link layer.
2. TCP/IP behaviour is identical — same MTU considerations, same DNS, same
   sockets-like interface.
3. The only difference is throughput (SLIP is slower), which doesn't matter
   for development & testing.

---

## Part 6 — Dependencies

| Dependency | Version | License | Size | Install |
|------------|---------|---------|------|---------|
| **libslirp** | ≥ 4.7 | MIT | ~100 KB | `brew install libslirp` / `apt install libslirp-dev` |
| **glib-2.0** | ≥ 2.50 | LGPL-2.1 | (system) | Required by libslirp |

libslirp's glib dependency is its one downside. If that's unacceptable, a
lightweight alternative is to vendor the older QEMU `slirp/` directory
(~4,000 lines, BSD-licensed, no glib dependency) as Basilisk II does. This
trades "clean external dependency" for "vendored code."

---

## Part 7 — Risks and Open Questions

1. **SCC baud rate emulation.** MacTCP may configure the SCC for a specific
   baud rate (e.g., 19200, 57600). The emulator currently ignores baud rate
   registers. For SLIP this is fine — our virtual serial link is infinitely
   fast — but we must ensure MacTCP doesn't stall waiting for CTS/RTS
   handshake signals. May need to fake DCD/CTS/DSR status bits in RR0.

2. **SCC interrupt timing.** MacTCP's SLIP driver expects interrupts when
   bytes arrive. The receive path must correctly trigger SCC interrupts per
   the configured `RxIntMode` (first-char, all-chars, or special-condition).
   The existing `EmLocalTalk` code already does this, so the pattern exists.

3. **MTU.** SLIP has no built-in MTU negotiation. MacTCP defaults to 1006
   bytes for SLIP. libslirp's default MTU is 1500. This is fine — libslirp
   handles fragmentation if needed.

4. **glib dependency.** libslirp requires glib. Options: accept it, vendor
   old QEMU slirp code, or write a minimal slirp from scratch (not
   recommended — TCP is hard).

5. **Concurrency.** libslirp is not thread-safe. The poll loop must run on
   the emulator's main thread, which is where the SCC tick already runs.
   No threading issues.

6. **MacTCP SLIP initialization sequence.** MacTCP may send AT commands or
   expect a login prompt before entering SLIP mode. Need to test whether
   MacTCP's "Server" SLIP mode (no dialing) bypasses this. If not, a small
   "fake modem" shim (respond OK to AT, go transparent) may be needed.

---

## References

- RFC 1055 — SLIP specification (2 pages)
- libslirp — https://gitlab.freedesktop.org/slirp/libslirp
- MacTCP Administrator's Guide — https://archive.org/details/apple-mactcp-admin-guide/
- Basilisk II slirp integration — https://github.com/cebix/macemu (ether_slirp.cpp)
- Basilisk II setup guide — "Set Ethernet Interface to slirp, TCP/IP control panel to Ethernet and DHCP"
- Existing maxivmac SCC code — src/devices/scc.cpp
- Existing maxivmac networking notes — docs/TODO_NETWORKING.md
