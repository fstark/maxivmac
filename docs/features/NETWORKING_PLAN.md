# Networking Implementation Plan

## Status: Phases 1–4 complete — networking infrastructure built

Companion docs:
- `docs/features/NETWORKING.md` — architecture, rationale, guest setup
- `docs/features/SERIAL.md` — serial port subsystem (prerequisite)
- `SERIAL_PLAN.md` — serial implementation plan (completed)

## Prerequisite

The serial backend interface, SCC device wiring, command-line parsing
(`--serial-a=MODE`), LOG infrastructure (`[SER]`, `[SCC]`), and RR0
DCD/CTS status bits are all implemented via **`SERIAL_PLAN.md`**.

This plan assumes a working `SerialBackend` interface where any backend
can be attached to an SCC channel and bytes flow correctly in both
directions with proper SCC interrupt generation.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | SLIP codec (pure library, unit-tested) | Done (08983b0) |
| 2 | CMake libslirp integration | Done (c552eb1) |
| 3 | SlirpBackend implementation | Done (d314f7c) |
| 4 | `--serial-a=slip` wiring | Done (e87cb19) |
| 5 | Integration test: MacTCP Ping | Not started (manual) |
| 6 | TCP/DNS validation + polish | Not started (manual) |

Build gate: `cmake --build bld/macos && cmake --build bld/macos-headless`
Test gate:  `cd test && ./verify.sh`

---

## Phase 1 — SLIP Codec

**Goal:** Implement RFC 1055 SLIP framing as a standalone, unit-testable
library with no emulator dependencies.

### 1.1 — Create SLIP codec

**New file:** `src/devices/slip.h`

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include <queue>

namespace slip {

constexpr uint8_t END     = 0xC0;
constexpr uint8_t ESC     = 0xDB;
constexpr uint8_t ESC_END = 0xDC;
constexpr uint8_t ESC_ESC = 0xDD;

// Encode an IP packet into SLIP-framed bytes appended to `out`.
inline void encode(const uint8_t* pkt, size_t len, std::vector<uint8_t>& out)
{
    out.push_back(END);  // flush any line noise
    for (size_t i = 0; i < len; ++i) {
        switch (pkt[i]) {
        case END: out.push_back(ESC); out.push_back(ESC_END); break;
        case ESC: out.push_back(ESC); out.push_back(ESC_ESC); break;
        default:  out.push_back(pkt[i]); break;
        }
    }
    out.push_back(END);
}

// Stateful decoder: feed bytes one at a time.  When a complete IP packet
// has been assembled, `packet()` returns it and clears the buffer.
class Decoder {
public:
    // Feed one byte.  Returns true if a complete packet is now available.
    bool feed(uint8_t byte)
    {
        if (byte == END) {
            if (!accum_.empty()) {
                // Packet complete
                return true;
            }
            // Empty frame (inter-packet END) — ignore
            return false;
        }
        if (byte == ESC) {
            escaped_ = true;
            return false;
        }
        if (escaped_) {
            escaped_ = false;
            switch (byte) {
            case ESC_END: accum_.push_back(END); break;
            case ESC_ESC: accum_.push_back(ESC); break;
            default:      accum_.push_back(byte); break; // protocol error, be lenient
            }
            return false;
        }
        accum_.push_back(byte);
        return false;
    }

    // Retrieve the completed packet and clear the buffer.
    std::vector<uint8_t> packet()
    {
        std::vector<uint8_t> pkt;
        pkt.swap(accum_);
        return pkt;
    }

    void reset()
    {
        accum_.clear();
        escaped_ = false;
    }

private:
    std::vector<uint8_t> accum_;
    bool escaped_ = false;
};

} // namespace slip
```

### 1.2 — Create SLIP unit test

**New file:** `test/test_slip.cpp`

```cpp
#include "devices/slip.h"
#include <cassert>
#include <cstdio>

