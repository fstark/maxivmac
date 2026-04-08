# Low-Memory Globals Viewer — Design

## Feature Summary

A debug tool window that displays the Macintosh low-memory globals ($000–$FFF)
with symbolic names, typed values, and change-tracking. Beatrix uses this to
inspect OS/Toolbox state while debugging vintage Mac applications.

From FEATURES.md:
> Beatrix wants to be able to look at the low-memory globals (and quickdraw?),
> with a nice display that shows the variable name.
> She wants to be able to "mark" a moment, and all updated values after that
> would be displayed in red.

---

## Personas

| Persona | Interest |
|---------|----------|
| **Beatrix** (primary) | Inspect OS/Toolbox state, track changes while stepping through app code |
| Dorothee | Occasionally useful when debugging emulator device interactions |

---

## Data Model

### Global Variable Descriptor

Each known low-memory global is described by a compile-time table entry:

```c
typedef enum {
    LM_BYTE,       /*  1 byte  */
    LM_WORD,       /*  2 bytes */
    LM_LONG,       /*  4 bytes */
    LM_POINTER,    /*  4 bytes, displayed as $XXXXXXXX */
    LM_HANDLE,     /*  4 bytes, displayed as $XXXXXXXX (H) */
    LM_PSTRING,    /* length-prefixed Pascal string, up to 32 bytes */
    LM_BYTES,      /* raw byte array, size in `size` field */
    LM_RECT,       /*  8 bytes: top, left, bottom, right (4 words) */
    LM_PATTERN,    /*  8 bytes displayed as pattern */
    LM_OSType,     /*  4 bytes displayed as 4-char code */
} LMType;

typedef enum {
    LM_CAT_SYSTEM,     /* Memory, heap zones, stack */
    LM_CAT_HARDWARE,   /* VIA, SCC, IWM, SCSI base addresses */
    LM_CAT_TIMING,     /* Ticks, Time, calibration */
    LM_CAT_INTERRUPT,  /* Interrupt vectors, VBL queue */
    LM_CAT_IO,         /* File system, drives, serial */
    LM_CAT_EVENTS,     /* Event queue, journal, keyboard */
    LM_CAT_WINDOW,     /* WindowList, WMgrPort, paint flags */
    LM_CAT_MENU,       /* MenuList, MBarHeight, highlights */
    LM_CAT_TEXTEDIT,   /* TE globals, scrap */
    LM_CAT_RESOURCE,   /* Resource maps, chain, error */
    LM_CAT_SCRAP,      /* Clipboard globals */
    LM_CAT_PRINTING,   /* PrintErr */
    LM_CAT_SOUND,      /* SdVolume, SoundBase, SoundPtr */
    LM_CAT_FONT,       /* ApFontID, system font, width tables */
    LM_CAT_APP,        /* CurApName, CurrentA5, ApplZone, etc. */
    LM_CAT_MISC,       /* Scratch areas, constants, flags */
    LM_CAT_COUNT
} LMCategory;

typedef struct {
    const char *name;       /* e.g. "MemTop" */
    uint32_t    addr;       /* e.g. 0x0108 */
    uint16_t    size;       /* byte count (derived from type for fixed types) */
    LMType      type;       /* how to display the value */
    LMCategory  category;   /* grouping for filter UI */
    const char *brief;      /* one-line description, e.g. "Address of end of RAM" */
} LMGlobal;
```

The table is derived from `macdocs/ref/GLOBAL_VARS.md` (Inside Macintosh
Appendix D). Approximately 160 entries.

### Snapshot Model (Change Tracking)

```c
typedef struct {
    uint32_t values[2048];  /* snapshot of low-memory area, packed */
    bool     valid;         /* true after first Mark */
} LMSnapshot;
```

When Beatrix clicks **Mark**, the current raw bytes of the low-memory region
($000–$FFF, 4 KB) are copied into `LMSnapshot.values`. On each subsequent
frame, the live value is compared to the snapshot:

- **Changed** values are drawn in red (`ImVec4(1, 0.3, 0.3, 1)`).
- **Unchanged** values are drawn in normal text color.

This is cheap: a single 4 KB `memcpy` on Mark, and per-variable byte comparison
during draw (only for visible rows, thanks to the ImGui clipper).

---

## UI Design

### Window: "Low Memory Globals"

