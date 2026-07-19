# Debugger Usability — Implementation Plan

**Status: COMPLETED** (2026-07-19)

All 11 phases implemented and committed:

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Bug fixes (info insn PRIu64, info break all kinds) | `debugger: fix info insn truncation and info break display` |
| 2 | `set`/`show` settings infrastructure | `debugger: add set/show settings infrastructure` |
| 3 | Duration parser (5s, 500ms, 1M, 100k) | `debugger: add human-readable duration parser` |
| 4 | Migrate `timeout` → `set default-timeout` | `debugger: replace timeout command with set default-timeout` |
| 5 | Inline timeout on `wait` commands | `debugger: inline timeout with human-readable units on wait` |
| 6 | Script EOF auto-exit | `debugger: scripts auto-exit at EOF` |
| 7 | `exit [code]` command | `debugger: add exit [code] command` |
| 8 | `delete` confirmation via `set confirm` | `debugger: delete-all confirmation with set confirm` |
| 9 | `help` output rewrite (auto-generated, categorized) | `debugger: auto-generated help with all commands grouped by category` |
| 10 | `info script` command | `debugger: add info script command` |
| 11 | Documentation cleanup | `docs: update debugger user guide for usability improvements` |

---

## Phase 1 — Bug Fixes

Pure correctness fixes.  No behavioral changes, no new commands.

### 1.1 — Fix `info insn` format string (U-7)

In `cmd_info.cpp`, `InfoInsn()`:
```cpp
// BEFORE:
dbg.io().write("Instructions executed: %u\n", g_instructionCount);
// AFTER:
dbg.io().write("Instructions executed: %" PRIu64 "\n", g_instructionCount);
```

### 1.2 — Fix `info break` display for all breakpoint kinds (U-6)

In `cmd_info.cpp`, `InfoBreak()`, replace the `if (bp.trapWord) / else`
block with a switch on `bp.kind`:

```cpp
switch (bp.kind) {
case Kind::Address:
    // existing address display (with SymbolsAtAddress)
case Kind::Trap:
    // existing trap display (with subtrap if applicable)
case Kind::Text:
    io.write("%-4u breakpoint  %s    -           text \"%s\"\n",
             bp.id, enb, bp.textPattern.c_str());
case Kind::Screen:
    io.write("%-4u breakpoint  %s    -           screen (threshold %.1f%%)\n",
             bp.id, enb, bp.screenMatcher.threshold);
case Kind::PowerOff:
    io.write("%-4u breakpoint  %s    -           power-off\n", bp.id, enb);
}
// After the switch, show: temporary flag, timeout deadline,
// condition, commands, ignoreCount (existing code).
```

### 1.3 — Fix `info break` for cycle-count breakpoints (BUG-A, BUG-B)

Add cycle-count BP display alongside the existing insn-count BP display.
Fix the empty check to include `cycleBreakCount() == 0`.

### 1.4 — Smoke test updates

Add smoke test cases for:
- `info insn` output contains correct large numbers (if feasible)
- `break text "pattern"` + `info break` shows `text "pattern"`
- `break off` + `info break` shows `power-off`

### Fence

- [ ] `info insn` uses `PRIu64`
- [ ] `info break` displays all 5 breakpoint kinds correctly
- [ ] `info break` shows cycle-count breakpoints
- [ ] Smoke tests pass
- [ ] Full build clean
- [ ] Commit: `debugger: fix info insn truncation and info break display`

---

## Phase 2 — `set`/`show` Settings Infrastructure (U-12)

Add a general-purpose settings mechanism.  No existing command
behavior changes yet — just the infrastructure.

### 2.1 — Create settings registry

Create `src/debugger/settings.h` and `settings.cpp`:

