---
name: compress-video
description: "Compress a screen recording or UI demo video for documentation. Use when: shrinking a .mov or .mp4 for README/docs, compressing a screen capture, encoding a demo video."
argument-hint: "Path to input video file"
---

# Compress UI Video

Compress a screen recording for use in docs or README.

## Command

```sh
ffmpeg -i input.mov -an -c:v libx264 -preset slow -tune animation -crf 32 -pix_fmt yuv420p output.mp4
```

## Flags

| Flag | Purpose |
|------|---------|
| `-an` | Strip audio |
| `-c:v libx264` | H.264 codec (universal playback) |
| `-preset slow` | Better compression (worth the wait) |
| `-tune animation` | Optimised for flat colors / UI / screen content |
| `-crf 32` | Quality — higher = smaller. 28–34 is fine for UI demos |
| `-pix_fmt yuv420p` | Compatibility with all players/browsers |

## Usage

Run the command with the input file path provided by the user. Output file goes next to the input with `.mp4` extension.
