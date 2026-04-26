# SharedDrive — Phase 3 Implementation Plan

Design: [SHAREDRIVE_DESIGN.md](SHAREDRIVE_DESIGN.md)
Spec: [SHAREDRIVE.md](SHAREDRIVE.md)
Prerequisite: [SHAREDRIVE_PLAN2.md](SHAREDRIVE_PLAN2.md) (all phases complete)

Phase 3 is optional cleanup.  The new INIT (Phase 2) uses only the
coarse commands ($0220–$0225) plus a few direct commands that remain
useful (Close, Read, Write, SetEOF, GetVol, OpenWD, CloseWD,
GetWDInfo, CreateDir, CatMove, GetDirInfo, SetDirInfo).

This phase deprecates and eventually removes the fine-grained
commands that are no longer called by the INIT.  It also adds
documentation and host-side cleanup.

| Phase | Description                                    | Status |
|-------|------------------------------------------------|--------|
| 3.1   | Mark deprecated commands in extn_extfs.cpp     |        |
| 3.2   | Add deprecation logging                        |        |
| 3.3   | Remove deprecated command handlers             |        |
| 3.4   | Clean up command constant numbering            |        |
| 3.5   | Update documentation                           |        |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `ctest --preset macos` + manual boot test

---

## Phase 3.1 — Mark Deprecated Commands

Identify which old commands are no longer called by the Phase 2
INIT and mark them with comments.

### 3.1.1 — Commands still in use

These commands are still called directly by the Phase 2 INIT (not
replaced by coarse equivalents):

| Cmd    | Name          | Used by            |
|--------|---------------|--------------------|
| $0200  | Version       | INIT main()        |
| $0201  | GetVol        | TrapGetVolInfo     |
| $0205  | Read          | TrapRead           |
| $0206  | Close         | TrapClose, error cleanup in TrapOpen |
| $020A  | GetWDInfo     | TrapGetWDInfo      |
| $020B  | OpenWD        | TrapOpenWD         |
| $020C  | CloseWD       | TrapCloseWD        |
| $020D  | DbgLog        | debug logging      |
| $020E  | GuestVars     | get_globals/set_globals |
| $020F  | LogTrap       | log_trap           |
| $0211  | Write         | TrapWrite          |
| $0214  | Fatal         | dbg_fatal          |
| $0215  | CreateDir     | TrapDirCreate      |
| $0216  | CatMove       | TrapCatMove        |
| $0218  | SetEOF        | TrapSetEOF         |
| $0219  | GetDirInfo    | TrapGetCatInfo (directory DInfo) |
| $021A  | SetDirInfo    | TrapSetCatInfo (directory DInfo) |
| $0220  | OpenByName    | *(available but not used — guest uses $0223)* |

### 3.1.2 — Commands to deprecate

These are fully superseded by coarse commands and no longer called:

| Cmd    | Name             | Replaced by                     |
|--------|------------------|---------------------------------|
| $0202  | GetCatInfo       | $0224 GetCatInfoResolved        |
| $0203  | GetCatInfoByName | $0224 GetCatInfoResolved        |
| $0204  | Open (by CNID)   | $0223 ResolveAndOpen            |
| $0207  | GetFileInfo      | $0222 GetFileInfoByName         |
| $0208  | ReadDir          | $0221 GetCatInfoFull (indexed)  |
| $0209  | ObjByName        | $0222/$0223/$0225 (integrated)  |
| $0210  | CreateFile       | $0225 FileOpByName              |
| $0212  | DeleteFile       | $0225 FileOpByName              |
| $0213  | SetFileInfo      | $0225 FileOpByName              |
| $0217  | Rename           | $0225 FileOpByName              |

### 3.1.3 — Modify `src/core/extn_extfs.cpp`

Add `/* DEPRECATED — superseded by $0224 */` comments to each
constant and case block:

```cpp
static constexpr uint16_t kExtFSGetCatInfo = 0x202;     /* DEPRECATED → $0224 */
static constexpr uint16_t kExtFSGetCatInfoName = 0x203;  /* DEPRECATED → $0224 */
static constexpr uint16_t kExtFSOpen = 0x204;            /* DEPRECATED → $0223 */
static constexpr uint16_t kExtFSGetFileInfo = 0x207;     /* DEPRECATED → $0222 */
static constexpr uint16_t kExtFSReadDir = 0x208;         /* DEPRECATED → $0221 */
static constexpr uint16_t kExtFSObjByName = 0x209;       /* DEPRECATED → $0222/$0223 */
static constexpr uint16_t kExtFSCreateFile = 0x210;      /* DEPRECATED → $0225 */
static constexpr uint16_t kExtFSDeleteFile = 0x212;      /* DEPRECATED → $0225 */
static constexpr uint16_t kExtFSSetFileInfo = 0x213;     /* DEPRECATED → $0225 */
static constexpr uint16_t kExtFSRename = 0x217;          /* DEPRECATED → $0225 */
```

