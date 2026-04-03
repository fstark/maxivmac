# System Global Variables

> Extracted from Inside Macintosh, Appendix D (im059)
> Source: `macdocs/tech_doc/im202.html` lines 61450–61689
>
> Low-memory global variables used by the Macintosh Toolbox and Operating System.

---

| Name | Address | Contents |
|------|---------|----------|
| ABusVars | $2D8 | Pointer to AppleTalk variables |
| ACount | $A9A | Stage number (0–3) of last alert (word) |
| ANumber | $A98 | Resource ID of last alert (word) |
| ApFontID | $984 | Font number of application font (word) |
| ApplLimit | $130 | Application heap limit |
| ApplScratch | $A78 | 12-byte scratch area for applications |
| ApplZone | $2AA | Address of application heap zone |
| AppParmHandle | $AEC | Handle to Finder information |
| AtMenuBottom | $A0C | Flag for menu scrolling (word) |
| AuxWinHead | $CD0 | Auxiliary window list header (long) |
| BootDrive | $210 | Working directory refnum for startup volume (word) |
| BufPtr | $10C | Address of end of jump table |
| BufTgDate | $304 | File tags: date/time of last modification (long) |
| BufTgFBkNum | $302 | File tags: logical block number (word) |
| BufTgFFlg | $300 | File tags: flags (word; bit 1=1 if resource fork) |
| BufTgFNum | $2FC | File tags: file number (long) |
| CaretTime | $2F4 | Caret-blink interval in ticks (long) |
| CPUFlag | $12F | Microprocessor in use (word) |
| CrsrThresh | $8EC | Mouse-scaling threshold (word) |
| CurActivate | $A64 | Pointer to window to receive activate event |
| CurApName | $910 | Name of current application (length byte + up to 31 chars) |
| CurApRefNum | $900 | Refnum of current application's resource file (word) |
| CurDeactive | $A68 | Pointer to window to receive deactivate event |
| CurDirStore | $398 | Directory ID of directory last opened (long) |
| CurJTOffset | $934 | Offset to jump table from A5 (word) |
| CurMap | $A5A | Refnum of current resource file (word) |
| CurPageOption | $936 | Sound/screen buffer config passed to Chain/Launch (word) |
| CurPitch | $280 | Count value in square-wave synthesizer buffer (word) |
| CurrentA5 | $904 | Address of boundary between app globals and app parameters |
| CurStackBase | $908 | Address of base of stack; start of app globals |
| DABeeper | $A9C | Address of current sound procedure |
| DAStrings | $AA0 | Handles to ParamText strings (16 bytes) |
| DefltStack | $322 | Default space allotment for stack (long) |
| DefVCBPtr | $352 | Pointer to default volume control block |
| DeskHook | $A6C | Address of procedure for painting/clicking desktop |
| DeskPattern | $A3C | Pattern for desktop painting (8 bytes) |
| DeviceList | $8A8 | Handle to first element in device list |
| DlgFont | $AFA | Font number for dialogs and alerts (word) |
| DoubleTime | $2F0 | Double-click interval in ticks (long) |
| DragHook | $9F6 | Address of procedure during drag/track operations |
| DragPattern | $A34 | Pattern of dragged region outline (8 bytes) |
| DrvQHdr | $308 | Drive queue header (10 bytes) |
| DSAlertRect | $3F8 | Rectangle enclosing system error alert (8 bytes) |
| DSAlertTab | $2BA | Pointer to system error alert table in use |
| DSErrCode | $AF0 | Current system error ID (word) |
| DTQueue | $D92 | Deferred task queue header (10 bytes) |
| EventQueue | $14A | Event queue header (10 bytes) |
| ExtStsDT | $2BE | External/status interrupt vector table (16 bytes) |
| FCBSPtr | $34E | Pointer to file-control-block buffer |
| FinderName | $2E0 | Name of Finder (length byte + up to 15 chars) |
| FractEnable | $BF4 | Nonzero to enable fractional widths (byte) |
| FScaleDisable | $A63 | Nonzero to disable font scaling (byte) |
| FSFCBLen | $3F6 | Size of file control block; −1 on 64K ROM (word) |
| FSQHdr | $360 | File I/O queue header (10 bytes) |
| GhostWindow | $A84 | Pointer to window never considered frontmost |
| GrayRgn | $9EE | Handle to region drawn as desktop |
| GZRootHnd | $328 | Handle to relocatable block not moved by grow zone |
| HeapEnd | $114 | Address of end of application heap zone |
| HiliteMode | $938 | Set if highlighting is on |
| HiliteRGB | $DA0 | Default highlight color for system |
| IntlSpec | $BA0 | International software installed if ≠ −1 (long) |
| JADBProc | $6B8 | Pointer to ADBReInit pre/post-processing routine |
| JDTInstall | $D9C | Jump vector for DTInstall routine |
| JFetch | $8F4 | Jump vector for Fetch function |
| JIODone | $8FC | Jump vector for IODone function |
| JournalFlag | $8DE | Journaling mode (word) |
| JournalRef | $8E8 | Refnum of journaling device driver (word) |
| JStash | $8F8 | Jump vector for Stash function |
| JVBLTask | $D28 | Jump vector for DoVBLTask routine |
| KbdLast | $218 | ADB address of keyboard last used (byte) |
| KbdType | $21E | Keyboard type of keyboard last used (byte) |
| KeyRepThresh | $190 | Auto-key rate (word) |
| KeyThresh | $18E | Auto-key threshold (word) |
| LastFOND | $BC2 | Handle to last family record used |
| Lo3Bytes | $31A | $00FFFFFF |
| Lvl1DT | $192 | Level-1 secondary interrupt vector table (32 bytes) |
| Lvl2DT | $1B2 | Level-2 secondary interrupt vector table (32 bytes) |
| MainDevice | $8A4 | Handle to current main device |
| MBarEnable | $A20 | Unique menu ID for active desk accessory (word) |
| MBarHeight | $BAA | Height of menu bar (word) |
| MBarHook | $A2C | Address of routine called by MenuSelect before draw |
| MemErr | $220 | Current value of MemError (word) |
| MemTop | $108 | Address of end of RAM |
| MenuCInfo | $D50 | Header for menu color information table |
| MenuDisable | $B54 | Menu ID and item for selected disabled item |
| MenuFlash | $A24 | Count for menu item blink duration (word) |
| MenuHook | $A30 | Address of routine called during MenuSelect |
| MenuList | $A1C | Handle to current menu list |
| MinStack | $31E | Minimum space allotment for stack (long) |
| MinusOne | $A06 | $FFFFFFFF |
| MMU32Bit | $CB2 | Current address mode (byte) |
| OldContent | $9EA | Handle to saved content region |
| OldStructure | $9E6 | Handle to saved structure region |
| OneOne | $A02 | $00010001 |
| PaintWhite | $9DC | Flag: paint window white before update event (word) |
| PortBUse | $291 | Current availability of serial port B (byte) |
| PrintErr | $944 | Result code from last Printing Manager routine (word) |
| QDColors | $8B0 | Default QuickDraw colors |
| RAMBase | $2B2 | Trap dispatch table base address for RAM routines |
| ResErr | $A60 | Current value of ResError (word) |
| ResErrProc | $AF2 | Address of resource error procedure |
| ResLoad | $A5E | Current SetResLoad state (word) |
| ResumeProc | $A8C | Address of resume procedure |
| RndSeed | $156 | Random number seed (long) |
| ROM85 | $28E | Version number of ROM (word) |
| ROMBase | $2AE | Base address of ROM |
| ROMFont0 | $980 | Handle to font record for system font |
| RomMapInsert | $B9E | Flag: insert map to ROM resources (byte) |
| SaveUpdate | $9DA | Flag: generate update events (word) |
| SaveVisRgn | $9F2 | Handle to saved visRgn |
| SCCRd | $1D8 | SCC read base address |
| SCCWr | $1DC | SCC write base address |
| ScrapCount | $968 | Count changed by ZeroScrap (word) |
| ScrapHandle | $964 | Handle to desk scrap in memory |
| ScrapName | $96C | Pointer to scrap file name (length byte preceded) |
| ScrapSize | $960 | Size in bytes of desk scrap (long) |
| ScrapState | $96A | Where desk scrap is (word) |
| Scratch8 | $9FA | 8-byte scratch area |
| Scratch20 | $1E4 | 20-byte scratch area |
| ScrDmpEnb | $2F8 | 0 if GetNextEvent ignores Cmd-Shift-number (byte) |
| ScrHRes | $104 | Pixels per inch horizontally (word) |
| ScrnBase | $824 | Address of main screen buffer |
| ScrVRes | $102 | Pixels per inch vertically (word) |
| SdVolume | $260 | Current speaker volume (byte; low 3 bits only) |
| SEvtEnb | $15C | 0 if SystemEvent returns FALSE (byte) |
| SFSaveDisk | $214 | Negative of volume refnum, used by Standard File (word) |
| SoundBase | $266 | Pointer to free-form synthesizer buffer |
| SoundLevel | $27F | Amplitude in 740-byte buffer (byte) |
| SoundPtr | $262 | Pointer to four-tone record |
| SPAlarm | $200 | Alarm setting (long) |
| SPATalkA | $1F9 | AppleTalk node ID hint for modem port (byte) |
| SPATalkB | $1FA | AppleTalk node ID hint for printer port (byte) |
| SPClikCaret | $209 | Double-click and caret-blink times (byte) |
| SPConfig | $1FB | Use types for serial ports (byte) |
| SPFont | $204 | Application font number minus 1 (word) |
| SPKbd | $206 | Auto-key threshold and rate (byte) |
| SPMisc2 | $20B | Mouse scaling, startup disk, menu blink (byte) |
| SPPortA | $1FC | Modem port configuration (word) |
| SPPortB | $1FE | Printer port configuration (word) |
| SPPrint | $207 | Printer connection (byte) |
| SPValid | $1F8 | Validity status (byte) |
| SPVolCtl | $208 | Speaker volume in parameter RAM (byte) |
| SynListHandle | $D32 | Handle to synthetic font list |
| SysEvtMask | $144 | System event mask (word) |
| SysFontFam | $BA6 | If nonzero, font number for system font (word) |
| SysFontSize | $BA8 | If nonzero, size of system font (word) |
| SysMap | $A58 | Refnum of system resource file (word) |
| SysMapHndl | $A54 | Handle to map of system resource file |
| SysParam | $1F8 | Low-memory copy of parameter RAM (20 bytes) |
| SysResName | $AD8 | Name of system resource file (length byte + up to 19 chars) |
| SysZone | $2A6 | Address of system heap zone |
| TEDoText | $A70 | Address of TextEdit multi-purpose routine |
| TERecal | $A74 | Address of routine to recalculate TE line starts |
| TEScrpHandle | $AB4 | Handle to TextEdit scrap |
| TEScrpLength | $AB0 | Size in bytes of TextEdit scrap (long) |
| TheGDevice | $CC8 | Handle to current active device (long) |
| TheMenu | $A26 | Menu ID of currently highlighted menu (word) |
| TheZone | $118 | Address of current heap zone |
| Ticks | $16A | Current ticks since system startup (long) |
| Time | $20C | Seconds since midnight, January 1, 1904 (long) |
| TimeDBRA | $D00 | DBRA instructions per millisecond (word) |
| TimeSCCDB | $D02 | SCC accesses per millisecond (word) |
| TimeSCSIDB | $DA6 | SCSI accesses per millisecond (word) |
| TmpResLoad | $B9F | Temporary SetResLoad state for ROMMapInsert (byte) |
| ToExtFS | $3F2 | Pointer to external file system |
| ToolScratch | $9CE | 8-byte scratch area |
| TopMapHndl | $A50 | Handle to resource map of most recently opened file |
| TopMenuItem | $A0A | Pixel value of top of scrollable menu |
| UTableBase | $11C | Base address of unit table |
| VBLQueue | $160 | Vertical retrace queue header (10 bytes) |
| VCBQHdr | $356 | Volume-control-block queue header (10 bytes) |
| VIA | $1DA | VIA base address |
| WidthListHand | $8E4 | Handle to list of handles to width tables |
| WidthPtr | $B10 | Pointer to global width table |
| WidthTabHandle | $B2A | Handle to global width table |
| WindowList | $9D6 | Pointer to first window in window list; 0 if none |
| WMgrPort | $9DE | Pointer to Window Manager port |

