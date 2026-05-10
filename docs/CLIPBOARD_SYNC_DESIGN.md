# Clipboard Sync — Redesign

Replaces the current polling-and-comparing change detection with an
event-driven sequence number and explicit staging/commit protocol.
Fixes the "only first image sync works" bug and eliminates all
feedback loops by construction.

Applies to all clipboard types (TEXT, PICT, future).  The rendering
mechanics (two-pass compositing, QuickDraw, PNG encode/decode) are
unchanged — see [PICT_CLIPBOARD_DESIGN.md](PICT_CLIPBOARD_DESIGN.md).

---

## 1. Problem Statement

The current design polls the host clipboard every 30 ticks and
compares content to detect changes.  This has three problems:

1. **Image identity is a boolean.**  `ClipSeqNo` tracks "has image"
   as a `bool`.  Once `true`, a different image on the host clipboard
   is invisible — `true == true`, no seqno bump, guest never learns
   about it.

2. **Feedback loops are patched, not prevented.**  When the guest
   imports an image from the host, it calls `PutScrap(PICT)` which
   bumps `ScrapCount`, which triggers re-export of the same image
   back to the host.  `ExtnClipMarkImageExported()` attempts to
   suppress re-detection but fails for multiple consecutive images.

3. **SDL feedback is unhandled.**  When we call
   `SDL_SetClipboardData()` to publish guest content, SDL fires
   `SDL_EVENT_CLIPBOARD_UPDATE`, which we could misinterpret as a
   new external change.

---

## 2. Design

### 2.1 Host-Side Pasteboard Object

A single `HostPasteboard` object holds a snapshot of the host
clipboard.  It is the **single source of truth** for what the host
clipboard contains — all guest queries read from it, never from SDL
directly.

```cpp
struct HostPasteboard {
    std::string  text;         // MacRoman-encoded text (empty = none)
    std::vector<uint8_t> png;  // PNG-encoded image (empty = none)
    int          imgW = 0;     // image width (0 = no image)
    int          imgH = 0;     // image height
    uint32_t     seq  = 0;     // monotonic sequence number
    std::mutex   mu;           // protects all fields
};
```

### 2.2 SDL Event Handler (main thread)

On `SDL_EVENT_CLIPBOARD_UPDATE`:

```
text = SDL_GetClipboardText()           // outside lock
png  = SDL_GetClipboardData("image/png") // outside lock

lock(mu):
    if text != pasteboard.text || png != pasteboard.png:
        pasteboard.text = text
        pasteboard.png  = png
        pasteboard.imgW, pasteboard.imgH = decode PNG header
        pasteboard.seq++
```

Content comparison suppresses SDL-side feedback: when we publish
guest content to SDL, the resulting `SDL_EVENT_CLIPBOARD_UPDATE`
reads back the same data, comparison fails, no seqno bump.

The SDL reads happen **outside** the lock because
`SDL_GetClipboardText()` and `SDL_GetClipboardData()` can block on
OS IPC.  The lock only protects the swap-and-increment.

### 2.3 Guest Export: Staging + Commit

When the guest's desk scrap changes, the INIT sends data in
stages, then issues an explicit commit:

```
1. kPictExport pass=0   → host stages white-bg pixels
2. kPictExport pass=1   → host stages black-bg pixels, composites PNG
3. kClipExport          → host stages TEXT
4. kClipCommit (new)    → host atomically publishes staged content
                           to SDL and returns new host seqno
```

The commit command:

```
ClipCommit    $10C
  (no input parameters)
  p0 = new host sequence number (output)
  result: 0 = ok
```

**On the host side**, `kClipCommit`:

1. Takes the lock.
2. Publishes staged text via `SDL_SetClipboardText()` (if present).
3. Publishes staged PNG via `SDL_SetClipboardData()` (if present).
4. Updates `pasteboard.text` and `pasteboard.png` to match what was
   just published — so the next `SDL_EVENT_CLIPBOARD_UPDATE` sees
   identical content and does not bump the seqno (feedback
   suppression).
5. Increments `pasteboard.seq`.
6. Returns the new seqno to the guest.
7. Clears staging buffers.

If neither TEXT nor PICT was staged, commit is a no-op and returns
the current seqno.