static void test_roundtrip()
{
    uint8_t pkt[] = {0x45, 0x00, 0x00, 0x1C, 0xC0, 0xDB, 0x01, 0x02};
    std::vector<uint8_t> framed;
    slip::encode(pkt, sizeof(pkt), framed);

    slip::Decoder dec;
    bool got = false;
    for (uint8_t b : framed) {
        if (dec.feed(b)) got = true;
    }
    assert(got);
    auto result = dec.packet();
    assert(result.size() == sizeof(pkt));
    for (size_t i = 0; i < sizeof(pkt); ++i)
        assert(result[i] == pkt[i]);
}

static void test_empty_frames()
{
    slip::Decoder dec;
    // Multiple ENDs in a row → no packet
    assert(!dec.feed(slip::END));
    assert(!dec.feed(slip::END));
    assert(!dec.feed(slip::END));
}

static void test_all_special_bytes()
{
    // Packet entirely made of END and ESC bytes
    uint8_t pkt[] = {slip::END, slip::ESC, slip::END, slip::ESC};
    std::vector<uint8_t> framed;
    slip::encode(pkt, sizeof(pkt), framed);

    slip::Decoder dec;
    bool got = false;
    for (uint8_t b : framed)
        if (dec.feed(b)) got = true;
    assert(got);
    auto result = dec.packet();
    assert(result.size() == sizeof(pkt));
    for (size_t i = 0; i < sizeof(pkt); ++i)
        assert(result[i] == pkt[i]);
}

int main()
{
    test_roundtrip();
    test_empty_frames();
    test_all_special_bytes();
    std::printf("SLIP: all tests passed\n");
    return 0;
}
```

### 1.3 — Add test to CMake

**File:** `CMakeLists.txt` (at the end, or in a test section)

```cmake
# SLIP unit test (always built — no external dependencies)
add_executable(test_slip test/test_slip.cpp)
target_include_directories(test_slip PRIVATE "${CMAKE_SOURCE_DIR}/src")
```

### 1.4 — Add SLIP LOG

**File:** `src/devices/slip.h` (or a separate slip_log.h)

Add a `SLIP_LOG` macro for framing events:

```cpp
#define SLIP_trace 1

#if SLIP_trace
#include <cstdio>
#define SLIP_LOG(fmt, ...) std::fprintf(stderr, "[SLP] " fmt "\n", ##__VA_ARGS__)
#else
#define SLIP_LOG(fmt, ...) ((void)0)
#endif
```

Log points:
- `SLIP_LOG("encode: %zu bytes → %zu framed", len, out.size())`
- `SLIP_LOG("decode: complete packet %zu bytes", accum_.size())`

Update `docs/LOG.md`:

```markdown
| SLP | SLIP codec | `src/devices/slip.h` | `SLIP_trace` |
```

### Validation

- `cmake --build bld/macos`
- `./bld/macos/test_slip` → "SLIP: all tests passed"
- `cd test && ./verify.sh` (golden tests unaffected)

### Commit

```
git add -A && git commit -m "net: phase 1 — SLIP codec with unit tests"
```

---

## Phase 2 — CMake libslirp Integration

**Goal:** Add libslirp as an optional CMake dependency gated by a build
option.  The emulator builds with and without it.

### 2.1 — Add CMake option

**File:** `CMakeLists.txt`, options section (after line ~40)

```cmake
option(MAXIVMAC_NETWORKING "Enable SLIP networking via libslirp" OFF)
```

### 2.2 — Find and link libslirp

**File:** `CMakeLists.txt`, after the common target setup (~line 210)

```cmake
if(MAXIVMAC_NETWORKING)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SLIRP REQUIRED IMPORTED_TARGET libslirp)
    target_link_libraries(maxivmac PRIVATE PkgConfig::SLIRP)
    target_compile_definitions(maxivmac PRIVATE "HAVE_SLIRP=1")
    message(STATUS "Networking: enabled (libslirp ${SLIRP_VERSION})")
else()
    target_compile_definitions(maxivmac PRIVATE "HAVE_SLIRP=0")
    message(STATUS "Networking: disabled")