```
┌─ Low Memory Globals ──────────────────────────────────────────────┐
│ Filter: [_______________] Category: [All        ▾]  [Mark] [Clear]│
│──────────────────────────────────────────────────────────────────  │
│  Name             Addr     Type   Value                   Brief   │
│  ───────────────  ──────   ────   ──────────────────────  ─────── │
│  MemTop           $0108    long   $00400000               End of… │
│  BufPtr           $010C    ptr    $0003F800               End of… │
│  HeapEnd          $0114    ptr    $0003A000               End of… │
│  TheZone          $0118    ptr    $00002800               Curren… │
│  ApplLimit        $0130    ptr    $00039800               App he… │
│  SysEvtMask       $0144    word   $FFFF                   System… │
│  Ticks            $016A    long   $000A1B2C  (red)        Tick c… │
│  Time             $020C    long   $B3F1240E  (red)        Second… │
│  CurApName        $0910    str    "Finder"                Name o… │
│  …                                                                │
└───────────────────────────────────────────────────────────────────┘
```

### Controls

| Control | Behavior |
|---------|----------|
| **Filter** text field | Case-insensitive substring match on Name or Brief. Filters the visible rows. |
| **Category** combo | Dropdown with `All` + each `LMCategory` label. Filters by category. |
| **Mark** button | Snapshots current low-memory bytes. After this, changed values turn red. |
| **Clear** button | Clears the snapshot (stops highlighting). |

### Table Columns

| Column | Width | Content |
|--------|-------|---------|
| **Name** | stretch | Symbolic name, e.g. `MemTop`. Sortable. |
| **Addr** | fixed 70px | `$XXXX` hex address. Sortable. |
| **Type** | fixed 50px | Short type label: `byte`, `word`, `long`, `ptr`, `hdl`, `str`, `8b`, `rect`, `pat`, `ostp`. |
| **Value** | stretch | Formatted value (see Formatting below). Red if changed since Mark. |
| **Brief** | stretch | Truncated description from the table. Tooltip shows full text. |

### Value Formatting

| LMType | Display |
|--------|---------|
| `LM_BYTE` | `$XX` |
| `LM_WORD` | `$XXXX` |
| `LM_LONG` | `$XXXXXXXX` |
| `LM_POINTER` | `$XXXXXXXX` |
| `LM_HANDLE` | `$XXXXXXXX (H)` |
| `LM_PSTRING` | `"contents"` (Mac Roman decoded, max 31 chars) |
| `LM_BYTES` | `XX XX XX …` (up to ~16 bytes inline, rest as `…`) |
| `LM_RECT` | `(T,L)-(B,R)` e.g. `(0,0)-(342,512)` |
| `LM_PATTERN` | `XX XX XX XX XX XX XX XX` |
| `LM_OSType` | `'TYPE'` 4-char display |

### Interactions

- **Click address** → opens a Memory tool window scrolled to that address.
- **Click pointer/handle value** → opens a Memory tool window at the pointed-to address.
- **Hover Brief** → full description in tooltip.

---

## Implementation Structure

### New Files

| File | Purpose |
|------|---------|
| `src/platform/lomem_globals.h` | `LMGlobal` struct, `LMType`/`LMCategory` enums, global table declaration, snapshot API |
| `src/platform/lomem_globals.cpp` | The static table of ~160 entries, snapshot read/compare functions |
| `src/platform/imgui_lomem_tool.h` | `LowMemTool` class declaration (inherits `ToolPanel`) |
| `src/platform/imgui_lomem_tool.cpp` | `LowMemTool::draw()` implementation |

### Modified Files

| File | Change |
|------|--------|
| `src/platform/imgui_debug_windows.h` | `#include "imgui_lomem_tool.h"` (or keep separate, just forward-declare) |
| `src/platform/imgui_debug_windows.cpp` | Add `LowMemTool` to `RegisterDebugTools()` |
| `CMakeLists.txt` | Add the two new `.cpp` files to the build |

### Class: `LowMemTool`

```cpp
class LowMemTool : public ToolPanel {
public:
    const char* name() const override { return "Low Memory Globals"; }
    void draw() override;
private:
    char filterBuf_[64] = {};
    int  categoryFilter_ = 0;      /* 0 = All */
    bool snapshotValid_ = false;
    uint8_t snapshot_[4096] = {};   /* $000–$FFF */
};
```

### Data Table (`lomem_globals.cpp`)

A static `constexpr`-friendly array:

```c
static const LMGlobal kLowMemGlobals[] = {
    { "MemTop",       0x0108, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of end of RAM" },
    { "BufPtr",       0x010C, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of end of jump table" },
    { "HeapEnd",      0x0114, 4, LM_POINTER, LM_CAT_SYSTEM,   "End of application heap zone" },
    { "TheZone",      0x0118, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of current heap zone" },
    /* ... ~160 entries ... */
    { "CurApName",    0x0910, 32, LM_PSTRING, LM_CAT_APP,     "Name of current application" },
    /* ... */
};
static const int kLowMemCount = sizeof(kLowMemGlobals) / sizeof(kLowMemGlobals[0]);
```

