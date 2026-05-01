# MCP Server for Vintage Mac Development

## TL;DR

Embed an MCP-compatible JSON-RPC server directly in the maxivmac headless
binary via **stdio** transport (native to VS Code Copilot).  A minimal
guest-side INIT provides high-level "Launch Application" via the existing
extension register block.  The approach reuses the existing headless backend,
extension dispatch, and parameter buffer infrastructure — no new dependencies.
A Unix domain socket transport may be added later for external scripts.

## Architecture

```
VS Code Copilot ──stdio──▶  maxivmac --mcp
                              │
                              ├─ JSON-RPC dispatcher
                              │    ├─ emulator control (pause/resume/step/reset)
                              │    ├─ screen capture (framebuffer → PNG base64)
                              │    ├─ keyboard/mouse injection
                              │    ├─ disk management (insert/eject)
                              │    ├─ memory read/write
                              │    ├─ debug log retrieval
                              │    └─ guest INIT commands (launch_app)
                              │
                              ├─ HeadlessBackend (emulation loop)
                              │
                              └─ Extension register block ──MMIO──▶ Guest INIT
                                   (existing mechanism)              (launch, status)
```

## Phases

### Phase 1 — JSON-RPC Core + stdio Transport

**Goal:** Emulator boots headless and accepts tool calls on stdin, responds
on stdout.  Minimal tool set.

1. Add `--mcp` flag to `LaunchConfig` in `src/core/config_loader.h` —
   selects MCP mode
2. Create `src/platform/mcp_server.h/.cpp` — JSON-RPC message parser/emitter
   (no external libs; JSON is simple enough for hand-rolled or a single-header
   lib like nlohmann/json or simdjson)
3. Implement MCP protocol bootstrap: `initialize` → `initialized` → tool
   listing via `tools/list`
4. Implement stdin reader in HeadlessBackend's pump loop: non-blocking read
   via `poll()`/`select()` on fd 0, parse JSON-RPC frames (Content-Length
   header framing per MCP spec)
5. Implement first 3 tools:
   - `screenshot` — reads framebuffer from `ScreenDevice`/`VideoDevice`,
     encodes as base64 PNG, returns as tool result
   - `type_text` — injects key-down/key-up events via `KeyboardDevice` for
     each character
   - `get_status` — returns model, emulated time, instruction count,
     running/paused state
6. Emulator stderr remains usable for debug logging (MCP only uses stdout)

**Key files to modify:**
- `src/core/config_loader.h` / `.cpp` — add `--mcp` flag
- `src/platform/headless_backend.h` / `.cpp` — add stdin polling to pump loop
- `src/platform/headless_main.cpp` — wire up MCP mode

**New files:**
- `src/platform/mcp_server.h` / `.cpp` — MCP protocol + JSON-RPC dispatch
- `src/platform/mcp_tools.h` / `.cpp` — tool implementations

**Verification:**
```bash
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list"}' \
  | ./maxivmac --mcp --model=MacII --rom=MacII.ROM disk.hfs
```
Returns a tool list.  Python script sends `screenshot` tool call, receives
base64 PNG.

### Phase 2 — Full Tool Set

**Goal:** Expose all useful emulator controls as MCP tools.

| Tool | Description | Maps to |
|------|-------------|---------|
| `screenshot` | Capture screen as base64 PNG | `ScreenDevice`/`VideoDevice` framebuffer |
| `type_text` | Type a string (with key-up/down timing) | `KeyboardDevice` |
| `press_key` | Press specific key (by name: cmd, option, return, etc.) | `KeyboardDevice` |
| `click` | Click at x,y coordinates | `MouseDevice` |
| `drag` | Drag from (x1,y1) to (x2,y2) | `MouseDevice` |
| `insert_disk` | Mount an HFS disk image | `SonyDevice` |
| `eject_disk` | Eject a disk | `SonyDevice` |
| `get_status` | Emulator state (model, time, instruction count) | `Rig` |
| `pause` / `resume` | Pause/resume emulation | `Rig` |
| `reset` | Restart the emulated Mac | `Machine::reset()` |
| `read_memory` | Read N bytes from emulated address space | Memory bus |
| `write_memory` | Write bytes to emulated address space | Memory bus |
| `wait_idle` | Wait until CPU is idle (polling loop settled) | Heuristic: VIA timer or instruction rate |
| `run_until` | Run until instruction count or time | `StateRecorder` infrastructure |
| `get_debug_log` | Retrieve guest-side debug log (from $108 ClipDbgLog) | `extn_clip.cpp` |

**Key files:**
- `src/platform/mcp_tools.cpp` — tool implementations calling into
  Machine/Device APIs

**Verification:**
End-to-end test: boot, wait for Finder, take screenshot, type text, take
another screenshot.  Automated via Python test script.

### Phase 3 — Guest INIT Bridge ("LaunchApp")

**Goal:** A guest-side INIT that accepts structured commands from the host
via the extension register block.

1. Add a new extension ID `kExtnAgent` with its own register-block command set
2. Host-side: `src/core/extn_agent.cpp` — manages a command queue (host writes
   command, guest reads it)
3. Command protocol:
   - Host sets command register (e.g., `$200` = LaunchApp)
   - Host writes app name to a parameter buffer
   - Guest INIT polls command register in its jGNEFilter
   - Guest reads parameter buffer, calls `_Launch` trap
   - Guest writes result back to result register