```cpp
// settings.h
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

enum class SettingType { Bool, UInt64, String };

struct SettingDef {
    std::string_view name;
    SettingType type;
    std::string_view help;
};

// Initialize with default values.
void SettingsInit();

// Get/set by name.  Returns false if name unknown or type mismatch.
bool SettingGetBool(std::string_view name, bool &out);
bool SettingSetBool(std::string_view name, bool value);
bool SettingGetUInt64(std::string_view name, uint64_t &out);
bool SettingSetUInt64(std::string_view name, uint64_t value);
bool SettingGetString(std::string_view name, std::string &out);

// Format a setting's current value as a human-readable string.
bool SettingFormat(std::string_view name, std::string &out);

// List all settings (for `show` with no args).
const std::vector<SettingDef> &SettingsList();
```

Initial settings (just declarations, wired in later phases):

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `default-timeout` | UInt64 | 0 | Default cycle budget for `wait` (0 = infinite) |
| `confirm` | Bool | false | Require y/n for destructive commands |

### 2.2 — Extend `set` command to handle settings

In `cmd_memory.cpp`, `CmdSet()`: before the existing register/memory
parsing, check if the first token matches a known setting name.  If
so, parse the value and call the appropriate setter.

Detection: settings have no `=` sign.  The existing code requires
`set <reg> = <val>` or `set *<addr> = <val>`.  A line like
`set confirm on` has no `=` and no `*`, so it routes to settings.

### 2.3 — Add `show` command

Add `CmdShow` to the command table.  `show` with no args lists all
settings.  `show <name>` displays one setting.

### 2.4 — Unit tests

Add test cases to `test/test_debugger.cpp`:
- Set and get a bool setting
- Set and get a uint64 setting
- `SettingFormat` produces expected strings
- Unknown setting name returns false

### Fence

- [ ] `src/debugger/settings.h` and `settings.cpp` exist
- [ ] `set confirm on` / `show confirm` works
- [ ] `show` with no args lists all settings
- [ ] Settings unit tests pass
- [ ] Smoke test: `set confirm on` + `show confirm` → "confirm = on"
- [ ] Full build clean
- [ ] Commit: `debugger: add set/show settings infrastructure`

---

## Phase 3 — Duration Parser (U-1 partial)

A reusable parser for human-readable duration values.  Used by phases
4 and 5.  No command changes yet.

### 3.1 — Create duration parser

In `src/debugger/duration.h` and `duration.cpp`:

```cpp
// Parse a human-readable duration into a cycle count.
// Accepts:
//   "5s", "500ms"    — wall-time, converted via emulated clock speed
//   "5M", "100k"     — SI multipliers on cycle counts
//   "40000000"       — raw cycle count
//   "off", "none"    — returns 0 (meaning infinite/disabled)
// Returns true on success.
bool ParseDuration(std::string_view text, uint64_t clockHz,
                   uint64_t &outCycles);

// Format a cycle count as a human-readable string.
// Uses the most natural unit (e.g., "5s" if evenly divisible).
std::string FormatDuration(uint64_t cycles, uint64_t clockHz);
```

The `clockHz` parameter comes from the emulated machine's clock speed
(e.g., 7833600 for Mac Plus, ~16 MHz for Mac II).

### 3.2 — Unit tests

Test cases in `test/test_debugger.cpp`:
- `"5s"` at 8 MHz → 40000000
- `"500ms"` at 8 MHz → 4000000
- `"5M"` → 5000000
- `"100k"` → 100000
- `"40000000"` → 40000000
- `"off"` → 0
- `"none"` → 0
- Invalid inputs → false
- `FormatDuration(40000000, 8000000)` → `"5s"`

### Fence

- [ ] `src/debugger/duration.h` and `duration.cpp` exist
- [ ] Duration parser and formatter unit tests pass
- [ ] Full build clean
- [ ] Commit: `debugger: add human-readable duration parser`

---

## Phase 4 — Migrate `timeout` → `set default-timeout` (U-1)

Wire the `default-timeout` setting and deprecate the `timeout` command.

### 4.1 — Wire `default-timeout` setting

In `settings.cpp`, register `default-timeout` with type UInt64,
default 0 (infinite — no timeout unless explicitly set).

In `cmd_script.cpp`, change `ScriptDefaultTimeout()` to read from
the settings registry instead of the static `s_defaultTimeout`.

