# INIT Clipboard Sync — Implementation Plan

## Status: Phases 1–3 DONE

Phases 1–3 committed. Phase 4 (manual testing) remains.

## Overview

Replace the ClipSync polling app with an INIT that installs a jGNEFilter.
The filter fires inside each app's partition on every GetNextEvent/WaitNextEvent
call, bypassing MultiFinder's per-partition scrap isolation entirely.

State that the filter needs between calls is stored on the host via new
key-value MMIO commands, avoiding all A5/globals issues.

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Host (emulator)                                │
│  ┌──────────────┐  ┌─────────────────────────┐  │
│  │ extn_clip.cpp│  │ Key-value store          │  │
│  │ $100–$105    │  │ $106 KVSet(key, val)     │  │
│  │ (existing)   │  │ $107 KVGet(key) → val    │  │
│  │              │  │ map<uint32, uint32>       │  │
│  └──────────────┘  └─────────────────────────┘  │
└─────────────────────────────────────────────────┘
        ▲ MMIO registers at extnBlockBase+$20
        │
┌───────┼─────────────────────────────────────────┐
│  Guest (Mac)                                    │
│       │                                         │
│  ┌────┴──────────────────────────────────┐      │
│  │  jGNEFilter (INIT, system heap)       │      │
│  │                                       │      │
│  │  On each GNE call:                    │      │
│  │    appId = *(short*)0x0900            │      │
│  │                                       │      │
│  │    Host→Mac:                          │      │
│  │      hostSeq = ClipSeqNo              │      │
│  │      lastSeq = KVGet(appId*2)         │      │
│  │      if hostSeq ≠ lastSeq:            │      │
│  │        ClipImport → ZeroScrap+PutScrap│      │
│  │        KVSet(appId*2, hostSeq)        │      │
│  │                                       │      │
│  │    Mac→Host:                          │      │
│  │      scrapCnt = *(short*)0x0968       │      │
│  │      lastCnt  = KVGet(appId*2+1)      │      │
│  │      if scrapCnt ≠ lastCnt:           │      │
│  │        GetScrap → ClipExport          │      │
│  │        KVSet(appId*2+1, scrapCnt)     │      │
│  └───────────────────────────────────────┘      │
└─────────────────────────────────────────────────┘
```

## Phases

### Phase 1 — Host-side key-value store

Add two new extension commands to `extn_clip.cpp`:

| Cmd    | Name      | Params                        |
|--------|-----------|-------------------------------|
| $106   | ClipKVSet | p0=key, p1=value              |
| $107   | ClipKVGet | p0=key → p0=value             |

Implementation:
- `static std::unordered_map<uint32_t, uint32_t> s_kvStore;`
- KVSet: `s_kvStore[regParam[0]] = regParam[1]; regResult = 0;`
- KVGet: `regParam[0] = s_kvStore[regParam[0]]; regResult = 0;`
  (returns 0 for unknown keys)

Files: `src/core/extn_clip.cpp`

**Gate:** Build imgui + headless. Run golden tests.

### Phase 2 — INIT source (THINK C)

Create `macsrc/clipsync/init.c` — a THINK C project that builds an INIT resource.

**Structure:**

```c
/* Globals stored in a struct right after the asm stub,
   found via a PC-relative trick or allocated in system heap */
typedef struct {
    long     oldFilter;     /* previous jGNEFilter */
    char    *regBase;       /* extension register base */
    long     lastTicks;     /* throttle: last check time */
} FilterGlobals;
```

**INIT installer (`main`):**
1. Call `find_reg_base()` — if NULL, bail (no emulator extension)
2. `ClipVersion` check — if version < 2, bail (need KV commands)
3. Allocate system heap block for filter code + FilterGlobals
4. Copy filter entry point + globals into it
5. Save current `jGNEFilter` ($029A) into `oldFilter`
6. Set `jGNEFilter` to our asm entry point

**jGNEFilter entry (inline asm or separate .a file):**
```asm
; A1 = EventRecord*, D0 = result boolean
; We must preserve both and chain to old filter

FilterEntry:
    MOVEM.L D1-D2/A0-A1, -(SP)   ; save regs
    MOVE.L  A1, -(SP)              ; push EventRecord*
    JSR     SyncOnGNE              ; call C function
    ADDQ.L  #4, SP
    MOVEM.L (SP)+, D1-D2/A0-A1   ; restore regs
    MOVE.L  oldFilter, -(SP)       ; chain to old filter
    RTS                            ; (or JMP if old is nil)
