# Dead Code — Platform Files

**Last updated:** 2026-05-31

Platform code is clean. The sole backend is `src/platform/sdl.cpp`. All
non-SDL backends (Cocoa, Carbon, X11, GTK, Win32, DOS, NDS, Classic Mac)
have been removed. `src/unused/` has been deleted.

The `#if 0 != SDL_MAJOR_VERSION` / `#if 0 == SDL_MAJOR_VERSION` blocks in
`sdl.cpp` are **not dead code** — they are SDL version dispatch and should
not be touched.

`src/platform/common/` is clean. All `UseActvCode`, `EnableDemoMsg`, and
`NeedIntlChars` blocks have been removed from `control_mode.cpp`,
`intl_chars.cpp`, and `intl_chars.h`.

---

## Remaining Items

### `src/platform/common/osglu_common.h` / `osglu_common.cpp` — `EmLocalTalk`

`EmLocalTalk = 0` gates LocalTalk platform glue in these files. Keep — part
of the not-yet-enabled LocalTalk feature.

---

### `WantAbnormalReports = 0` — keep

Guards ~80 lines across `machine.h`, `machine.cpp`, `platform.h`,
`osglu_common.cpp`. Useful debug diagnostics. Do not remove.
