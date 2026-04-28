# Manual Test Checklist

Regression checklist for user-facing features.  
Run on **Mac II** model with the **608.hfs** boot disk (reference system).

```
Launch command:
  ./bld/macos/maxivmac --model MacII --drive=shared 608.hfs
```

Place test files in `shared/` before launching. Suggested contents:
- `hello.txt` — a short text file (a few lines, LF line endings)
- `big.txt` — a text file >4 KB
- `photo.jpg` — a JPEG image (to verify type/creator mapping)

---

## Session 1 — Boot & Shared Drive

**Goal:** Verify that the system boots, shared volume mounts, and basic file operations work.

### 1.1 Boot
- [ ] System boots to Finder desktop without errors
- [ ] SharedDrive volume icon appears on desktop

### 1.2 Shared volume — file listing
- [ ] Open the SharedDrive volume — files from `shared/` are listed
- [ ] `hello.txt` shows correct type (`TEXT`) and creator
- [ ] `photo.jpg` shows an appropriate type/creator (e.g. `JPEG`/`JVWR`)

### 1.3 Shared volume — reading files
- [ ] Double-click `hello.txt` — opens in TeachText/SimpleText, content is correct
- [ ] Line endings display correctly (no extra blank lines, no missing breaks)

### 1.4 Shared volume — copy to local disk
- [ ] Drag `hello.txt` from SharedDrive to the 608 boot disk
- [ ] File copies without error
- [ ] Open the copied file — content matches the original

### 1.5 Shared volume — folders
- [ ] If `shared/` contains a subfolder with files, the folder appears correctly
- [ ] Files inside the subfolder are accessible (not flattened to root)

### 1.6 Shared volume — rename file
- [ ] Select `hello.txt` on the SharedDrive, click the name to rename it
- [ ] Type a new name (e.g. "renamed.txt"), press Return
- [ ] File shows the new name in Finder
- [ ] Reopen the SharedDrive window — new name persists
- [ ] On the **host**, check `shared/` — file is renamed on disk

### 1.7 Shared volume — change type/creator (via ResEdit or Get Info)
- [ ] Select a file on SharedDrive, use Get Info (Cmd-I) or ResEdit
      to change the Type and/or Creator code
- [ ] Close and reopen the SharedDrive window — new type/creator sticks
- [ ] On the **host**, check that the AppleDouble sidecar (._file) was
      updated (if applicable)

### 1.8 Shared volume — Finder icon positions
- [ ] Open the SharedDrive window, rearrange icons by dragging them
- [ ] Close and reopen the SharedDrive window — icons stay where placed
- [ ] Reboot the guest (control overlay → Reboot) and reopen the
      SharedDrive — icon positions survive reboot

### 1.9 Shared volume — folder view type
- [ ] Open the SharedDrive window in icon view (default)
- [ ] Switch to list view (View → by Name)
- [ ] Close and reopen the window — list view is retained
- [ ] Switch back to icon view, close and reopen — icon view is retained

### 1.10 Shared volume — create folder
- [ ] In the SharedDrive window, File → New Folder
- [ ] Name the folder, press Return — folder appears
- [ ] Drag a file into the new folder — file moves in
- [ ] On the **host**, verify the folder and file exist in `shared/`

### 1.11 Shared volume — delete file
- [ ] Drag a file from SharedDrive to Trash
- [ ] Empty Trash — no errors
- [ ] File is gone from SharedDrive and from host `shared/` directory

---

## Session 2 — Clipboard (same launch)

**Goal:** Test bidirectional clipboard between host and guest.  
Keep the same session running from Session 1.

### 2.1 Host → Guest (via ClipSync INIT)
- [ ] On the **host**, copy a short string to the clipboard (e.g. "hello from host")
- [ ] In the **guest**, open TeachText, Cmd-V — the host string appears
- [ ] Characters with Mac Roman equivalents (é, ü, ©) survive round-trip

### 2.2 Guest → Host
- [ ] In TeachText, type some text, select it, Cmd-C
- [ ] On the **host**, paste in a text editor — the guest text appears

