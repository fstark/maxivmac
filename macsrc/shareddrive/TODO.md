trace traps GetVolInfo Open Close OpenRF SetVol GetVol PBGetCatInfo PBGetFCBInfo
(dbg) trace traps Open Close Read Write GetVolInfo Create Delete OpenRF Rename GetFileInfo SetFileInfo UnmountVol Allocate GetEOF SetEOF FlushVol GetVol SetVol Eject GetFPos SetFPos FlushFile HOpen HCreate HDelete HOpenRF HRename HGetFileInfo HSetFileInfo HSetVol PBOpenWD PBCloseWD PBCatMove PBDirCreate PBGetWDInfo PBGetFCBInfo PBGetCatInfo PBSetCatInfo PBSetVInfo PBGetVolParms


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
--> I don't care but want a log on the host.


- [ ] Negative ioReqCount → paramErr (-50) — currently returns noErr
  with 0 bytes
--> do it


- [ ] vcbNxtCNID never updated — stale in VCB; PBHGetVInfo is correct
- [ ] vcbFilCnt / vcbDirCnt zero in VCB — tools reading the VCB
  directly (DiskTop etc.) see 0
- [ ] ioVDirCnt hardcoded to 1 in TrapGetVolInfo
- [ ] FreeFCB: zero all 94 bytes instead of just fcbFlNum
  do it

- [ ] fdLocation / fdFldr not round-tripped through Set/GetFileInfo

		/* Unknown HFS selector — only intercept if it targets our volume;
		   pass through to the real File Manager for other volumes. */
		if (IsOurVolume(*(short *)(pb + pb_ioVRefNum))) {
			*(short *)(pb + pb_ioResult) = kParamErr;
			log_trap(g->regBase, selector, pb, LOG_ERROR, kParamErr, LOG_F_HFS);
			result = 0;
		}