**Ordering:** The commit is always the last call.  PICT passes and
TEXT can arrive in any order before it.  If the guest has only TEXT
(no PICT), it stages TEXT then commits.  If it has only PICT (no
TEXT), it stages PICT passes then commits.

### 2.4 Guest Import

When the host seqno changes, the guest imports:

```
1. kClipSeqNo           → read host seqno
2. if seqno != lastSeq:
3.   kClipImport        → get TEXT (if any)
4.   kPictHasImage      → check for image
5.   kPictImport        → get pixels (if image present)
6.   PutScrap(TEXT/PICT) on guest side
7.   store new seqno as lastSeq
8.   store new ScrapCount to suppress re-export
```

After import, the guest updates both its stored `hostSeq` and
`ScrapCount`.  This prevents the `PutScrap` bump from triggering a
re-export.

### 2.5 Revised Guest Sync Loop

```c
void SyncClipboard(Globals *g)
{
    /* Throttle: 30 ticks */
    if (TickCount() - g->lastClipTicks < 30) return;
    g->lastClipTicks = TickCount();

    scrapCnt = *(short *)kScrapCount;

    /* --- Mac changed? Export to host --- */
    if (scrapCnt != g->lastScrapCnt)
    {
        /* Stage TEXT */
        ExportTextToHost(g->regBase);       /* kClipExport */
        /* Stage PICT (two passes) */
        ExportPictToHost(g->regBase);       /* kPictExport x2 */
        /* Commit — get new host seqno */
        reg_command(g->regBase, kClipCommit);
        g->lastHostSeq  = reg_get(g->regBase, 0);
        g->lastScrapCnt = scrapCnt;
        return; /* done for this tick */
    }

    /* --- Host changed? Import to Mac --- */
    reg_command(g->regBase, kClipSeqNo);
    hostSeq = reg_get(g->regBase, 0);

    if (hostSeq != g->lastHostSeq)
    {
        ImportTextFromHost(g->regBase);     /* kClipImport */
        ImportPictFromHost(g->regBase);     /* kPictHasImage + kPictImport */
        g->lastHostSeq  = hostSeq;
        /* Update ScrapCount to suppress re-export */
        g->lastScrapCnt = *(short *)kScrapCount;
    }
}
```

Note: `lastHostSeq` and `lastScrapCnt` are stored in the `Globals`
struct (or in KV store for MultiFinder).  No more separate per-app
keys multiplied by 2 — just two values.

---

## 3. Feedback Suppression Summary

