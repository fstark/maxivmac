# Branding

## Name

Always lowercase: **maxivmac**. No capitalization variants, no "Maxi vMac",
no spaces. It is the repo name, the binary name, and the only way to write it.

## Logo

A chunky pixel-art **M** followed by a **red square** — rendered inside a
compact Mac silhouette. Two separate things that share a letter:

- **Name** — the thing you type: `maxivmac`
- **Logo** — the thing you see: M followed by a red square

They can appear side by side (e.g. an about panel) but neither depends on the
other.

### The M

A custom-drawn sans-serif M with ~2px strokes, designed at 32×32 inside the
Mac's screen area. It is a logo, not a font glyph. It sits at the bottom of
the screen with 1px white margin, vertically anchored.

### The Red Square

- 3×3 pixels at 32×32 scale
- Apple red (#FF3B30)
- Always a square, at every resolution — never a circle
- Positioned to the right of the M, baseline-aligned
- In the 1-bit version, it is a black square

Design principle: Turner's *Helvoetsluys* — a single point of saturated color
in a monochrome composition.

## Versions

### 1-bit (canonical)

- Mac silhouette from System 6.0.8 `ICN# 3` (Susan Kare's original)
- White screen, black M, black square
- Used for: `ICN#`, INIT parade, favicon

### Color

- Mac body from System 7's color icon (same geometry as System 6)
- Platinum gray body with shading
- Lavender-blue screen
- Near-black M (flat, not dithered)
- Single red square (#FF3B30) — the only saturated element
- No Apple logo on body
- Used for: `icl4`/`icl8`, `.icns`, GitHub social preview

## Derived Icons

### INIT Icon

Generic INIT silhouette with M· (M + square) inside. Used for ClipSync and
any future maxivmac INITs bundled into the guest system.

### Shared Drive Icon

Standard drive silhouette with a small M· badge in the corner. The drive
shape communicates "disk"; the badge communicates "maxivmac." Subtle,
not dominant.

## GitHub Social Preview

1280×640 PNG, uploaded via repo Settings → Social preview.

- Dark gray background (~#222222)
- Color icon scaled up with nearest-neighbor (hard pixel edges)
- 1px grid lines between pixels (ResEdit style)
- "maxivmac" in bitmap Chicago font, same pixel grid scale as the icon
- Text color: light gray (~#AAAAAA)
- Layout: icon left-of-center, text to the right
- Chicago size should match the icon's pixel grid (12pt = ~10px cap height)
- Keep the preview image in `assets/social-preview.png` for version control