endif()
```

### 2.3 — Add a CMake preset for networking builds

**File:** `CMakePresets.json`

Add a configure preset:

```json
{
    "name": "macos-net",
    "displayName": "macOS with networking",
    "inherits": "macos",
    "cacheVariables": {
        "MAXIVMAC_NETWORKING": "ON"
    }
}
```

And a build preset:

```json
{
    "name": "macos-net",
    "configurePreset": "macos-net"
}
```

### 2.4 — Verify libslirp is findable

```bash
brew install libslirp   # if not already
cmake --preset macos-net
cmake --build bld/macos-net
```

### Validation

- `cmake --build bld/macos` (networking OFF — must still build)
- `cmake --build bld/macos-net` (networking ON — must link libslirp)
- `cd test && ./verify.sh`

### Commit

```
git add -A && git commit -m "net: phase 2 — CMake libslirp dependency (optional)"
```

---

## Phase 3 — SlirpBackend Implementation

**Goal:** Implement the libslirp wrapper as a `SerialBackend` that bridges
SLIP-framed serial bytes to/from libslirp's IP packet interface.

### 3.1 — Create SlirpBackend

**New file:** `src/platform/slirp_backend.h`

```cpp
#pragma once

#if HAVE_SLIRP

#include "devices/serial_backend.h"
#include "devices/slip.h"
#include <libslirp.h>
#include <memory>
#include <vector>
#include <queue>

class SlirpBackend : public SerialBackend {
public:
    SlirpBackend();
    ~SlirpBackend() override;

    void txByte(uint8_t byte) override;
    bool rxReady() override;
    uint8_t rxByte() override;
    void poll() override;

    // libslirp callback: a packet arrived from the network, deliver to guest
    void onSlirpOutput(const uint8_t* pkt, size_t len);

private:
    Slirp* slirp_ = nullptr;
    slip::Decoder rxDecoder_;        // TX from guest: decode SLIP → IP packet
    std::queue<uint8_t> txToGuest_;  // RX to guest: SLIP-encoded queue
};

#endif // HAVE_SLIRP
```

**New file:** `src/platform/slirp_backend.cpp`

Key implementation points:

```cpp
// libslirp callbacks (C function pointers)
static ssize_t slirp_send_packet(const void* buf, size_t len, void* opaque)
{
    auto* self = static_cast<SlirpBackend*>(opaque);
    self->onSlirpOutput(static_cast<const uint8_t*>(buf), len);
    return (ssize_t)len;
}

// ... other required callbacks (timer, clock_get_ns, etc.)

SlirpBackend::SlirpBackend()
{
    SlirpConfig cfg = {};
    cfg.version = 4;  // SlirpConfig version
    cfg.restricted = false;
    cfg.in_enabled = true;
    // Network: 10.0.2.0/24
    cfg.vnetwork.s_addr  = htonl(0x0A000200);  // 10.0.2.0
    cfg.vnetmask.s_addr  = htonl(0xFFFFFF00);  // 255.255.255.0
    cfg.vhost.s_addr     = htonl(0x0A000202);  // 10.0.2.2 (gateway)
    cfg.vdhcp_start.s_addr = htonl(0x0A00020F); // 10.0.2.15 (DHCP start)
    cfg.vnameserver.s_addr = htonl(0x0A000203); // 10.0.2.3 (DNS)

    static const SlirpCb callbacks = {
        .send_packet = slirp_send_packet,
        .guest_error = ...,
        .clock_get_ns = ...,
        .timer_new = ...,
        .timer_free = ...,
        .timer_mod = ...,
        .register_poll_fd = ...,
        .unregister_poll_fd = ...,
        .notify = ...,
    };

    slirp_ = slirp_new(&cfg, &callbacks, this);
}
```

### 3.2 — TX path: guest → network

```cpp
void SlirpBackend::txByte(uint8_t byte)
{
    SLIP_LOG("tx byte 0x%02X", byte);
    if (rxDecoder_.feed(byte)) {
        auto pkt = rxDecoder_.packet();
        SLIP_LOG("tx packet: %zu bytes → slirp_input", pkt.size());
        slirp_input(slirp_, pkt.data(), (int)pkt.size());
    }
}
```

### 3.3 — RX path: network → guest

```cpp
void SlirpBackend::onSlirpOutput(const uint8_t* pkt, size_t len)
{
    SLIP_LOG("rx packet: %zu bytes from slirp", len);
    std::vector<uint8_t> framed;
    slip::encode(pkt, len, framed);
    for (uint8_t b : framed)
        txToGuest_.push(b);
}

