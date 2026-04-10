# VIDEO_PLAN — Screen Conversion Pipeline Rewrite

**STATUS: COMPLETE** — All 5 phases executed and committed.

Implements the architectural changes described in VIDEO.md §11.

## Completed Phases

| Phase | Commit | Summary |
|:---:|---|---|
| 1 | GL filter toggle | `TextureFilter` enum, `setTextureFilter()`, Display tab radio buttons |
| 2 | Flat palette | `clut32[256]` in DisplayState, `BuildPalette()` |
| 3 | Unified converter | `ConvertScreen()` template-on-depth, no rect/bpp params |
| 4 | Dead code removal | Deleted `ScreenMapConvert`, `screen_map.h`, `screen_map_inst.h`, `BuildClutTable`, `ConvertRect`, `ConvertRectSlow`, `CLUT_final`, `scalingBuff` |
| 5 | Bug fixes | B2/B3 already fixed; B4 PRAM[0x48]=0x81 for all colour depths |

## Known remaining issues

* 16 bpp and 32 bpp modes crash on Mac II — pre-existing emulation
  bug unrelated to the screen conversion pipeline.  The converter
  (`ConvertScreenDepth4`, `ConvertScreenDepth5`) produces correct
  output for valid VRAM data; the crash is in the guest OS.