The table is populated directly from `macdocs/ref/GLOBAL_VARS.md`. Each entry
gets a manually assigned `LMType` and `LMCategory` based on the "Contents"
column description (e.g. "pointer" → `LM_POINTER`, "word" → `LM_WORD`).

### Snapshot API

```cpp
/* Copy 4 KB of guest RAM ($000–$FFF) into snapshot buffer. */
void lomem_snapshot_take(uint8_t *snapshot);

/* Compare: returns true if the value at [addr, addr+size) differs. */
bool lomem_snapshot_changed(const uint8_t *snapshot, uint32_t addr, uint16_t size);

/* Read a value from live guest memory for display. */
uint32_t lomem_read_value(uint32_t addr, uint16_t size);
```

`lomem_snapshot_take` uses direct `g_ram` access (the low-memory region is
always real RAM, never memory-mapped I/O) for a fast `memcpy` of 4 KB.

`lomem_snapshot_changed` does `memcmp(snapshot + addr, g_ram + addr, size)`.

### Draw Loop (Pseudocode)

```
draw():
    if !Begin("Low Memory Globals", &visible): End(); return

    draw filter text input
    draw category combo
    draw Mark / Clear buttons

    if Mark clicked:
        lomem_snapshot_take(snapshot_)
        snapshotValid_ = true
    if Clear clicked:
        snapshotValid_ = false

    Separator()

    BeginTable("lomem", 5, Borders | RowBg | Sortable | ScrollY | SizingStretchProp)
        setup columns: Name, Addr, Type, Value, Brief
        HeadersRow()

        handle sorting (by name alphabetically, by address numerically)

        for each entry in kLowMemGlobals:
            if categoryFilter_ != 0 and entry.category != categoryFilter_-1: skip
            if filterBuf_[0] and entry.name/brief doesn't match: skip

            TableNextRow()

            // Name
            TableNextColumn()
            TextUnformatted(entry.name)

            // Addr
            TableNextColumn()
            Text("$%04X", entry.addr)

            // Type
            TableNextColumn()
            TextUnformatted(type_label(entry.type))

            // Value — red if changed
            TableNextColumn()
            changed = snapshotValid_ && lomem_snapshot_changed(snapshot_, entry.addr, entry.size)
            if changed: PushStyleColor(ImGuiCol_Text, red)
            display formatted value per entry.type
            if changed: PopStyleColor()

            // Brief
            TableNextColumn()
            TextUnformatted(entry.brief)  // truncated naturally by column width
            if IsItemHovered(): SetTooltip(entry.brief)  // full text

    EndTable()
    End()
```

---

## Performance

| Operation | Cost | Notes |
|-----------|------|-------|
| Mark snapshot | 4 KB memcpy | Negligible; runs once on button click |
| Per-frame draw | ~160 × memcmp (2–32 bytes each) | Only for visible rows via ImGui clipper; well under 1 µs total |
| Filter matching | ~160 × strcasestr | Fast; string table fits in L1 cache |

No per-frame allocations. No guest memory writes. No emulation locks needed
for direct `g_ram` reads (the low-memory region doesn't alias I/O on any
supported Mac model).

---

## Edge Cases

| Case | Handling |
|------|----------|
| **g_ram is NULL** (no machine loaded) | Show "No machine loaded" and return early |
| **RAM < 4 KB** | Cannot happen — minimum Mac has 128 KB |
| **Address outside RAM** (some globals are in I/O space for Mac II) | Use `get_vm_byte()` fallback for addresses ≥ RAM size; mark as `??` if unmapped |
| **Variable spans a valid/invalid boundary** | Read byte-by-byte, show `??` for unmapped bytes |
| **Empty filter** | Show all entries (no filtering) |
| **Multiple marks without clear** | Each Mark overwrites the previous snapshot |

---

## Future Extensions (Not in Initial Scope)

- **QuickDraw globals**: QD globals live at negative offsets from `(A5)`, not at
  fixed addresses. Would need to read `CurrentA5`, then display offsets like
  `thePort`, `screenBits`, etc. Different mechanism; separate feature.
- **Auto-refresh rate control**: Let Beatrix choose update frequency (every
  frame, every N frames, paused).
- **Export**: Copy table to clipboard as tab-separated text.
- **Watchlist**: Pin specific globals to a separate small window.
- **Breakpoint on change**: Stop emulation when a specific global changes value.