The `set default-timeout` / `show default-timeout` commands now work
via the generic infrastructure.  The duration parser (phase 3) is
used when setting the value: `set default-timeout 10s` parses "10s"
into cycles.

### 4.2 — Delete `timeout` command

Remove `CmdTimeout` entirely from `cmd_script.cpp` and the command
table in `debugger.cpp`.  Remove the static `s_defaultTimeout`
variable.  No deprecation shim — dead code goes away.

### 4.3 — Update existing scripts

Update `test/scripts/boot_wait.dbg`, `test/scripts/must_timeout.dbg`,
`data/macs/debug.dbg`, and any other `.dbg` files that use `timeout`
to use `set default-timeout` instead.

### Fence

- [ ] `set default-timeout 10s` works
- [ ] `show default-timeout` shows value in cycles and human-readable
- [ ] `timeout` command removed from command table
- [ ] Existing scripts updated and passing
- [ ] Full build clean
- [ ] Commit: `debugger: replace timeout command with set default-timeout`

---

## Phase 5 — Inline Timeout on `wait` Commands (U-1 complete)

Allow each `wait` to specify its own timeout inline, using the
duration parser.

### 5.1 — Update `wait` timeout parsing

In `cmd_script.cpp`, `CmdWait()`: the `parseBudget` lambda currently
reads a raw number from args.  Change it to call `ParseDuration()`
on the token text, so `wait text "File" 10s` works.

Precedence: inline timeout > `default-timeout` setting > no timeout.

### 5.2 — Smoke tests

Add tests:
- `wait for 1M` (SI multiplier)
- Verify inline timeout overrides default-timeout

### Fence

- [ ] `wait text "pattern" 5s` accepted
- [ ] `wait for 1M` accepted
- [ ] Inline timeout overrides default-timeout
- [ ] Smoke tests pass
- [ ] Full build clean
- [ ] Commit: `debugger: inline timeout with human-readable units on wait`

---

## Phase 6 — Script EOF Auto-Exit (U-2, U-3)

When a script reaches EOF, exit the process with code 0 instead of
falling through to the interactive prompt.

### 6.1 — Track script execution mode

Add a flag to `Debugger::Impl`: `bool scriptMode = false;`.  Set to
true when `--dbg-script` is used without `--debugger`.

### 6.2 — Auto-exit at script EOF

In `executeCommands()`, when all lines are consumed and
`impl_->scriptMode` is true:
- If state is Stopped (no pending waits): call `std::exit(0)`.
- If a `wait` is pending (state is Running with a scriptOwned BP):
  do nothing — the wait will complete and the script will resume.
  When the resumed script is then exhausted, exit 0.

In the command loop, when `commandLoop()` is entered and scriptMode
is true and `pendingScript` is exhausted: exit 0.

### 6.3 — `debugger` command overrides auto-exit

When `CmdDebugger` fires, set `scriptMode = false` (or a separate
`interactiveOverride` flag) so the prompt stays after the script
break.

### 6.4 — Update existing scripts

Remove trailing `quit` from scripts that have it purely to avoid
the interactive fallthrough.  Review all `.dbg` files.

### 6.5 — Tests

- Script with no `quit` at the end exits cleanly (code 0)
- Script with `debugger` command stays interactive
- `must_timeout.dbg` still exits non-zero on timeout

### Fence

- [ ] Scripts auto-exit 0 at EOF
- [ ] `--debugger --dbg-script=FILE` still gets interactive prompt
- [ ] `debugger` command in a script drops to interactive
- [ ] Existing scripts work without trailing `quit`
- [ ] Full build clean
- [ ] Commit: `debugger: scripts auto-exit at EOF`

---

## Phase 7 — `exit [code]` Command (U-18)

### 7.1 — Add `CmdExit`

```cpp
void CmdExit(Debugger &dbg, const std::vector<Token> &args)
{
    int code = 0;
    if (!args.empty() && args[0].isNumber())
        code = static_cast<int>(args[0].numValue);
    dbg.io().write("Exiting with code %d\n", code);
    std::exit(code);
}
```

