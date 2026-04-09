# Mouse Handling

## Principles

The emulator guest is a complete Macintosh that draws its own cursor.
The host also has a cursor.  The goal is to make the two feel like one:
when the user is interacting with the guest, only the guest cursor
should be visible; when interacting with host UI, only the host cursor
should be visible.

Host-to-guest coordinate mapping accounts for scale and offset so the
guest always receives positions in its native resolution, regardless of
how the image is displayed.

Mouse position and mouse clicks are treated as separate concerns.
Position updates are informational — they let the guest track the
pointer.  Clicks are commitments — they should only reach the guest
when the user intends to interact with it.

When the emulator is backgrounded or speed-stopped, the host cursor is
always shown and no mouse input reaches the guest.

## Coordinate spaces

Three coordinate spaces are involved:

1. **Host window** — the SDL window, in logical pixels.
2. **Emulator viewport** — the region of the window occupied by the
   guest image, potentially scaled and offset (centered with black
   borders in fullscreen, or at a fixed origin in developer mode).
3. **Guest screen** — the Macintosh framebuffer in native pixels
   (e.g. 512×342 for a Mac Plus).

All mouse positions from the host are mapped through the viewport
(subtract origin, divide by scale) before reaching the guest.
Positions that fall outside the guest screen are clamped to its edges.

## Three display modes

### Windowed

The guest image is displayed at an integer scale (1× or 2×) in a
window sized to match (e.g. 1024×684 for a Mac Plus at 2×).  The
viewport fills the window, so the coordinate mapping is a simple
division by the scale factor.

The host cursor is hidden whenever the pointer is inside the window.
On the guest screen only the guest-drawn cursor is visible.  When the
pointer leaves the window, the host cursor reappears.

Clicks and keyboard input always reach the guest (there is no
competing host UI apart from the Ctrl overlay).

### Fullscreen

The guest image is integer-scaled and centered on the display.  The
remaining area is filled with black borders.  Because the display is
exclusive, there is no meaningful distinction between "on the guest"
and "on the border" — the user has no other UI to interact with.

The host cursor is therefore always hidden, even over the borders.
The mouse operates in relative (grabbed) mode: raw deltas are applied
to the guest position and the physical cursor is confined to the
display.  Positions on the border are clamped to the nearest guest
edge.

### Developer

The guest image is displayed at native resolution inside a resizable
ImGui window.  Other ImGui windows (debug tools, menus) may overlap
the guest viewport.

Cursor visibility depends on what is topmost under the pointer:

- **Guest viewport topmost** — the host cursor is hidden and the
  guest receives both position and click events, exactly as in
  windowed mode.
- **ImGui window on top of the guest** — the host cursor is shown
  (the user is interacting with host UI).  The guest still receives
  position updates so its cursor tracks the host pointer, but clicks
  go to the ImGui window, not the guest.
- **Pointer outside the guest area entirely** — the guest receives
  no updates; its cursor stays where it was last seen.

This means the guest cursor follows the host pointer whenever the
pointer is geometrically over the guest image, regardless of which
layer is on top, but the guest only reacts to clicks when nothing
is covering it.

## Control overlay

Pressing Ctrl toggles the control overlay in any mode.  While the
overlay is visible, the host cursor is forced visible and no mouse
events are forwarded to the guest.  Dismissing the overlay (Escape or
Ctrl again) restores normal handling for the current mode.
