# SharedDrive INIT — TODO

Source: macsrc/shareddrive/init.c

## Do

- [ ] **TrapRead: return eofErr** — apps looping on eofErr to detect
  end-of-file will spin forever (0 bytes, noErr at EOF)
- [ ] **TrapRead: return posErr** — negative mark silently clamped to 0
- [ ] **TrapSetFPos: return eofErr / posErr** — same silent clamping
- [ ] **TrapWrite: return posErr** — negative mark silently clamped to 0
- [ ] **Bad-refnum error codes** — seven handlers return fnfErr (-43)
  instead of rfNumErr (-51); one-line fix each
- [ ] **Open permissions (ioPermssn)** — every open hard-codes write
  access; need to read ioPermssn, set fcbFlags accordingly, and
  return wrPermErr on write-to-read-only FCB
- [ ] **Open conflict detection (opWrErr)** — multiple opens of the
  same file all succeed; IM requires opWrErr (-49) when exclusive
  read/write conflicts exist

## Could do

- [ ] Newline mode (bit 7 of ioPosMode) — not implemented; rare in
  practice but needed for line-by-line text reads
- [ ] Negative ioReqCount → paramErr (-50) — currently returns noErr
  with 0 bytes
- [ ] vcbNxtCNID never updated — stale in VCB; PBHGetVInfo is correct
- [ ] vcbFilCnt / vcbDirCnt zero in VCB — tools reading the VCB
  directly (DiskTop etc.) see 0
- [ ] ioVDirCnt hardcoded to 1 in TrapGetVolInfo
- [ ] FreeFCB: zero all 94 bytes instead of just fcbFlNum
- [ ] fdLocation / fdFldr not round-tripped through Set/GetFileInfo