---

## Key Variables for Emulator Development

### Hardware I/O

| Name | Address | Used By |
|------|---------|---------|
| VIA | $1DA | VIA base address — all VIA device access |
| SCCRd | $1D8 | SCC read base — serial port reads |
| SCCWr | $1DC | SCC write base — serial port writes |
| SoundBase | $266 | Sound buffer pointer — sound generation |
| ScrnBase | $824 | Screen buffer pointer — video display |
| ROMBase | $2AE | ROM base — ROM overlay, trap dispatch |
| MemTop | $108 | End of RAM — memory sizing |

### Timing

| Name | Address | Used By |
|------|---------|---------|
| Ticks | $16A | VBL counter, event timing |
| Time | $20C | Real-time clock seconds |
| TimeDBRA | $D00 | CPU speed calibration |
| TimeSCCDB | $D02 | SCC timing calibration |
| TimeSCSIDB | $DA6 | SCSI timing calibration |

### Interrupt Vectors

| Name | Address | Used By |
|------|---------|---------|
| Lvl1DT | $192 | VIA interrupt dispatch (32 bytes) |
| Lvl2DT | $1B2 | SCC interrupt dispatch (32 bytes) |
| ExtStsDT | $2BE | External/status interrupts (16 bytes) |
| JVBLTask | $D28 | VBL task dispatch |
| JDTInstall | $D9C | Deferred task install |
| JFetch | $8F4 | Serial input |
| JStash | $8F8 | Serial output |
| JIODone | $8FC | I/O completion |

### Keyboard and Mouse

| Name | Address | Used By |
|------|---------|---------|
| KbdLast | $218 | ADB keyboard address |
| KbdType | $21E | Keyboard type |
| KeyThresh | $18E | Auto-key threshold |
| KeyRepThresh | $190 | Auto-key repeat rate |
| CrsrThresh | $8EC | Mouse scaling threshold |
