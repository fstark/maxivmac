# ClipSync TODO

## Status

- Host→Mac import works under Finder (single app)
- Does NOT work under MultiFinder — per-partition scrap isolation
- UnloadScrap() after PutScrap did not help
- Need deeper investigation into MultiFinder scrap propagation

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

## Key Questions to Resolve

1. **How does MultiFinder propagate scrap between partitions?**
   - TN#180 says it watches `_SysEdit` and menu events
   - Does it also check on resume? Does it read the Clipboard File?
   - What exactly triggers `LoadScrap` in the receiving partition?

2. **Suspend/Resume event clipboard bit**
   - app4Evt (event code 15), message low byte: 0=suspend, 1=resume
   - message bit 1: "convertClipboardFlag" — if set, app should convert scrap
   - Need IM Vol V to confirm exact format and semantics

3. **Could we patch `_ZeroScrap`/`_PutScrap` from an INIT?**
   - Runs in the calling app's partition context
   - Could intercept and propagate to other partitions
   - But: how to find other partitions' scrap globals?

4. **Could we patch `_GetNextEvent`/`_WaitNextEvent`?**
   - INIT trap patch runs in each app's context
   - Could check host clipboard and import before returning
   - Avoids per-partition isolation entirely
   - But: A5 management, reentrancy, must not move memory at wrong time

5. **Alternative: work at the TE scrap level?**
   - Directly write `TEScrpHandle` ($0AB4) and `TEScrpLength` ($0AB0)
   - Bypasses desk scrap entirely for TE-based apps
   - But: only works for TEXT, doesn't help non-TE apps