Add to command table with help text.

### 7.2 — Smoke test

- `exit` → process exits 0
- `exit 1` → process exits 1

### Fence

- [ ] `exit` and `exit 1` work
- [ ] Help text for `exit` present
- [ ] Smoke tests pass
- [ ] Full build clean
- [ ] Commit: `debugger: add exit [code] command`

---

## Phase 8 — `delete` Confirmation (U-4)

### 8.1 — Wire `confirm` setting

In `settings.cpp`, register `confirm` as Bool, default false (backward
compat).

### 8.2 — Prompt in `CmdDelete`

When `delete` is called with no args and `confirm` is true:
- Print `"Delete all breakpoints and watchpoints? (y/n) "`
- Read a line from `dbg.io()`
- Only proceed if the response starts with 'y' or 'Y'
- In a `commands` block or script, skip the prompt (no interactive I/O)

### Fence

- [ ] `set confirm on` + `delete` prompts for confirmation
- [ ] `set confirm off` + `delete` works silently (backward compat)
- [ ] `delete 3` never prompts (specific ID, not destructive)
- [ ] Full build clean
- [ ] Commit: `debugger: delete-all confirmation with set confirm`

---

## Phase 9 — `help` Output Rewrite (U-8)

### 9.1 — Auto-generate help summary from command table

Replace the hand-written `CmdHelp()` body with a loop over the
command table, grouped by category.  Add a `category` field to
`CmdEntry`:

```cpp
struct CmdEntry {
    std::string_view name;
    std::string_view shortcut;
    void (*handler)(...);
    std::string_view helpBrief;
    std::string_view helpFull;
    std::string_view category;   // NEW: "Execution", "Breakpoints", etc.
};
```

Categories:
- **Execution**: run, continue, step, stepi, next, finish, until
- **Breakpoints**: break, tbreak, delete, disable, enable, watch,
  rwatch, awatch, commands, ignore
- **Memory**: x, print, set, find, disas
- **Tracing**: trace, diag
- **Information**: info, backtrace, log, show
- **Scripting**: wait, exit, fail, source, debugger
- **Guest**: type, key, clearkeys, click, dialog, launch,
  exittoshell, shutdown, drive, screenshot, showtext
- **Other**: help, quit

### 9.2 — Smoke tests

- `help` output contains "Scripting:" and "Guest:" sections
- `help` mentions `wait`, `exit`, `fail`, `type`, `key`, etc.

### Fence

- [ ] `help` lists all commands grouped by category
- [ ] Adding a command to the table automatically adds it to `help`
- [ ] Smoke tests pass
- [ ] Full build clean
- [ ] Commit: `debugger: auto-generated help with all commands`

---

## Phase 10 — `info script` Command (U-13)

### 10.1 — Add `info script` subcommand

Display:
- Whether a pending script exists (and how many lines remain)
- The current `default-timeout` value
- Any active scriptOwned breakpoints (wait conditions in flight)

### Fence

- [ ] `info script` shows useful state
- [ ] Smoke test passes
- [ ] Full build clean
- [ ] Commit: `debugger: add info script command`

---

## Phase 11 — Documentation Cleanup (U-5, U-9, U-10, U-14)

### 11.1 — Rewrite DEBUGGER.md as a user guide

Replace the original spec with a practical user guide covering:
- Getting started (interactive mode, server mode)
- All commands (organized by the same categories as `help`)
- The `break` vs `wait` conceptual model
- Settings (`set`/`show`)
- Script writing guide (with examples)
- Duration syntax reference

### 11.2 — Add doc notes for aliases / equivalences

- `run` == `continue` (documented in help and user guide)
- `step` == `stepi` (documented in help and user guide)
- `showtext` note about planned rename (or rename it now)

### Fence

- [ ] DEBUGGER.md is a complete, accurate user guide
- [ ] No stale command references
- [ ] Commit: `docs: rewrite debugger user guide`
