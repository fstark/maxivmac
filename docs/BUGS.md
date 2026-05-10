# Known Bugs

- **Clipboard: redundant SDL publish on identical content.** When the
  guest exports clipboard content that matches what the host pasteboard
  already holds (e.g. MacPaint bumps ScrapCount on quit without changing
  the scrap), `ClipCommit` still calls `SDL_SetClipboardText()` /
  `SDL_SetClipboardData()`. Harmless — the SDL event handler suppresses
  the feedback — but wastes an IPC round-trip. Fix: compare staged
  content against `pb.text`/`pb.png` before publishing.