4. Guest-side: `macsrc/agent/init.c` — THINK C INIT based on
   `macsrc/clipsync/init.c` pattern:
   - Installs jGNEFilter
   - On each GNE call: checks if host has a pending command
   - If `LaunchApp`: reads app path from param buffer, calls `_Launch`
   - Writes success/failure to result register
5. MCP tool `launch_app` wraps this: writes command + params, waits for
   result register to be set

**Key files to create:**
- `src/core/extn_agent.h` / `.cpp` — host-side command queue + dispatch
- `macsrc/agent/init.c` — guest-side INIT (THINK C)
- `macsrc/agent/ExtnGlue.i` — copy from clipsync, add agent commands

**Key files to modify:**
- `src/core/machine.h` — add `kExtnAgent` to extension enum
- `src/core/machine.cpp` — register the new extension in `extnAccess()`

**Verification:**
Boot System 7 with agent INIT installed, send `launch_app("TeachText")` via
MCP, observe TeachText opens (screenshot confirms).

### Phase 4 — Unix Domain Socket Transport (optional)

**Goal:** External tools (Python, test scripts) can connect to a running
emulator over UDS, in addition to stdio.

1. Add `--mcp-socket=<path>` flag — creates a Unix domain socket listener
2. Add socket accept/read/write to the pump loop (same `poll()` call handles
   both stdin and socket fds)
3. Multiple clients can connect simultaneously (each gets its own fd in the
   poll set)
4. Same JSON-RPC dispatcher handles both transports — transport is abstracted
   behind a `Transport` interface (read/write methods)
5. When both `--mcp` and `--mcp-socket` are specified, both transports are
   active

**Key files:**
- `src/platform/mcp_server.h` / `.cpp` — add `Transport` abstraction,
  `SocketTransport` class
- `src/platform/headless_backend.cpp` — add socket fds to poll set

**Verification:**
```bash
socat - UNIX-CONNECT:/tmp/maxivmac.sock
```
Can send JSON-RPC and receive responses.  Python script using `socket` module
connects, sends `screenshot`, decodes result.

## Relevant Files

**Existing infrastructure to reuse:**
- `src/core/machine.h` / `.cpp` — extension dispatch (`extnAccess`,
  `regBlockAccess`), extension IDs enum
- `src/core/extn_clip.cpp` — reference implementation for register-block RPC
  (commands $100-$108)
- `src/platform/headless_backend.h` / `.cpp` — headless pump loop, no-GUI
  rendering
- `src/platform/headless_main.cpp` — headless entry point
- `src/core/config_loader.h` / `.cpp` — CLI argument parsing (`LaunchConfig`)
- `src/core/state_recorder.hpp` — instruction counting, max-instructions limit
- `src/platform/common/param_buffers.h` / `.cpp` — bulk data transfer
- `macsrc/clipsync/init.c` — reference INIT implementation (jGNEFilter pattern)
- `macsrc/clipin/ExtnGlue.i` — guest-side extension glue library

**Screen/input devices:**
- `src/devices/screen.h` / `.cpp` — compact Mac framebuffer
- `src/devices/video.h` / `.cpp` — Mac II video framebuffer
- `src/devices/keyboard.h` / `.cpp` — keyboard event injection
- `src/devices/mouse.h` / `.cpp` — mouse event injection

## Decisions

- **stdio as primary transport** — native to VS Code MCP, zero configuration,
  lifecycle managed by editor
- **UDS deferred** — may be added later for external scripts/test harnesses.
  HTTP rejected: unnecessary complexity for local development, adds
  dependency, port management overhead
- **C++ embedded, single binary** — no extra runtime dependency, reuses
  existing codebase, emulator IS the server
- **JSON-RPC (MCP protocol)** — standard, well-documented, maps 1:1 to tool
  calls
- **No external JSON library initially** — evaluate nlohmann/json (already
  header-only, trivial to add) vs hand-rolled for the small subset needed.
  Decision at implementation time.
- **Guest INIT deferred to Phase 4** — Phases 1-3 deliver useful automation
  without guest changes.  The INIT adds high-level semantics later.
- **Content-Length framing** — MCP spec uses HTTP-style
  `Content-Length: N\r\n\r\n{json}` framing on stdio.  This is mandatory.

## Further Considerations

1. **PNG encoding dependency**: Base64 encoding is trivial, but PNG encoding
   requires a library (`stb_image_write.h` is header-only, public domain,
   ~1 KB).  Alternatively, return raw pixel data and let the client render.
   **Recommendation:** use stb_image_write — it's tiny, no build complexity.

2. **Timing/synchronization**: After `type_text` or `launch_app`, the client
   needs to wait for the guest to process.  Options: (a) explicit `wait_idle`
   tool, (b) automatic delay after input tools, (c) event-based notification
   when guest INIT reports completion.  **Recommendation:** start with explicit
   `wait_idle` + instruction-count-based heuristic, add INIT-based completion
   later.

3. **Disk image preparation**: To build and test vintage Mac apps, the agent
   needs to get compiled binaries onto an HFS disk image.  This could be done
   via: (a) host-side `hfsutils` CLI tools (create/modify HFS images), (b) the
   existing file import extension, (c) a new MCP tool wrapping hfsutils.
   **Recommendation:** add `inject_file` MCP tool that uses `hfsutils` or the
   existing param buffer mechanism to put files on a mounted disk.
