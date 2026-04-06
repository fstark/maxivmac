# ClipSync TODO

## Status

- **main.c** — bidirectional sync console app.  Works under Finder
  (single app).  Does NOT work under MultiFinder due to per-partition
  scrap isolation.  Kept for reference/debugging.
- **init.c** — jGNEFilter INIT that bypasses MultiFinder isolation.
  Filter fires in each app's partition context.  Per-app state stored
  on host via KV commands ($106/$107).  Throttled to 30 ticks.
  Requires extension version >= 2.
- UnloadScrap() after PutScrap did not fix MultiFinder isolation

## Relevant Documentation

### Inside Macintosh (in macdocs/tech_doc/im202.html)

| Chapter | Lines | Topic |
|---------|-------|-------|
| im009 | 9827–11404 | TextEdit — TE scrap, TEFromScrap/TEToScrap, TECut/TECopy/TEPaste, Styled TE |
| im020 | 22705–24192 | Dialog Manager — DlgCut/DlgCopy/DlgPaste, editText items |
| im038 | 44779–45214 | Scrap Manager — desk scrap, ZeroScrap/PutScrap/GetScrap, UnloadScrap/LoadScrap, scrap format, private scraps |
| im052 | 54751–55748 | Toolbox Event Manager — event types, GetNextEvent/WaitNextEvent |

### Technical Notes (in macdocs/tech_doc/tn405.html)

| TN# | Lines | Topic |
|------|-------|-------|
| TN#158 | 15083–15326 | Frequently Asked MultiFinder Questions — suspend/resume, SIZE resource, WaitNextEvent |
| TN#180 | 17918–18320 | MultiFinder Miscellanea — **key doc**: per-partition scrap, _SysEdit watching, scrapCount propagation, suspend/resume events |
| TN#205 | 20527–? | MultiFinder Revisited: The 6.0 System Release — updated MF behavior, SIZE resource flags, scrap handling |
| TN#208 | 21160–? | Setting and Restoring A5 — required for INIT/VBL/interrupt code under MultiFinder |

### Documentation NOT in our set (need to find)

| Doc | Topic | Why we need it |
|-----|-------|----------------|
| Inside Macintosh Vol V, ch 2 | MultiFinder Environment | Suspend/resume event format, convertClipboardFlag (message bit 1), SIZE resource flags |
| Inside Macintosh Vol V, ch 9 | Compatibility Guidelines | MultiFinder-aware app requirements |
| Programmer's Guide to MultiFinder (APDA) | Full MultiFinder programming guide | Definitive reference for suspend/resume, scrap conversion, SIZE resource, background processing |

## Key Questions (Resolved)

1. **How does MultiFinder propagate scrap between partitions?**
   - Answer: It watches `_SysEdit` and menu events (TN#180).
     On resume with convertClipboardFlag set, the app should call
     ConvertDeskScrapToPrivate.  But this is insufficient for us.
   - Solution: jGNEFilter bypasses all of this — it writes directly
     to whatever partition is active.

2. **Approach chosen: jGNEFilter ($029A) INIT**
   - Filter fires in each app's partition context on GNE/WNE
   - Uses CurApRefNum ($0900) as per-app identifier
   - State stored on host via KV commands — no A5/globals issues
   - See init.c for implementation

## Remaining Work

- Test under MultiFinder with multiple apps
- Test edge cases: empty clipboard, large clipboard (>4K)
- Consider adding support for PICT scrap type
