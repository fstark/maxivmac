# Clipboard Sync — Implementation Plan

Design: [CLIPBOARD_SYNC_DESIGN.md](CLIPBOARD_SYNC_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | HostPasteboard struct + SDL event handler | done |
| 2 | Rewire ClipSeqNo, ClipImport, PictHasImage, PictImport to read from pasteboard | done |
| 3 | Stage-and-commit: ClipExport, PictExport staging + kClipCommit | done |
| 4 | Guest-side sync loop rewrite | done |
| 5 | Remove dead code + version bump | done |
| 6 | Manual integration test | |

Build gate: `cmake --build bld/macos`
Test gate:  `bld/macos/tests`

---

## Phase 1 — HostPasteboard + SDL Event Handler

Introduce the `HostPasteboard` object as the single source of truth
for host clipboard state.  Wire it to `SDL_EVENT_CLIPBOARD_UPDATE`
so it captures every external change with content deduplication.

### 1.1 — New file: `src/core/host_pasteboard.h`

```cpp
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

/*
    Single source of truth for the host clipboard.
    Updated by the SDL event handler (main thread).
    Read by clipboard commands on the emu thread.
    All access must hold mu_.
*/
struct HostPasteboard {
    std::string        text;    // MacRoman-encoded text (empty = none)
    std::vector<uint8_t> png;   // PNG-encoded image data (empty = none)
    int                imgW = 0;
    int                imgH = 0;
    uint32_t           seq  = 0;
    std::mutex         mu;
};

/* Global pasteboard instance. */
HostPasteboard &GetHostPasteboard();

/*
    Called from the SDL event loop on SDL_EVENT_CLIPBOARD_UPDATE.
    Reads the current system clipboard, compares with the snapshot,
    and increments seq if content changed.
*/
void HostPasteboardOnClipboardUpdate();
```

### 1.2 — New file: `src/core/host_pasteboard.cpp`

```cpp
#include "core/host_pasteboard.h"
#include "core/diag.h"
#include "util/macroman.h"

#include <cstring>

#ifdef HAVE_SDL
#include <SDL3/SDL.h>
#endif

#include "stb_image.h"

static HostPasteboard s_pasteboard;

HostPasteboard &GetHostPasteboard()
{
    return s_pasteboard;
}

void HostPasteboardOnClipboardUpdate()
{
#ifdef HAVE_SDL
    // Read from SDL — outside the lock (may block on IPC)
    char *utf8 = SDL_GetClipboardText();
    std::string newText;
    if (utf8)
    {
        newText = MacRomanFromUTF8(utf8);
        SDL_free(utf8);
    }

    size_t pngLen = 0;
    void *pngRaw = SDL_GetClipboardData("image/png", &pngLen);
    std::vector<uint8_t> newPng;
    int newW = 0, newH = 0;
    if (pngRaw && pngLen > 0)
    {
        newPng.assign(static_cast<uint8_t *>(pngRaw),
                      static_cast<uint8_t *>(pngRaw) + pngLen);
        SDL_free(pngRaw);

        int comp = 0;
        stbi_info_from_memory(newPng.data(),
                              static_cast<int>(newPng.size()),
                              &newW, &newH, &comp);
    }

    // Compare and update under the lock
    std::lock_guard lock(s_pasteboard.mu);

    if (newText == s_pasteboard.text && newPng == s_pasteboard.png)
    {
        DIAG(CLIP, "SDL clipboard update: identical content, skipped\n");
        return;
    }

    uint32_t oldSeq = s_pasteboard.seq;
    s_pasteboard.text = std::move(newText);
    s_pasteboard.png  = std::move(newPng);
    s_pasteboard.imgW = newW;
    s_pasteboard.imgH = newH;
    s_pasteboard.seq++;

    DIAG(CLIP, "SDL clipboard update: text=%zuB png=%zuB → seq %u→%u\n",
         s_pasteboard.text.size(), s_pasteboard.png.size(),
         oldSeq, s_pasteboard.seq);
#endif
}
```

### 1.3 — Hook into SDL event loop

In `src/platform/imgui_backend.cpp`, inside the `SDL_PollEvent`
loop, add a case for `SDL_EVENT_CLIPBOARD_UPDATE`:

```cpp
#include "core/host_pasteboard.h"

// Inside the while (SDL_PollEvent(&event)) loop, after existing
// event handling and before the closing brace:
if (event.type == SDL_EVENT_CLIPBOARD_UPDATE)
{
    HostPasteboardOnClipboardUpdate();
    continue;
}
```

### 1.4 — CMakeLists.txt

Add `src/core/host_pasteboard.cpp` to `MAXIVMAC_SOURCES`.

### 1.5 — Tests

No unit tests for this phase — it depends on SDL and real clipboard
hardware.  Verified via DIAG logging in later phases.

### Fence

- [ ] `src/core/host_pasteboard.h` exists with `HostPasteboard` struct
      and `GetHostPasteboard()`, `HostPasteboardOnClipboardUpdate()`
- [ ] `src/core/host_pasteboard.cpp` implements SDL reads + content
      comparison + seqno increment
- [ ] `SDL_EVENT_CLIPBOARD_UPDATE` handled in imgui_backend.cpp
- [ ] Build clean, tests pass
- [ ] Commit: `"clipboard: host pasteboard with SDL event handler (sync phase 1)"`

---

## Phase 2 — Rewire Read Commands to Pasteboard

Change `ClipSeqNo`, `ClipImport`, `PictHasImage`, and `PictImport`
to read from `HostPasteboard` instead of calling SDL directly.

### 2.1 — `ClipSeqNo` ($105) in `extn_clip.cpp`

Replace the polling-and-comparing logic with a simple pasteboard read:

```cpp
case kClipSeqNo:
{
    auto &pb = GetHostPasteboard();
    std::lock_guard lock(pb.mu);
    regParam[0] = pb.seq;
    DIAG(CLIP, "ClipSeqNo: returning seq=%u\n", pb.seq);
    regResult = 0;
}
break;
```

Remove the old `s_lastClipText`, `s_lastHasImage`, `s_lastImageW`,
`s_lastImageH` statics — they are no longer needed.

### 2.2 — `ClipImport` ($102) in `extn_clip.cpp`

Read from `pb.text` instead of calling `hostClipGetTextMacRoman()`:

```cpp
case kClipImport:
{
    auto &pb = GetHostPasteboard();
    std::string text;
    {
        std::lock_guard lock(pb.mu);
        text = pb.text;
    }
    uint32_t guestAddr = regParam[0];
    uint32_t capacity = regParam[1];
    uint32_t actual = static_cast<uint32_t>(
        std::min(static_cast<size_t>(capacity), text.size()));
    DIAG(CLIP, "ClipImport: %u bytes → guest $%08X\n", actual, guestAddr);
    for (uint32_t i = 0; i < actual; i++)
        put_vm_byte(guestAddr + i, static_cast<uint8_t>(text[i]));
    regParam[1] = actual;
    regResult = 0;
}
break;
```

Remove `s_clipCache` and `refreshCache()` — no longer needed.

### 2.3 — `ClipGetLen` ($104) in `extn_clip.cpp`

Read from pasteboard:

```cpp
case kClipGetLen:
{
    auto &pb = GetHostPasteboard();
    std::lock_guard lock(pb.mu);
    regParam[0] = static_cast<uint32_t>(pb.text.size());
    regResult = 0;
}
break;
```

### 2.4 — `PictHasImage` ($10A) in `extn_clip_pict.cpp`

Read from pasteboard instead of calling `HostClipHasImage()`:

```cpp
void HandlePictHasImage(uint32_t regParam[], uint16_t &regResult)
{
    auto &pb = GetHostPasteboard();
    std::lock_guard lock(pb.mu);
    bool has = !pb.png.empty();
    DIAG(CLIP, "PictHasImage: has=%d %dx%d\n", has, pb.imgW, pb.imgH);
    regParam[0] = has ? 1 : 0;
    regParam[1] = static_cast<uint32_t>(pb.imgW);
    regParam[2] = static_cast<uint32_t>(pb.imgH);
    regResult = 0;
}
```

### 2.5 — `PictImport` ($10B) in `extn_clip_pict.cpp`

Decode from `pb.png` instead of calling `HostClipGetImageRGBA()`:

```cpp
void HandlePictImport(uint32_t regParam[], uint16_t &regResult)
{
    uint32_t bufAddr  = regParam[0];
    uint32_t rowBytes = regParam[1];
    uint32_t depth    = regParam[2];
    uint32_t width    = regParam[3];
    uint32_t height   = regParam[4];

    DIAG(CLIP, "PictImport: %ux%u depth=%u rb=%u → guest $%08X\n",
         width, height, depth, rowBytes, bufAddr);

    // Grab the PNG blob under the lock
    std::vector<uint8_t> pngCopy;
    {
        auto &pb = GetHostPasteboard();
        std::lock_guard lock(pb.mu);
        pngCopy = pb.png;
    }

    if (pngCopy.empty())
    {
        DIAG(CLIP, "PictImport: no PNG in pasteboard\n");
        regResult = 1;
        return;
    }

    // Decode PNG to RGBA (outside lock)
    int imgW = 0, imgH = 0, comp = 0;
    uint8_t *pixels = stbi_load_from_memory(
        pngCopy.data(), static_cast<int>(pngCopy.size()),
        &imgW, &imgH, &comp, 4);
    if (!pixels)
    {
        DIAG(CLIP, "PictImport: PNG decode failed\n");
        regResult = 1;
        return;
    }

    // ... dimension check + pixel conversion unchanged ...
```

The pixel conversion loop (RGBATo1Bit / RGBATo32Bit + write to
guest RAM) remains as-is.  Just stbi_image_free at the end:

```cpp
    stbi_image_free(pixels);
    DIAG(CLIP, "PictImport: wrote %ux%u depth=%u into guest RAM\n",
         width, height, depth);
    regResult = 0;
}
```

### 2.6 — `ClipHasData` ($103) in `extn_clip.cpp`

Read from pasteboard:

```cpp
case kClipHasData:
{
    auto &pb = GetHostPasteboard();
    std::lock_guard lock(pb.mu);
    regParam[0] = pb.text.empty() ? 0 : 1;
    regResult = 0;
}
break;
```

### Fence

- [ ] `ClipSeqNo` returns `pasteboard.seq` — no polling
- [ ] `ClipImport`, `ClipGetLen`, `ClipHasData` read from
      `pasteboard.text`
- [ ] `PictHasImage`, `PictImport` read from `pasteboard.png`
- [ ] Old statics removed: `s_lastClipText`, `s_lastHasImage`,
      `s_lastImageW`, `s_lastImageH`, `s_clipCache`, `refreshCache()`
- [ ] Build clean, tests pass
- [ ] Commit: `"clipboard: read commands from host pasteboard (sync phase 2)"`

---

## Phase 3 — Stage-and-Commit: ClipExport, PictExport, ClipCommit

Change guest→host exports to stage data instead of publishing
immediately.  Add `kClipCommit` ($10C) to atomically publish
and return the new seqno.

### 3.1 — Staging state in `extn_clip.cpp`

```cpp
static std::string s_stagedText;
static bool s_hasStakedText = false;

// Declared in extn_clip_pict.cpp, exposed via header:
// staged PNG + staging flag are managed there.
```

### 3.2 — `ClipExport` ($101) stages text

```cpp
case kClipExport:
{
    uint32_t guestAddr = regParam[0];
    uint32_t count = regParam[1];
    DIAG(CLIP, "ClipExport: staged %u bytes text\n", count);
    s_stagedText.resize(count);
    for (uint32_t i = 0; i < count; i++)
        s_stagedText[i] = static_cast<char>(get_vm_byte(guestAddr + i));
    s_hasStakedText = true;
    regResult = 0;
}
break;
```

No SDL calls here — text is only staged.

### 3.3 — `PictExport` ($109) stages PNG

In `extn_clip_pict.cpp`, the pass-1 handler composites and stores
the PNG in a staging buffer instead of calling `HostClipSetImage()`:

```cpp
// After compositing:
s_stagedPng = EncodeRGBAPng(rgba.data(), width, height);
s_hasStagedPng = !s_stagedPng.empty();
DIAG(CLIP, "PictExport: staged pass=1 %dx%d depth=%d "
     "→ composited %zuB PNG\n",
     width, height, depth, s_stagedPng.size());
// Do NOT call HostClipSetImage or ExtnClipMarkImageExported
```

Add accessors in `extn_clip_pict.h`:

```cpp
/* Staging accessors for ClipCommit */
bool HasStagedPng();
std::vector<uint8_t> TakeStagedPng();
```

### 3.4 — New `kClipCommit` ($10C) in `extn_clip.cpp`

```cpp
static constexpr uint16_t kClipCommit = 0x10C;

case kClipCommit:
{
    auto &pb = GetHostPasteboard();
    bool hasPng = HasStagedPng();

    if (!s_hasStakedText && !hasPng)
    {
        std::lock_guard lock(pb.mu);
        DIAG(CLIP, "ClipCommit: nothing staged, returning seq=%u\n",
             pb.seq);
        regParam[0] = pb.seq;
        regResult = 0;
        break;
    }

    // Publish to SDL (these enqueue, don't block significantly)
    std::string textUtf8;
    if (s_hasStakedText)
        textUtf8 = UTF8FromMacRoman({
            reinterpret_cast<const uint8_t *>(s_stagedText.data()),
            s_stagedText.size()});

    std::vector<uint8_t> pngData;
    if (hasPng)
        pngData = TakeStagedPng();

    if (s_hasStakedText)
        SDL_SetClipboardText(textUtf8.c_str());
    if (!pngData.empty())
        HostClipSetImage(pngData.data(), pngData.size());

    // Update pasteboard so SDL event sees identical content
    {
        std::lock_guard lock(pb.mu);
        uint32_t oldSeq = pb.seq;
        if (s_hasStakedText)
            pb.text = std::move(s_stagedText);
        if (!pngData.empty())
        {
            pb.png = std::move(pngData);
            // Decode dimensions for the snapshot
            int comp = 0;
            stbi_info_from_memory(pb.png.data(),
                static_cast<int>(pb.png.size()),
                &pb.imgW, &pb.imgH, &comp);
        }
        pb.seq++;
        DIAG(CLIP, "ClipCommit: publishing text=%zuB png=%zuB "
             "→ seq %u→%u\n",
             pb.text.size(), pb.png.size(), oldSeq, pb.seq);
        regParam[0] = pb.seq;
    }

    s_hasStakedText = false;
    s_stagedText.clear();
    regResult = 0;
}
break;
```

### 3.5 — Dispatch in `ExtnClipDispatch`

Add `case kClipCommit` to the switch in `ExtnClipDispatch()`.

### 3.6 — Guest constant in `defs.h`

```c
#define kClipCommit 0x010C
```

### Fence

- [ ] `ClipExport` stages text, does not call SDL
- [ ] `PictExport` pass 1 stages PNG, does not call
      `HostClipSetImage` or `ExtnClipMarkImageExported`
- [ ] `kClipCommit` publishes staged content to SDL, updates
      pasteboard snapshot, increments seq, returns new seq
- [ ] `kClipCommit` constant added to guest `defs.h`
- [ ] Build clean, tests pass
- [ ] Commit: `"clipboard: staging + commit protocol (sync phase 3)"`

---

## Phase 4 — Guest Sync Loop Rewrite

Rewrite `SyncClipboard()` in `macsrc/init/clip.c` to use the new
protocol per Design §2.5.

### 4.1 — Add fields to Globals in `defs.h`

```c
/* In the Globals struct, replace or augment clip fields: */
unsigned long lastHostSeq;    /* last seen host pasteboard seq */
short         lastScrapCnt;   /* last seen ScrapCount */
```

These replace the per-app KV store usage for clip tracking.

### 4.2 — Rewrite `SyncClipboard()` in `clip.c`

```c
void SyncClipboard(Globals *g)
{
    long now;
    short scrapCnt;
    unsigned long hostSeq;

    /* Throttle: 30 ticks */
    now = TickCount();
    if (now - g->lastClipTicks < 30) return;
    g->lastClipTicks = now;

    scrapCnt = *(short *)kScrapCount;

    /* --- Mac changed? Export to host --- */
    if (scrapCnt != g->lastScrapCnt)
    {
        dbg_log2(g->regBase, "Sync: mac changed cnt %ld→%ld, exporting",
                 (long)g->lastScrapCnt, (long)scrapCnt);
        ExportMacToHost(g->regBase);
        ExportPictToHost(g->regBase);
        reg_command(g->regBase, kClipCommit);
        g->lastHostSeq  = reg_get(g->regBase, 0);
        g->lastScrapCnt = scrapCnt;
        dbg_log1(g->regBase, "Sync: commit → hostSeq=%lu",
                 g->lastHostSeq);
        return;
    }

    /* --- Host changed? Import to Mac --- */
    reg_command(g->regBase, kClipSeqNo);
    hostSeq = reg_get(g->regBase, 0);

    if (hostSeq != g->lastHostSeq)
    {
        dbg_log2(g->regBase, "Sync: host changed seq %lu→%lu, importing",
                 g->lastHostSeq, hostSeq);
        ImportHostToMac(g->regBase);
        ImportPictFromHost(g->regBase);
        g->lastHostSeq  = hostSeq;
        g->lastScrapCnt = *(short *)kScrapCount;
        dbg_log1(g->regBase, "Sync: imported, scrapCnt now %ld",
                 (long)g->lastScrapCnt);
    }
}
```

### 4.3 — Initialize new fields

In `init.c` (or wherever Globals are initialized), set:

```c
g->lastHostSeq  = 0;
g->lastScrapCnt = *(short *)kScrapCount;
```

### 4.4 — Remove KV store usage from clip.c

Delete the `kv_get`/`kv_set` calls with the `appId * 2` /
`appId * 2 + 1` key scheme.  The per-app tracking is replaced by
the flat `lastHostSeq` / `lastScrapCnt` fields.

### Fence

- [ ] `SyncClipboard` uses `lastHostSeq` and `lastScrapCnt` fields
- [ ] Export path: stages → commits → stores returned seqno
- [ ] Import path: reads seqno → imports → stores scrapCount
- [ ] No `kv_get`/`kv_set` in clip.c for clipboard tracking
- [ ] THINK C build succeeds (manual)
- [ ] Commit: `"clipboard: guest sync loop rewrite (sync phase 4)"`

---

## Phase 5 — Remove Dead Code + Version Bump

Clean up mechanisms that the new design replaces.

### 5.1 — Remove from `extn_clip.cpp`

- `s_lastClipText` static
- `s_lastHasImage`, `s_lastImageW`, `s_lastImageH` statics
- `s_clipCache` static and `refreshCache()` function
- `ExtnClipMarkImageExported()` function

### 5.2 — Remove from `extn_clip.h`

- `ExtnClipMarkImageExported()` declaration

### 5.3 — Remove from `extn_clip_pict.cpp`

- `s_justImported`, `s_importedW`, `s_importedH` statics
- The feedback suppression block in `HandlePictExport` ("suppressed
  feedback re-export")

### 5.4 — Remove from `clipboard.h` / `clipboard.cpp`

Functions no longer called directly by command handlers:

- `hostClipGetTextMacRoman()` — still used? Check if anything else
  calls it.  If only the old `ClipSeqNo` used it, remove.
- `HostClipHasImage()` — same check.
- `HostClipGetImageRGBA()` — same check.
- `hostClipHasText()` — same check.

Keep any functions still used by the legacy `HTCEexport`/`HTCEimport`
path or other callers.

### 5.5 — Version bump

In `extn_clip.cpp`, change `kClipVersion` response from 3 to 4.

### 5.6 — Update `ExtnClipReset`

Remove references to deleted statics.  Ensure `ExtnPictReset()`
is still called.  Reset staging state.

### Fence

- [ ] No `s_lastClipText`, `s_lastHasImage`, `s_clipCache`,
      `ExtnClipMarkImageExported`, `s_justImported` in codebase
- [ ] `ClipVersion` returns 4
- [ ] Build clean, tests pass
- [ ] Commit: `"clipboard: remove old sync mechanisms, version 4 (sync phase 5)"`

---

## Phase 6 — Manual Integration Test

Verify all sync paths work correctly with real clipboard usage.
No code changes — this is a human test gate.

### 6.1 — Host → Guest TEXT

1. Copy text on host
2. Switch to guest, paste — text appears
3. Copy different text on host
4. Paste in guest — new text appears (not stuck)

### 6.2 — Host → Guest PICT

1. Copy image on host
2. Paste in guest — image appears
3. Copy a different image on host
4. Paste in guest — **new image appears** (this is the original bug)

### 6.3 — Guest → Host TEXT

1. Copy text in guest app
2. Paste on host — text appears

### 6.4 — Guest → Host PICT

1. Copy image in guest (e.g., select in MacPaint, Edit → Copy)
2. Paste on host — PNG appears

### 6.5 — Feedback loop check

Watch `--diag=CLIP` output.  After each sync, verify:
- After import: no "mac changed" log on next tick
- After export+commit: no "host changed" log on next tick
- After export+commit: "SDL clipboard update: identical content,
  skipped" appears

### 6.6 — Rapid changes

Copy 5 different images quickly on the host.  Guest should eventually
land on the last one.  No stuck state.

### Fence

- [ ] All 6 sub-tests pass
- [ ] No feedback loops visible in DIAG output
- [ ] Commit: (none — test-only gate)
