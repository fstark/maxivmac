# Media Assets Needed for README

All assets below are referenced from `README.md` at the repo root using relative paths
(`media/filename`).  Record the actual filename here once it is created.

---

## Logo / Banner

| File | Description | Status |
|------|-------------|--------|
| `logo.png` | Full color logo — compact Mac silhouette with M· mark, ideally 600 × 200 px or similar wide format suitable for a GitHub README hero banner. See [docs/features/BRANDING.md](../docs/features/BRANDING.md) for design spec. | **NEEDED** |
| `icon.png` | 128×128 app icon (color version). Used in feature list and for GitHub social preview. | **NEEDED** |

---

## Demo GIFs / MP4 (10 seconds each, ~600 px wide, looping)

These are the hero differentiators listed in [docs/roadmap/ROADMAP.md](../docs/roadmap/ROADMAP.md).

| File | What to Show | Notes | Status |
|------|--------------|-------|--------|
| `demo-launcher.gif` | The launcher card grid — hovering a card, clicking it, Mac booting. | Start from cold launch with no args. | **NEEDED** |
| `demo-clipboard.gif` | Bidirectional clipboard in action — type text on the host, paste into MacWrite on the guest; copy from the guest, paste in a host terminal. | Show both directions in one 10-s clip. | **NEEDED** |
| `demo-shared-drive.gif` | Drag a host folder onto the emulator window, watch it appear as an HFS volume on the Mac desktop, open a file from it. | Shows drag-drop mount. | **NEEDED** |
| `demo-models.gif` | Launcher showing multiple machine cards (Plus, Mac II), clicking to boot each in turn. | Shows all-models-in-one-binary pitch. | **NEEDED** |
| `demo-macii-resolution.gif` | Switch color depth / resolution in Monitors control panel on Mac II; host window resizes in real time. | 8-s clip, keep it snappy. | **NEEDED** |
| `demo-slip.gif` | Netscape 2.0 on the emulated Mac loading a retro web page over the host's internet via SLIP. | External dependency: blog must serve early-HTML pages. | **NEEDED** |
| `demo-debugger.gif` | Set a breakpoint on a Toolbox trap, run, hit the breakpoint, step through a few instructions, inspect registers. | Use the `--debugger` flag startup. | **NEEDED** |
| `demo-crt.gif` | Toggle the CRT post-process effect on/off via the overlay — scanlines, barrel distortion, corner vignette appear and disappear. | Short 5-s loop is fine. | **NEEDED** |

---

## Static Screenshots (fallback if GIFs are too heavy)

| File | Description | Status |
|------|-------------|--------|
| `screenshot-launcher.png` | Launcher with Plus and Mac II cards visible. | **NEEDED** |
| `screenshot-macplus.png` | Mac Plus running System 6.0.8 desktop. | **NEEDED** |
| `screenshot-macii-color.png` | Mac II desktop in 8-bit color. | **NEEDED** |
| `screenshot-debugger.png` | Debugger prompt with a breakpoint and register dump. | **NEEDED** |
| `screenshot-crt.png` | CRT effect enabled on Mac Plus — visible scanlines and barrel curve. | **NEEDED** |

---

## Recording Tips

- Record on a Retina / HiDPI display at 2× scale, then export at 1× so pixels are crisp.
- GIFs: use `ffmpeg` to convert screen recordings, then optimise with `gifsicle`.
- MP4: H.264, CRF 23, web-optimised (`faststart`). GitHub renders MP4 inline in READMEs.
- Preferred format for GitHub README: MP4 (GitHub supports `<video>` in Markdown since 2022) or animated GIF ≤ 10 MB.
- Compress final videos with the `compress-video` skill if needed.