### Fence

- [ ] Comments added, no code change
- [ ] Build clean
- [ ] Commit: `"extfs: mark 10 deprecated commands"`

---

## Phase 3.2 — Add Deprecation Logging

Add a log warning when deprecated commands are called.  This helps
verify that the new INIT never calls them, and catches any
third-party or debugging tool that still does.

### 3.2.1 — Modify deprecated case blocks

For each deprecated command, add at the top of its case:

```cpp
case kExtFSGetCatInfo:
{
    dbg_printf("[ExtFS] WARNING: deprecated cmd $%03x (use $%03x)\n",
               cmd, kExtFSGetCatInfoResolved);
    // ... existing code unchanged ...
}
```

Keep the existing handler code intact — the commands still work,
they just log a warning.

### Fence

- [ ] Deprecation warnings appear in log when old commands are called
- [ ] Boot with new INIT → no deprecation warnings in log
- [ ] Commit: `"extfs: log deprecation warnings for old commands"`

---

## Phase 3.3 — Remove Deprecated Command Handlers

After confirming (Phase 3.2) that the new INIT never calls the old
commands, remove their handler code.  Replace each deprecated case
with a stub that returns an error:

### 3.3.1 — Modify `src/core/extn_extfs.cpp`

Replace each deprecated case block with:

```cpp
case kExtFSGetCatInfo:     /* REMOVED — use $0224 */
case kExtFSGetCatInfoName: /* REMOVED — use $0224 */
case kExtFSOpen:           /* REMOVED — use $0223 */
case kExtFSGetFileInfo:    /* REMOVED — use $0222 */
case kExtFSReadDir:        /* REMOVED — use $0221 */
case kExtFSObjByName:      /* REMOVED — use $0222/$0223 */
case kExtFSCreateFile:     /* REMOVED — use $0225 */
case kExtFSDeleteFile:     /* REMOVED — use $0225 */
case kExtFSSetFileInfo:    /* REMOVED — use $0225 */
case kExtFSRename:         /* REMOVED — use $0225 */
    dbg_printf("[ExtFS] REMOVED cmd $%03x — update guest INIT\n", cmd);
    regResult = fmErrToReg(storage::FMErr::kParamErr);
    break;
```

This saves ~250 lines of handler code from `extn_extfs.cpp`.

### Fence

- [ ] Removed commands return `paramErr`
- [ ] New INIT still works perfectly (never calls removed commands)
- [ ] `extn_extfs.cpp` is ~250 lines shorter
- [ ] Build clean, tests pass
- [ ] Commit: `"extfs: remove 10 deprecated command handlers"`

---

## Phase 3.4 — Clean Up Command Constant Numbering

With the deprecated constants still defined but handlers removed,
consider whether to also remove the `constexpr` declarations.

### 3.4.1 — Decision

**Keep** the constants with `/* REMOVED */` annotations.  They serve
as documentation of the historical command numbering and prevent
accidental reuse of those command codes.  Don't renumber anything.

Remove `$0220 OpenByName` if it's not used by the new INIT either
(the INIT uses `$0223 ResolveAndOpen` instead).  Handle it the same
way as the other removed commands.

### 3.4.2 — Modify `src/core/extn_extfs.cpp`

Add `$0220` to the removed case block if unused.  Update the
catalog-validation `switch` to only list commands that still exist.

### Fence

- [ ] No orphaned constants or unreachable code
- [ ] Build clean
- [ ] Commit: `"extfs: final command table cleanup"`

---

## Phase 3.5 — Update Documentation

### 3.5.1 — Update `SHAREDRIVE.md`

Update the Architecture (overview) section to reflect the new
command set:

- Document the coarse commands ($0220–$0225) as the primary API
- Note that the fine-grained commands ($0202–$0213, $0217) are
  removed
- Update the register block description (12 params, not 7)

### 3.5.2 — Update the `init.c` file header comment

Replace the old register interface table (listing $0200–$021A) with
the current command set.  The header should document only commands
the INIT actually calls.

### 3.5.3 — Update `SHAREDRIVE_DESIGN.md`

Add a "Status" note at the top:

```
**Status:** Implemented.  All three phases complete.
```

### Fence

- [ ] Documentation matches actual implementation
- [ ] No references to removed commands (except in historical notes)
- [ ] Commit: `"docs: update SharedDrive documentation for new architecture"`