bool SlirpBackend::rxReady()
{
    return !txToGuest_.empty();
}

uint8_t SlirpBackend::rxByte()
{
    uint8_t b = txToGuest_.front();
    txToGuest_.pop();
    return b;
}
```

### 3.4 — Poll loop

```cpp
void SlirpBackend::poll()
{
    // Use slirp_pollfds_fill/poll for non-blocking I/O.
    // Timeout = 0 (non-blocking) since we're called every emulator tick.
    uint32_t timeout = 0;
    slirp_pollfds_fill(slirp_, &timeout, ...);
    // poll(fds, nfds, 0);
    slirp_pollfds_poll(slirp_, ...);
}
```

### 3.5 — Add NET LOG

**File:** `src/platform/slirp_backend.cpp`

```cpp
#define NET_trace 1

#if NET_trace
#define NET_LOG(fmt, ...) std::fprintf(stderr, "[NET] " fmt "\n", ##__VA_ARGS__)
#else
#define NET_LOG(fmt, ...) ((void)0)
#endif
```

Log points:
- `NET_LOG("init: network 10.0.2.0/24 gateway 10.0.2.2 dns 10.0.2.3")`
- `NET_LOG("poll: %d fds ready", n)`
- `NET_LOG("shutdown")`

Update `docs/LOG.md`:

```markdown
| NET | Networking / libslirp | `src/platform/slirp_backend.cpp` | `NET_trace` |
```

### 3.6 — Add to CMakeLists.txt

**File:** `CMakeLists.txt`

Only compile `slirp_backend.cpp` when networking is enabled:

```cmake
if(MAXIVMAC_NETWORKING)
    target_sources(maxivmac PRIVATE src/platform/slirp_backend.cpp)
endif()
```

### Validation

- `cmake --build bld/macos` (networking OFF — no slirp code compiled)
- `cmake --build bld/macos-net` (networking ON — links and compiles)
- `cd test && ./verify.sh`

### Commit

```
git add -A && git commit -m "net: phase 3 — SlirpBackend (SLIP + libslirp)"
```

---

## Phase 4 — Register `slip` Backend Mode

**Goal:** Hook the `slip` backend into the serial subsystem's
`--serial-a=MODE` mechanism (which already exists from SERIAL_PLAN.md).

### 4.1 — Add `slip` case to backend factory

**File:** The backend factory function created by SERIAL_PLAN.md
(likely in `src/platform/emulator_shell.cpp` or `src/core/main.cpp`).

Add the `slip` case alongside the existing `loopback`, `file`, `pty`,
and `device` cases:

```cpp
#if HAVE_SLIRP
    else if (spec == "slip") {
        scc->setBackend(chan, std::make_unique<SlirpBackend>());
        SER_LOG("ch%d: SLIP/libslirp backend", chan);
    }
#endif
```

### 4.2 — Add `--slip-redir=` argument (port forwarding)

**File:** `src/core/config_loader.h`

```cpp
struct LaunchConfig {
    // ... existing fields including serialA, serialB ...
    std::vector<std::string> slipRedirs;  // "tcp:hostport:guestip:guestport"
};
```

**File:** `src/core/config_loader.cpp`

```cpp
if (strncmp(arg, "--slip-redir=", 13) == 0) {
    lc.slipRedirs.push_back(arg + 13);
    continue;
}
```

### 4.3 — Usage / help text

```
  --serial-a=slip        SLIP networking via libslirp (requires networking build)
  --slip-redir=SPEC      Port forwarding: tcp:hostport:guestip:guestport