| Feedback path | How it's prevented |
|---|---|
| Guest exports → SDL fires event → host bumps seqno → guest re-imports | `SDL_EVENT_CLIPBOARD_UPDATE` handler compares content; staged data was written to pasteboard in commit, so content matches → no seqno bump |
| Guest imports → `PutScrap` bumps `ScrapCount` → guest re-exports | Guest stores `ScrapCount` after import → no re-export detected |
| Host changes → guest imports AND simultaneously guest changed | `ScrapCount` check runs first; if guest changed, export wins. Host change is overwritten (unavoidable — can't merge clipboards) |

---

## 4. Threading Model

Two threads touch the pasteboard:

| Thread | Reads | Writes |
|---|---|---|
| **Main (SDL)** | Full SDL clipboard (on event) | `pasteboard.{text,png,seq}` |
| **Emu (68k)** | `pasteboard.{text,png,seq}` (on ClipSeqNo, ClipImport, PictHasImage, PictImport) | `pasteboard.{text,png,seq}` (on ClipCommit) |

All access is guarded by `pasteboard.mu`.  Lock hold time is a
pointer swap + integer increment — no SDL calls under the lock.

SDL clipboard reads (`SDL_GetClipboardText`, `SDL_GetClipboardData`)
happen **before** taking the lock.  SDL clipboard writes
(`SDL_SetClipboardText`, `SDL_SetClipboardData`) happen **inside**
the commit handler, but these are just enqueuing data for SDL — they
don't block on IPC.

---

## 5. Removed Mechanisms

The following are eliminated by this design:

| Removed | Reason |
|---|---|
| `s_lastHasImage` bool | Replaced by content comparison in SDL event handler |
| `s_lastClipText` in `ClipSeqNo` | Replaced by pasteboard snapshot |
| `ExtnClipMarkImageExported()` | Replaced by commit-updates-pasteboard |
| `s_justImported` / dimension-based feedback suppression | Replaced by ScrapCount store after import |
| Per-app KV keys `appId * 2`, `appId * 2 + 1` | Replaced by two flat values in Globals |
| Polling-based content comparison in `ClipSeqNo` handler | `ClipSeqNo` just returns `pasteboard.seq` |

---

## 6. New/Changed Commands

| Command | Code | Change |
|---|---|---|
| `ClipSeqNo` | $105 | Now just returns `pasteboard.seq` — no polling, no comparison |
| `ClipExport` | $101 | Stages text; no longer publishes immediately |
| `PictExport` | $109 | Stages pixels; unchanged semantics but no longer calls `SDL_SetClipboardData` directly |
| **`ClipCommit`** | **$10C** | **New.** Publishes staged content, returns new seqno |
| `ClipImport` | $102 | Reads from `pasteboard.text` instead of calling SDL |
| `PictHasImage` | $10A | Reads from `pasteboard.{imgW,imgH}` |
| `PictImport` | $10B | Decodes from `pasteboard.png` |

`ClipVersion` returns **4** (was 3).

---

## 7. Diagnostic Logging

All host logs use `DIAG(CLIP, ...)`.  Guest logs use `dbg_log()`.

### 7.1 Host: SDL Event Handler

```
[CLIP] SDL clipboard update: text=142B png=4821B → seq 3→4
[CLIP] SDL clipboard update: identical content, skipped
```

### 7.2 Host: ClipSeqNo ($105)

```
[CLIP] ClipSeqNo: returning seq=4
```

### 7.3 Host: ClipExport ($101) — staging

```
[CLIP] ClipExport: staged 142 bytes text
```

### 7.4 Host: PictExport ($109) — staging

```
[CLIP] PictExport: staged pass=0 236x170 depth=1 rb=30
[CLIP] PictExport: staged pass=1 236x170 depth=1 rb=30 → composited 1900B PNG
```

Compositing happens at pass 1 (not at commit).  The staged PNG is
ready before commit is called.

### 7.5 Host: ClipCommit ($10C)

```
[CLIP] ClipCommit: publishing text=142B png=1900B → seq 4→5
[CLIP] ClipCommit: nothing staged, returning seq=4
```

### 7.6 Host: ClipImport ($102)

```
[CLIP] ClipImport: 142 bytes → guest $0002C886
```

### 7.7 Host: PictHasImage ($10A)

```
[CLIP] PictHasImage: has=1 236x170
[CLIP] PictHasImage: has=0
```

### 7.8 Host: PictImport ($10B)

```
[CLIP] PictImport: 236x170 depth=1 rb=30 → guest $0002D600
[CLIP] PictImport: wrote 236x170 depth=1 into guest RAM
```

### 7.9 Guest: Sync Loop

```
Sync: mac changed cnt 3→4, exporting
Sync: exported text=94B pict=236x170, commit → hostSeq=5
Sync: host changed seq 5→6, importing
Sync: imported text=142B pict=328x268, scrapCnt now 5
```

These guest logs confirm feedback prevention is working: after an
import, the next tick should NOT log "mac changed" (because
`lastScrapCnt` was updated).  After an export+commit, the next tick
should NOT log "host changed" (because `lastHostSeq` was updated
from the commit response).

---

## 8. Source Files Affected

| File | Change |
|---|---|
| `src/core/extn_clip.cpp` | Replace polling with pasteboard reads; add `kClipCommit`; staging logic |
| `src/core/extn_clip.h` | Remove `ExtnClipMarkImageExported`; add `HostPasteboard` |
| `src/core/extn_clip_pict.cpp` | Remove feedback suppression state; stage PNG instead of publishing |
| `src/platform/common/clipboard.cpp` | Add `SDL_EVENT_CLIPBOARD_UPDATE` handler; pasteboard update logic |
| `src/platform/common/clipboard.h` | Expose `HostPasteboard` or event hookup |
| `macsrc/init/clip.c` | Rewrite `SyncClipboard` per §2.5; store seqno in Globals |
| `macsrc/init/defs.h` | Add `kClipCommit` ($10C); add fields to Globals |
