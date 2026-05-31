# Launcher

The Launcher is displayed when the application starts with no `--model` argument and no
`.mac` file path.  No emulation is running.  The user picks a pre-configured machine from
the available `.mac` files to boot.

---

## Window

- Fixed 700×500 px, not resizable.
- Light gray background (rgb 0.78, 0.78, 0.78 — roughly 50% gray, evoking Mac boot screen).
- Centered "maxivmac" title in black, with a separator rule below.
- Version string in the bottom-right corner (gray text).

---

## Card Grid

`.mac` files are loaded from `data/macs/` at startup.

- 4-column grid (falls back to 3 columns if the available width per card drops below 120 px).
- Cards are 135 px tall, 16 px gap between cards, centered in the window.
- Each card shows:
  - A 64×64 px icon: PNG loaded from the `iconPath` field in the `.mac` file, or a
    colored square with the machine's first letter if no icon is available.
  - Machine name below the icon.

**Valid card** (ROM and all disk images present and found on disk):
- Full opacity.
- White highlight overlay on hover.
- Single click → boot immediately.

**Invalid card** (ROM or a required disk image missing):
- 35% opacity.
- On hover: tooltip showing the validation error string from the `.mac` file.
- Not clickable.

**Info button (ⓘ):** appears in the top-right corner of a card on hover.  Opens the info
popup for that machine.

**Empty state:** if `data/macs/` contains no `.mac` files (or the directory does not
exist), the card area shows:
> No .mac files found in data/macs/

---

## Info Popup

- Opened by clicking the ⓘ button on any card.
- Shows machine details from the `.mac` file.
- Dismissed by clicking outside the popup.

---

## Boot Sequence

1. User clicks a valid card.
2. The launcher window, GL context, and ImGui context are destroyed.
3. `initMachine()` creates the emulator window at 2× the guest resolution (or 1× if 2× does
   not fit the display), loads the ROM and disk images, and starts the emulator.
4. UI transitions to Windowed state.  See [features/UI.md](../features/UI.md).

---

## No In-Launcher Configuration

There is no per-machine configuration UI.  RAM size, disk images, ROM path, and initial
speed are all specified in the `.mac` file.  To change a configuration, edit the `.mac`
file directly.

---

## Bypassing the Launcher

The launcher is skipped in two cases:

| Invocation | Behaviour |
|------------|-----------|
| `maxivmac path/to/file.mac` | Parsed and validated; boots directly to Windowed state |
| `maxivmac --model <name>` | Legacy CLI flag; uses global config; boots directly to Windowed state |