### 2.3 Large clipboard
- [ ] On the host, copy a block of text >4 KB
- [ ] Paste in guest — text matches, no truncation or corruption

### 2.4 Empty clipboard
- [ ] On the host, copy an empty selection (or clear the clipboard)
- [ ] Guest paste should produce nothing (no crash, no stale data)

### 2.5 Private-scrap apps
- [ ] If a private-scrap app (e.g. ResEdit) is open, clipboard sync may
      only update after switching to Finder and back — verify this works
      rather than silently failing

---

## Session 3 — Disk Management (same launch)

**Goal:** Test inserting and ejecting disk images at runtime.

### 3.1 Insert a second disk
- [ ] Open Control overlay (Ctrl key)
- [ ] Click "Insert Disk", select `mf2.hfs` or another disk image
- [ ] New volume icon appears on the Finder desktop

### 3.2 Access the second disk
- [ ] Open the newly mounted volume — files are listed
- [ ] Open/copy a file from the second volume — works correctly

### 3.3 Eject
- [ ] Drag the second volume to Trash (or Cmd-E) — volume ejects cleanly
- [ ] Volume icon disappears from desktop
- [ ] No error dialogs, guest continues running

### 3.4 Eject All
- [ ] Insert a second disk again
- [ ] Open Control overlay → "Eject All"
- [ ] All secondary volumes eject (boot disk remains)

---

## Session 4 — Display & Speed (same launch)

**Goal:** Verify display controls and speed settings.

### 4.1 Zoom
- [ ] Open Control overlay → Display tab → toggle between 1x and 2x
- [ ] Display scales correctly, no visual artifacts
- [ ] Mouse tracking stays accurate after zoom change

### 4.2 Texture filter
- [ ] Toggle between Nearest and Linear filtering
- [ ] Nearest: pixels are sharp/blocky; Linear: pixels are smoothed

### 4.3 Fullscreen
- [ ] Toggle fullscreen from the overlay
- [ ] Display fills the screen properly
- [ ] Toggle back to windowed — window restores to previous size

### 4.4 Speed
- [ ] Set speed to 8x — guest visibly speeds up
- [ ] Set speed back to 1x — guest returns to normal pace
- [ ] Check "Stopped" — guest freezes (no screen updates, no input)
- [ ] Uncheck "Stopped" — guest resumes

---

## Session 5 — Clipboard edge cases & file export (same launch)

### 5.1 Copy from guest, modify on host, paste back in guest
- [ ] In TeachText, type "AAA", Cmd-A, Cmd-C
- [ ] On host, verify "AAA" in clipboard
- [ ] On host, copy "BBB"
- [ ] In guest, Cmd-V — "BBB" appears (not "AAA")

### 5.2 File export
- [ ] If an export mechanism is available, export a guest file to host
- [ ] Verify the file appears on the host filesystem
- [ ] Content and metadata (as applicable) are preserved

### 5.3 File import
- [ ] If an import mechanism is available, import a host file into guest
- [ ] File appears on a guest volume
- [ ] Content is readable in a guest app

---

## Quick smoke-test (single launch, ~3 minutes)

Minimal check after a build, before deeper testing:

1. [ ] Boot with `--model MacII --drive=shared 608.hfs` — reaches Finder
2. [ ] SharedDrive icon visible, can open and list files
3. [ ] Copy text on host → paste in guest TeachText → text matches
4. [ ] Copy text in guest → paste on host → text matches
5. [ ] Insert second disk via overlay → icon appears
6. [ ] Eject second disk → icon gone, no error
7. [ ] Toggle 1x/2x zoom — display OK

---

## Notes

- All tests use **Mac II** and **608.hfs** as the reference configuration.
- Sessions 1–5 are designed to run sequentially in a **single app launch**
  (no need to quit and relaunch between sessions).
- Known limitation: private-scrap apps need a Finder switch for clipboard
  sync — this is expected behavior, not a bug.
- If control mode (Ctrl key) is hard to activate, note it — this is a
  known UI defect.