```

### Validation

- `cmake --build bld/macos-net`
- `./bld/macos-net/maxivmac --model=MacII --serial-a=slip disk.hfs`
  → boots, `[NET]` logs show slirp initialised
- `cd test && ./verify.sh`

### Commit

```
git add -A && git commit -m "net: phase 4 — register slip backend mode"
```

---

## Phase 5 — Integration Test: MacTCP Ping

**Goal:** Full end-to-end validation. Boot the emulator with MacTCP
configured for SLIP, ping the libslirp gateway, observe replies.

### 5.1 — Prepare a test disk image

Create a System 7 HFS disk image containing:
- System 7.1 (or 7.5) minimal install
- MacTCP 2.0.6 control panel configured for SLIP on modem port
  - IP: `10.0.2.15`
  - Subnet: `255.255.255.0`
  - Gateway: `10.0.2.2`
  - DNS: `10.0.2.3`
- MacTCP Ping (or another ICMP tool)

Store as `extras/disks/net-test.hfs`.

### 5.2 — Manual interactive test

```bash
./bld/macos-net/maxivmac \
    --model=MacII --rom=roms/MacII.ROM \
    --serial-a=slip \
    extras/disks/net-test.hfs \
    2>&1 | tee /tmp/net-test.log
```

Inside the guest:
1. Open MacTCP Ping.
2. Ping `10.0.2.2` (gateway).
3. Observe ICMP echo reply.

In the host terminal:
```bash
grep -E '^\[(SCC|SLP|NET)\]' /tmp/net-test.log | tail -40
```

Expected log flow:
```
[NET] init: network 10.0.2.0/24 gateway 10.0.2.2 dns 10.0.2.3
[SCC] backend ch0 set to active
[SCC] tx ch0 byte=0xC0        ← SLIP END (start of frame)
[SCC] tx ch0 byte=0x45        ← IP version+IHL
...
[SLP] tx packet: 28 bytes → slirp_input
[NET] poll: 1 fds ready
[SLP] rx packet: 28 bytes from slirp
[SCC] rx ch0 byte=0xC0        ← SLIP frame back to guest
...
```

### 5.3 — Automated headless smoke test (stretch goal)

If the headless build can script guest input (e.g., via a pre-configured
disk where MacTCP Ping auto-launches), add a test script:

```bash
#!/bin/bash
# test/test_net_ping.sh
timeout 30 ./bld/macos-net-headless/maxivmac \
    --model=MacII --rom=roms/MacII.ROM \
    --serial-a=slip --silent \
    extras/disks/net-test.hfs 2>&1 | \
    grep -q '\[SLP\] rx packet' && echo "PASS" || echo "FAIL"
```

### Validation

- Manual: ICMP echo reply received in guest
- Automated: `[SLP] rx packet` appears in log output
- `cd test && ./verify.sh` (existing golden tests still pass)

### Commit

```
git add -A && git commit -m "net: phase 5 — MacTCP ping integration test"
```

---

## Phase 6 — TCP/DNS Validation + Polish

**Goal:** Confirm full TCP and DNS functionality.  Add port forwarding.
Final documentation.

### 6.1 — DNS resolution test

Inside the guest:
1. Open MacTCP Watcher (or NCSA Telnet's DNS lookup).
2. Resolve a hostname (e.g., `example.com`).
3. Confirm the DNS query goes through `10.0.2.3` → host DNS → reply.

Expected logs:
```
[SLP] tx packet: 45 bytes → slirp_input    ← DNS query (UDP port 53)
[SLP] rx packet: 78 bytes from slirp        ← DNS response
```

### 6.2 — TCP connection test

Inside the guest:
1. Open NCSA Telnet, connect to a reachable host on port 23 (or a
   host-local service via port forwarding).
2. Observe the TCP handshake in `[SLP]` / `[NET]` logs.

### 6.3 — Port forwarding

**File:** `src/platform/slirp_backend.cpp`

Add support for the `--slip-redir=` specs parsed in Phase 4.2:

```cpp
// Parse redirect spec and call slirp_add_hostfwd()
slirp_add_hostfwd(slirp_, false /* is_udp */,
    host_addr, host_port,
    guest_addr, guest_port);
