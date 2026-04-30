# Model Selector

The model selector is shown when the application launches without a
`--model` argument.  No emulation is running; the user picks a machine
and configures it before booting.

## Model Grid

- 700×500 window, light gray background, centered "Maxi vMac" title.
- 4-column grid of model cards (3 columns if the window is narrow).
- Each card shows:
  - Colored icon square with the model's initial letter.
  - Model name (bold).
  - CPU and RAM (e.g. "68000, 4 MB RAM").
  - Resolution (e.g. "512×342" or "640×480 color").
- Models whose ROM is not found in `roms/` are greyed out (35% opacity)
  and cannot be clicked.
- Clicking an available card opens the Configuration Panel.

## Configuration Panel

Replaces the grid.  A **Back** button returns to the grid.

**Machine tab**

| Control     | Description                                  |
|-------------|----------------------------------------------|
| RAM         | Dropdown of valid sizes for the model        |
| Speed       | 1×, 2×, 4×, 8×, Unlimited                   |
| ROM path    | Read-only display of the resolved ROM file   |

**Disks tab**

- Six drive slots (Drive 1 – Drive 6).
- Each slot has a Browse button (opens a native file dialog).
- Disk images can also be dragged from the host desktop onto a slot.
- Occupied slots show the filename; empty slots show "—".

**Boot button** — green, centered below the tabs.  Starts emulation
and transitions to Windowed state.