```

**`SyncOnGNE` (C function):**
```c
void SyncOnGNE(EventRecord *evt)
{
    FilterGlobals *g = GetFilterGlobals(); /* PC-relative or fixed addr */
    long now;
    short appId;
    unsigned long hostSeq, lastSeq;
    short scrapCnt;
    unsigned long lastCnt;
    
    /* Throttle: check at most every 30 ticks (~0.5s) */
    now = TickCount();
    if (now - g->lastTicks < 30)
        return;
    g->lastTicks = now;
    
    appId = *(short *)0x0900;  /* CurApRefNum */
    
    /* --- Host → Mac --- */
    reg_command(g->regBase, 0x0105);            /* ClipSeqNo */
    hostSeq = reg_get_p0(g->regBase);
    
    reg_set_p0(g->regBase, (unsigned long)(appId * 2));
    reg_command(g->regBase, 0x0107);            /* KVGet */
    lastSeq = reg_get_p0(g->regBase);
    
    if (hostSeq != lastSeq) {
        ImportHostToMac(g->regBase);
        /* Store new seq for this app */
        reg_set_p0(g->regBase, (unsigned long)(appId * 2));
        reg_set_p1(g->regBase, hostSeq);
        reg_command(g->regBase, 0x0106);        /* KVSet */
    }
    
    /* --- Mac → Host --- */
    scrapCnt = *(short *)0x0968;  /* ScrapCount */
    
    reg_set_p0(g->regBase, (unsigned long)(appId * 2 + 1));
    reg_command(g->regBase, 0x0107);            /* KVGet */
    lastCnt = reg_get_p0(g->regBase);
    
    if ((unsigned long)scrapCnt != lastCnt) {
        ExportMacToHost(g->regBase);
        reg_set_p0(g->regBase, (unsigned long)(appId * 2 + 1));
        reg_set_p1(g->regBase, (unsigned long)scrapCnt);
        reg_command(g->regBase, 0x0106);        /* KVSet */
    }
}
```

**Reused from main.c:**
- `find_reg_base()`, register access helpers
- `ImportHostToMac()`, `ExportMacToHost()` (slightly adapted — no printf)

Files: `macsrc/clipsync/init.c`

**Gate:** Compiles in THINK C. INIT installs without crash. Filter chains correctly.

### Phase 3 — Bump version, update docs

- `ClipVersion` returns 2 (was 1) to indicate KV support
- Update `macsrc/clipsync/TODO.md`
- Update `docs/TODO_CLIPBOARD.md`

**Gate:** Build imgui + headless. Run golden tests.

### Phase 4 — Testing

1. Boot with Finder only (no MultiFinder)
   - Host→Mac: copy on host, verify Mac app sees it
   - Mac→Host: copy in Mac app, verify host sees it
2. Boot with MultiFinder
   - Host→Mac: copy on host, switch between apps, verify each sees it
   - Mac→Host: copy in App A, verify host gets it; switch to App B, copy there, verify host updates
3. Edge cases:
   - Empty clipboard
   - Large clipboard (>4K)
   - Rapid switching between apps
   - App that doesn't call GNE frequently (busy loop)

## Key Design Decisions

1. **State on host, not on Mac** — avoids A5 world issues entirely.
   KV store is just MMIO reads/writes, works from any partition.

2. **CurApRefNum as app identifier** — a word at $0900, unique per
   open app. Gives us a stable partition identifier for KV keys.

3. **Throttle to 30 ticks** — jGNEFilter fires potentially hundreds
   of times per second. TickCount() comparison is one memory read,
   negligible overhead. Full sync only runs ~2x/sec.

4. **No TE scrap manipulation** — we only touch the desk scrap.
   Apps that use private/TE scraps will pick up changes via their
   normal TEFromScrap calls on activate/resume.

5. **Bidirectional in one filter** — both import and export in the
   same GNE call. Export check is just reading ScrapCount ($0968),
   which is already in the current partition's low memory.

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| ZeroScrap/PutScrap from filter context | These are normal Toolbox calls, safe from GNE time. Not at interrupt level. |
| GetScrap/NewHandle from filter | Allocates in current app's heap. Should be fine — GNE is called at app level. |
| Filter slows down event loop | Throttle + fast MMIO reads. Worst case 6 MMIO reads per 30 ticks. |
| App doesn't call GNE | Same as MultiFinder — that app won't see clipboard changes until it does. |
| CurApRefNum collision | RefNums are unique per open resource file. No collision possible. |
| INIT runs before extensions ready | `find_reg_base` returns NULL → INIT bails cleanly. |