```

Example usage:
```bash
./maxivmac --serial-a=slip --slip-redir=tcp:8080:10.0.2.15:80 disk.hfs
```

This exposes TCP port 80 in the guest as port 8080 on the host.

### 6.4 — MacTCP initialization sequence handling

If during Phase 5 testing MacTCP sends AT commands or expects a login
prompt before entering SLIP mode, implement a tiny "fake modem" shim:

```cpp
class FakeModemState {
    // Accumulate characters until we see "AT\r" or "ATDT...\r"
    // Respond with "OK\r\n" and then switch to transparent SLIP mode.
};
```

This would be embedded in `SlirpBackend::txByte()` as a startup state
machine, disabled once the first SLIP END byte is seen.

**Note:** This may not be needed at all — MacTCP's "Direct Connection"
SLIP mode may skip modem commands entirely.  Determine during Phase 5.

### 6.5 — Documentation

**New file:** `docs/features/NETWORKING_GUIDE.md`

User-facing guide covering:
1. Host prerequisites (`brew install libslirp`)
2. Building with networking (`cmake --preset macos-net`)
3. Guest setup (MacTCP control panel screenshots / step-by-step)
4. Launching (`--serial-a=slip`)
5. Port forwarding
6. Troubleshooting (LOG filtering, common issues)

### 6.6 — Update docs/LOG.md — final subsystem table

```markdown
| Tag | Subsystem | File | Flag |
|:---:|---|---|---|
| VID | Video card / display | `src/devices/video.cpp` | `VID_dolog` |
| SCC | Serial Communications Controller | `src/devices/scc.cpp` | `SCC_trace` |
| SLP | SLIP codec | `src/devices/slip.h` | `SLIP_trace` |
| NET | Networking / libslirp | `src/platform/slirp_backend.cpp` | `NET_trace` |
```

### 6.7 — Update NETWORKING.md status

Mark milestones as complete in `docs/features/NETWORKING.md`.

### Validation

- Full manual test suite: ping, DNS, telnet, FTP
- `cd test && ./verify.sh`
- Build both with and without networking:
  ```
  cmake --build bld/macos        # no networking
  cmake --build bld/macos-net    # with networking
  ```

### Commit

```
git add -A && git commit -m "net: phase 6 — TCP/DNS validation, port forwarding, docs"
```

---

## Summary of New/Modified Files

| File | Phase | Change |
|------|-------|--------|
| `src/devices/slip.h` | 1 | **New** — SLIP codec |
| `test/test_slip.cpp` | 1 | **New** — SLIP unit test |
| `CMakeLists.txt` | 1,2,3 | SLIP test target, libslirp option, slirp sources |
| `CMakePresets.json` | 2 | `macos-net` preset |
| `src/platform/slirp_backend.h` | 3 | **New** — libslirp wrapper header |
| `src/platform/slirp_backend.cpp` | 3,6 | **New** — libslirp wrapper impl + port forwarding |
| `src/core/config_loader.h` | 4 | Add `slipRedirs` field |
| `src/core/config_loader.cpp` | 4,6 | Parse `--slip-redir=` |
| backend factory (see SERIAL_PLAN) | 4 | Add `slip` case |
| `docs/LOG.md` | 1,3,6 | Add SLP, NET subsystem entries |
| `docs/features/NETWORKING_GUIDE.md` | 6 | **New** — user-facing guide |
| `extras/disks/net-test.hfs` | 5 | **New** — test disk image |

> **Note:** Serial-infrastructure files (`serial_backend.h`,
> `loopback_backend.h`, `scc.h`, `scc.cpp` changes, `--serial-a/b=`
> parsing) are created by **SERIAL_PLAN.md** and are prerequisites for
> this plan.

## LOG Subsystem Summary

Two new LOG subsystems introduced by this plan:

| Tag | Scope | Key Events |
|-----|-------|------------|
| `[SLP]` | SLIP framing | `encode: N bytes → M framed`, `decode: complete packet N bytes`, `tx packet: N bytes → slirp_input`, `rx packet: N bytes from slirp` |
| `[NET]` | libslirp lifecycle | `init: network ...`, `poll: N fds ready`, `shutdown`, `hostfwd: tcp:... → ...` |

> **Note:** `[SCC]` and `[SER]` LOG tags are defined in SERIAL_PLAN.md.

Filtering:
```bash
# All networking trace:
./maxivmac --serial-a=slip disk.hfs 2>&1 | grep -E '^\[(SCC|SLP|NET)\]'

# Packets only:
./maxivmac --serial-a=slip disk.hfs 2>&1 | grep '^\[SLP\]'

# High-level events only:
./maxivmac --serial-a=slip disk.hfs 2>&1 | grep '^\[NET\]'
```
