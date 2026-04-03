# System Traps

> Extracted from Inside Macintosh, Appendix C (im058)
> Source: `macdocs/tech_doc/im202.html` lines 59557–61449
>
> Trap macros for Toolbox and Operating System routines with trap word values.
> The "Name" column gives the trap macro name (without its initial underscore).
> Where the Pascal call name differs, it appears indented below.
> Package routines list the routine selector in parentheses.
>
> Use `GetTrapAddress`/`SetTrapAddress` with the **trap number** (last two digits
> of trap word; or `1` + last two digits if word starts with `A9`), not the trap word.

---

## Alphabetical by Name

| Name | Trap Word |
|------|-----------|
| ADBOp | A07C |
| ADBReInit | A07B |
| ActivatePalette | AA94 |
| AddComp | AA3B |
| AddDrive | A04E |
| AddPt | A87E |
| AddResMenu | A94D |
| AddResource | A9AB |
| AddSearch | AA3A |
| Alert | A985 |
| AllocCursor | AA1D |
| Allocate (PBAllocate) | A010 |
| AngleFromSlope | A8C4 |
| AnimateEntry | AA99 |
| AnimatePalette | AA9A |
| AppendMenu | A933 |
| AttachVBL | A071 |
| BackColor | A863 |
| BackPat | A87C |
| BackPixPat | AA0B |
| BeginUpdate | A922 |
| BitAnd | A858 |
| BitClr | A85F |
| BitNot | A85A |
| BitOr | A85B |
| BitSet | A85E |
| BitShift | A85C |
| BitTst | A85D |
| BitXor | A859 |
| BlockMove | A02E |
| BringToFront | A920 |
| Button | A974 |
| CTab2Palette | AA9F |
| CalcCMask | AA4F |
| CalcMask | A838 |
| CalcMenuSize | A948 |
| CalcVBehind (CalcVisBehind) | A90A |
| CalcVis | A909 |
| CautionAlert | A988 |
| Chain | A9F3 |
| ChangedResource | A9AA |
| CharExtra | AA23 |
| CharWidth | A88D |
| CheckItem | A945 |
| CheckUpdate | A911 |
| ClearMenuBar | A934 |
| ClipAbove | A90B |
| ClipRect | A87B |
| Close (PBClose) | A001 |
| CloseCPort | A87D |
| CloseDeskAcc | A9B7 |
| CloseDialog | A982 |
| ClosePgon (ClosePoly) | A8CC |
| ClosePicture | A8F4 |
| ClosePort | A87D |
| CloseResFile | A99A |
| CloseRgn | A8DB |
| CloseWindow | A92D |
| CmpString (EqualString) | A03C |
| Color2Index | AA33 |
| ColorBit | A864 |
| CompactMem | A04C |
| Control (PBControl) | A004 |
| CopyBits | A8EC |
| CopyMask | A817 |
| CopyPixMap | AA05 |
| CopyPixPat | AA09 |
| CopyRgn | A8DC |
| CouldAlert | A989 |
| CouldDialog | A979 |
| Count1Resources | A80D |
| Count1Types | A81C |
| CountADBs | A077 |
| CountMItems | A950 |
| CountResources | A99C |
| CountTypes | A99E |
| Create (PBCreate) | A008 |
| CreateResFile | A9B1 |
| CurResFile | A994 |
| DTInstall | A082 |
| Date2Secs | A9C7 |
| DelComp | AA4D |
| DelMCEntries | AA60 |
| DelMenuItem | A952 |
| DelSearch | AA4C |
| Delay | A03B |
| Delete (PBDelete) | A009 |
| DeleteMenu | A936 |
| DeltaPoint | A94F |
| Dequeue | A96E |
| DetachResource | A992 |
| DialogSelect | A980 |
| DiffRgn | A8E6 |
| DisableItem | A93A |
| DispMCInfo | AA63 |
| DisposCCursor | AA26 |
| DisposCIcon | AA25 |
| DisposCTable | AA24 |
| DisposControl (DisposeControl) | A955 |
| DisposDialog | A983 |
| DisposGDevice | AA30 |
| DisposHandle | A023 |
| DisposMenu (DisposeMenu) | A932 |
| DisposPixMap | AA04 |
| DisposPixPat | AA08 |
| DisposPtr | A01F |
| DisposRgn (DisposeRgn) | A8D9 |
| DisposWindow (DisposeWindow) | A914 |
| DisposePalette | AA93 |
| DoVBLTask | A072 |
| DragControl | A967 |
| DragGrayRgn | A905 |
| DragTheRgn | A926 |
| DragWindow | A925 |
| Draw1Control | A96D |
| DrawChar | A883 |
| DrawControls | A969 |
| DrawDialog | A981 |
| DrawGrowIcon | A904 |
| DrawMenuBar | A937 |
| DrawNew | A90F |
| DrawPicture | A8F6 |
| DrawString | A884 |
| DrawText | A885 |
| DrvrInstall | A03D |
| DrvrRemove | A03E |
| Eject (PBEject) | A017 |
| Elems68K | A9EC |
| EmptyHandle | A02B |
| EmptyRect | A8AE |
| EmptyRgn | A8E2 |
| EnableItem | A939 |
| EndUpdate | A923 |
| Enqueue | A96F |
| EqualPt | A881 |
| EqualRect | A8A6 |
| EqualRgn | A8E3 |
| EraseArc | A8C0 |
| EraseOval | A8B9 |
| ErasePoly | A8C8 |
| EraseRect | A8A3 |
| EraseRgn | A8D4 |
| EraseRoundRect | A8B2 |
| ErrorSound | A98C |
| EventAvail | A971 |
| ExitToShell | A9F4 |
| FMSwapFont | A901 |
| FP68K | A9EB |
| FillArc | A8C2 |
| FillCArc | AA11 |
| FillCOval | AA0F |
| FillCPoly | AA13 |
| FillCRect | AA0E |
| FillCRgn | AA12 |
| FillCRoundRect | AA10 |
| FillOval | A8BB |
| FillPoly | A8CA |
| FillRect | A8A5 |
| FillRgn | A8D6 |
| FillRoundRect | A8B4 |
| FindControl | A96C |
| FindDItem | A984 |
| FindWindow | A92C |
| Fix2Frac | A841 |
| Fix2Long | A840 |
| Fix2X | A843 |
| FixAtan2 | A818 |
| FixDiv | A84D |
| FixMul | A868 |
| FixRatio | A869 |
| FixRound | A86C |
| FlashMenuBar | A94C |
| FlushEvents | A032 |
| FlushFile (PBFlushFile) | A045 |
| FlushVol (PBFlushVol) | A013 |
| FontMetrics | A835 |
| ForeColor | A862 |
| Frac2Fix | A842 |
| Frac2X | A845 |
| FracCos | A847 |
| FracDiv | A84B |
| FracMul | A84A |
| FracSin | A848 |
| FracSqrt | A849 |
| FrameArc | A8BE |
| FrameOval | A8B7 |
| FramePoly | A8C6 |
| FrameRect | A8A1 |
| FrameRgn | A8D2 |
| FrameRoundRect | A8B0 |
| FreeAlert | A98A |
| FreeDialog | A97A |
| FreeMem | A01C |
| FrontWindow | A924 |
| Get1IxResource (Get1IndResource) | A80E |
| Get1IxType (Get1IndType) | A80F |
| Get1NamedResource | A820 |
| Get1Resource | A81F |
| GetADBInfo | A079 |
| GetAppParms | A9F5 |
| GetAuxCtl | AA44 |
| GetAuxWin | AA42 |
| GetBackColor | AA1A |
| GetCCursor | AA1B |
| GetCIcon | AA1E |
| GetCPixel | AA17 |
| GetCRefCon | A95A |
| GetCTable | AA18 |
| GetCTitle | A95E |
| GetCVariant | A809 |
| GetCWMgrPort | AA48 |
| GetClip | A87A |
| GetCtlAction | A96A |
| GetCtlValue | A960 |
| GetCTSeed | AA28 |
| GetCursor | A9B9 |
| GetDefaultStartup | A07D |
| GetDeviceList | AA29 |
| GetDItem | A98D |
| GetEOF (PBGetEOF) | A011 |
| GetEntryColor | AA9B |
| GetEntryUsage | AA9D |
| GetFName (GetFontName) | A8FF |
| GetFNum | A900 |
| GetFPos (PBGetFPos) | A018 |
| GetFileInfo (PBGetFInfo) | A00C |
| GetFontInfo | A88B |
| GetForeColor | AA19 |
| GetGDevice | AA32 |
| GetHandleSize | A025 |
| GetIText | A990 |
| GetIcon | A9BB |
| GetIndADB | A078 |
| GetIndResource | A99D |
| GetIndType | A99F |
| GetItem | A946 |
| GetItemCmd | A84E |
| GetItmIcon (GetItemIcon) | A93F |
| GetItmMark (GetItemMark) | A943 |
| GetItmStyle (GetItemStyle) | A941 |
| GetKeys | A976 |
| GetMCEntry | AA64 |
| GetMCInfo | AA61 |
| GetMHandle | A949 |
| GetMainDevice | AA2A |
| GetMaxCtl (GetCtlMax) | A962 |
| GetMaxDevice | AA27 |
| GetMenuBar | A93B |
| GetMinCtl (GetCtlMin) | A961 |
| GetMouse | A972 |
| GetNamedResource | A9A1 |
| GetNewCWindow | AA46 |
| GetNewControl | A9BE |
| GetNewDialog | A97C |
| GetNewMBar | A9C0 |
| GetNewPalette | AA92 |
| GetNewWindow | A9BD |
| GetNextDevice | AA2B |
| GetNextEvent | A970 |
| GetOSDefault | A084 |
| GetOSEvent | A031 |
| GetPalette | AA96 |
| GetPattern | A9B8 |
| GetPen | A89A |
| GetPenState | A898 |
| GetPicture | A9BC |
| GetPixPat | AA0C |
| GetPixel | A865 |
| GetPort | A874 |
| GetPtrSize | A021 |
| GetRMenu (GetMenu) | A9BF |
| GetResAttrs | A9A6 |
| GetResFileAttrs | A9F6 |
| GetResInfo | A9A8 |
| GetResource | A9A0 |
| GetScrap | A9FD |
| GetString | A9BA |
| GetSubTable | AA37 |
| GetTrapAddress | A146 |
| GetVideoDefault | A080 |
| GetVol (PBGetVol) | A014 |
| GetVolInfo (PBGetVInfo) | A007 |
| GetWMgrPort | A910 |
| GetWRefCon | A917 |
| GetWTitle | A919 |
| GetWVariant | A80A |
| GetWindowPic | A92F |
| GetZone | A11A |
| GlobalToLocal | A871 |
| GrafDevice | A872 |
| GrowWindow | A92B |
| HClrRBit | A068 |
| HGetState | A069 |
| HLock | A029 |
| HNoPurge | A04A |
| HPurge | A049 |
| HSetRBit | A067 |
| HSetState | A06A |
| HUnlock | A02A |
| HandAndHand | A9E4 |
| HandToHand | A9E1 |
| HandleZone | A126 |
| HiWord | A86A |
| HideControl | A958 |
| HideCursor | A852 |
| HideDItem | A827 |
| HidePen | A896 |
| HideWindow | A916 |
| HiliteColor | AA22 |
| HiliteControl | A95D |
| HiliteMenu | A938 |
| HiliteWindow | A91C |
| HomeResFile | A9A4 |
| Index2Color | AA34 |
| InfoScrap | A9F9 |
| InitAllPacks | A9E6 |
| InitApplZone | A02C |
| InitCport | AA01 |
| InitCursor | A850 |
| InitDialogs | A97B |
| InitFonts | A8FE |
| InitGDevice | AA2E |
| InitGraf | A86E |
| InitMenus | A930 |
| InitPack | A9E5 |
| InitPalettes | AA90 |
| InitPort | A86D |
| InitProcMenu | A808 |
| InitQueue (FInitQueue) | A016 |
| InitResources | A995 |
| InitUtil | A03F |
| InitWindows | A912 |
| InitZone | A019 |
| InsMenuItem | A826 |
| InsertMenu | A935 |
| InsertResMenu | A951 |
| InsetRect | A8A9 |
| InsetRgn | A8E1 |
| InternalWait | A07F |
| InvalRect | A928 |
| InvalRgn | A927 |
| InverRect (InvertRect) | A8A4 |
| InverRgn (InvertRgn) | A8D5 |
| InverRoundRect (InvertRoundRect) | A8B3 |
| InvertArc | A8C1 |
| InvertColor | AA35 |
| InvertOval | A8BA |
| InvertPoly | A8C9 |
| IsDialogEvent | A97F |
| KeyTrans | A9C3 |
| KillControls | A956 |
| KillIO (PBKillIO) | A006 |
| KillPicture | A8F5 |
| KillPoly | A8CD |
| Launch | A9F2 |
| Line | A892 |
| LineTo | A891 |
| LoWord | A86B |
| LoadResource | A9A2 |
| LoadSeg | A9F0 |
| LocalToGlobal | A870 |
| LodeScrap (LoadScrap) | A9FB |
| Long2Fix | A83F |
| LongMul | A867 |
| MakeITable | AA39 |
| MakeRGBPat | AA0D |
| MapPoly | A8FC |
| MapPt | A8F9 |
| MapRect | A8FA |
| MapRgn | A8FB |
| MaxApplZone | A063 |
| MaxBlock | A061 |
| MaxMem | A11D |
| MaxSizeRsrc | A821 |
| MeasureText | A837 |
| MenuChoice | AA66 |
| MenuKey | A93E |
| MenuSelect | A93D |
| ModalDialog | A991 |
| MoreMasters | A036 |
| MountVol (PBMountVol) | A00F |
| Move | A894 |
| MoveControl | A959 |
| MoveHHi | A064 |
| MovePortTo | A877 |
| MoveTo | A893 |
| MoveWindow | A91B |
| Munger | A9E0 |
| NewCDialog | AA4B |
| NewCWindow | AA45 |
| NewControl | A954 |
| NewDialog | A97D |
| NewEmptyHandle | A066 |
| NewGDevice | AA2F |
| NewHandle | A122 |
| NewMenu | A931 |
| NewPalette | AA91 |
| NewPixMap | AA03 |
| NewPixPat | AA07 |
| NewPtr | A11E |
| NewRgn | A8D8 |
| NewString | A906 |
| NewWindow | A913 |
| NoteAlert | A987 |
| OSEventAvail | A030 |
| ObscureCursor | A856 |
| Offline (PBOffline) | A035 |
| OffsetPoly | A8CE |
| OffsetRect | A8A8 |
| OfsetRgn (OffsetRgn) | A8E0 |
| OpColor | AA21 |
| Open (PBOpen) | A000 |
| OpenCport | AA00 |
| OpenDeskAcc | A9B6 |
| OpenPicture | A8F3 |
| OpenPoly | A8CB |
| OpenPort | A86F |
| OpenRF (PBOpenRF) | A00A |
| OpenRFPerm | A9C4 |
| OpenResFile | A997 |
| OpenRgn | A8DA |
| PPostEvent | A12F |
| PackBits | A8CF |
| PaintArc | A8BF |
| PaintBehind | A90D |
| PaintOne | A90C |
| PaintOval | A8B8 |
| PaintPoly | A8C7 |
| PaintRect | A8A2 |
| PaintRgn | A8D3 |
| PaintRoundRect | A8B1 |
| Palette2CTab | AAA0 |
| ParamText | A98B |
| PenMode | A89C |
| PenNormal | A89E |
| PenPat | A89D |
| PenPixPat | AA0A |
| PenSize | A89B |
| PicComment | A8F2 |
| PinRect | A94E |
| PlotCIcon | AA1F |
| PlotIcon | A94B |
| PmBackColor | AA98 |
| PmForeColor | AA97 |
| PopUpMenuSelect | A80B |
| PortSize | A876 |
| PostEvent | A02F |
| PrGlue | A8FD |
| ProtectEntry | AA3D |
| Pt2Rect | A8AC |
| PtInRect | A8AD |
| PtInRgn | A8E8 |
| PtToAngle | A8C3 |
| PtrAndHand | A9EF |
| PtrToHand | A9E3 |
| PtrToXHand | A9E2 |
| PtrZone | A148 |
| PurgeMem | A04D |
| PurgeSpace | A062 |
| PutScrap | A9FE |
| QDError | AA40 |
| RDrvrInstall | A04F |
| RGBBackColor | AA15 |
| RGBForeColor | AA14 |
| RGetResource | A80C |
| Random | A861 |
| Read (PBRead) | A002 |
| ReadDateTime | A039 |
| RealColor | AA36 |
| RealFont | A902 |
| ReallocHandle | A027 |
| RecoverHandle | A128 |
| RectInRgn | A8E9 |
| RectRgn | A8DF |
| RelString | A050 |
| ReleaseResource | A9A3 |
| Rename (PBRename) | A00B |
| ResError | A9AF |
| ReserveEntry | AA3E |
| ResrvMem | A040 |
| RestoreEntries | AA4A |
| RmveResource | A9AD |
| RsrcMapEntry | A9C5 |
| RsrcZoneInit | A996 |
| RstFilLock (PBRstFLock) | A042 |
| SaveEntries | AA49 |
| SaveOld | A90E |
| ScalePt | A8F8 |
| ScrollRect | A8EF |
| Secs2Date | A9C6 |
| SectRect | A8AA |
| SectRgn | A8E4 |
| SeedCFill | AA50 |
| SeedFill | A839 |
| SelIText | A97E |
| SelectWindow | A91F |
| SendBehind | A921 |
| SetADBInfo | A07A |
| SetAppBase (SetApplBase) | A057 |
| SetApplLimit | A02D |
| SetCCursor | AA1C |
| SetCPixel | AA16 |
| SetCPortPix | AA06 |
| SetCRefCon | A95B |
| SetCTitle | A95F |
| SetClientID | AA3C |
| SetClip | A879 |
| SetCtlAction | A96B |
| SetCtlColor | AA43 |
| SetCtlValue | A963 |
| SetCursor | A851 |
| SetDItem | A98E |
| SetDateTime | A03A |
| SetDefaultStartup | A07E |
| SetDeskCPat | AA47 |
| SetDeviceAttribute | AA2D |
| SetEOF (PBSetEOF) | A012 |
| SetEmptyRgn | A8DD |
| SetEntries | AA3F |
| SetEntryColor | AA9C |
| SetEntryUsage | AA9E |
| SetFPos (PBSetFPos) | A044 |
| SetFScaleDisable | A834 |
| SetFilLock (PBSetFLock) | A041 |
| SetFilType (PBSetFVers) | A043 |
| SetFileInfo (PBSetFInfo) | A00D |
| SetFontLock | A903 |
| SetGDevice | AA31 |
| SetGrowZone | A04B |
| SetHandleSize | A024 |
| SetIText | A98F |
| SetItem | A947 |
| SetItemCmd | A84F |
| SetItmIcon (SetItemIcon) | A940 |
| SetItmMark (SetItemMark) | A944 |
| SetItmStyle (SetItemStyle) | A942 |
| SetMCEntries | AA65 |
| SetMCInfo | AA62 |
| SetMFlash (SetMenuFlash) | A94A |
| SetMaxCtl (SetCtlMax) | A965 |
| SetMenuBar | A93C |
| SetMinCtl (SetCtlMin) | A964 |
| SetOSDefault | A083 |
| SetOrigin | A878 |
| SetPBits (SetPortBits) | A875 |
| SetPalette | AA95 |
| SetPenState | A899 |
| SetPort | A873 |
| SetPt | A880 |
| SetPtrSize | A020 |
| SetRecRgn (SetRectRgn) | A8DE |
| SetRect | A8A7 |
| SetResAttrs | A9A7 |
| SetResFileAttrs | A9F7 |
| SetResInfo | A9A9 |
| SetResLoad | A99B |
| SetResPurge | A993 |
| SetStdCProcs | AA4E |
| SetStdProcs | A8EA |
| SetString | A907 |
| SetTrapAddress | A047 |
| SetVideoDefault | A081 |
| SetVol (PBSetVol) | A015 |
| SetWRefCon | A918 |
| SetWTitle | A91A |
| SetWinColor | AA41 |
| SetWindowPic | A92E |
| SetZone | A01B |
| ShieldCursor | A855 |
| ShowControl | A957 |
| ShowCursor | A853 |
| ShowDItem | A828 |
| ShowHide | A908 |
| ShowPen | A897 |
| ShowWindow | A915 |
| SizeControl | A95C |
| SizeRsrc (SizeResource) | A9A5 |
| SizeWindow | A91D |
| SlopeFromAngle | A8BC |
| SlotVInstall | A06F |
| SlotVRemove | A070 |
| SndAddModifier | A802 |
| SndControl | A806 |
| SndDisposeChannel | A801 |
| SndDoCommand | A803 |
| SndDoImmediate | A804 |
| SndNewChannel | A807 |
| SndPlay | A805 |
| SpaceExtra | A88E |
| StackSpace | A065 |
| Status (PBStatus) | A005 |
| StdArc | A8BD |
| StdBits | A8EB |
| StdComment | A8F1 |
| StdGetPic | A8EE |
| StdLine | A890 |
| StdOval | A8B6 |
| StdPoly | A8C5 |
| StdPutPic | A8F0 |
| StdRRect | A8AF |
| StdRect | A8A0 |
| StdRgn | A8D1 |
| StdText | A882 |
| StdTxMeas | A8ED |
| StillDown | A973 |
| StopAlert | A986 |
| StringWidth | A88C |
| StripAddress | A055 |
| StuffHex | A866 |
| SubPt | A87F |
| SwapMMUMode | A05D |
| SysBeep | A9C8 |
| SysEdit (SystemEdit) | A9C2 |
| SysEnvirons | A090 |
| SysError | A9C9 |
| SystemClick | A9B3 |
| SystemEvent | A9B2 |
| SystemMenu | A9B5 |
| SystemTask | A9B4 |
| TEActivate | A9D8 |
| TEAutoView | A813 |
| TECalText | A9D0 |
| TEClick | A9D4 |
| TECopy | A9D5 |
| TECut | A9D6 |
| TEDeactivate | A9D9 |
| TEDelete | A9D7 |
| TEDispose | A9CD |
| TEGetOffset | A83C |
| TEGetText | A9CB |
| TEIdle | A9DA |
| TEInit | A9CC |
| TEInsert | A9DE |
| TEKey | A9DC |
| TENew | A9D2 |
| TEPaste | A9DB |
| TEPinScroll | A812 |
| TEScroll | A9DD |
| TESelView | A811 |
| TESetJust | A9DF |
| TESetSelect | A9D1 |
| TESetText | A9CF |
| TEStyleNew | A83E |
| TEUpdate | A9D3 |
| TestControl | A966 |
| TestDeviceAttribute | AA2C |
| TextBox | A9CE |
| TextFace | A888 |
| TextFont | A887 |
| TextMode | A889 |
| TextSize | A88A |
| TextWidth | A886 |
| TickCount | A975 |
| TrackBox | A83B |
| TrackControl | A968 |
| TrackGoAway | A91E |
| UnionRect | A8AB |
| UnionRgn | A8E5 |
| Unique1ID | A810 |
| UniqueID | A9C1 |
| UnloadSeg | A9F1 |
| UnlodeScrap (UnloadScrap) | A9FA |
| UnmountVol (PBUnmountVol) | A00E |
| UnpackBits | A8D0 |
| UpdateResFile | A999 |
| UpdtControl | A953 |
| UpdtDialog | A978 |
| UprString | A054 |
| UseResFile | A998 |
| VInstall | A033 |
| VRemove | A034 |
| ValidRect | A92A |
| ValidRgn | A929 |
| WaitMouseUp | A977 |
| Write (PBWrite) | A003 |
| WriteParam | A038 |
| WriteResource | A9B0 |
| X2Fix | A844 |
| X2Frac | A846 |
| XorRgn | A8E7 |
| ZeroScrap | A9FC |
| ZoomWindow | A83A |

---

## Packages

### Pack0 (A9E7) — List Manager

| Routine | Selector |
|---------|----------|
| LActivate | 0 |
| LAddColumn | 4 |
| LAddRow | 8 |
| LAddToCell | 12 |
| LAutoScroll | 16 |
| LCellSize | 20 |
| LClick | 24 |
| LClrCell | 28 |
| LDelColumn | 32 |
| LDelRow | 36 |
| LDispose | 40 |
| LDoDraw | 44 |
| LDraw | 48 |
| LFind | 52 |
| LGetCell | 56 |
| LGetSelect | 60 |
| LLastClick | 64 |
| LNew | 68 |
| LNextCell | 72 |
| LRect | 76 |
| LScroll | 80 |
| LSearch | 84 |
| LSetCell | 88 |
| LSetSelect | 92 |
| LSize | 96 |
| LUpdate | 100 |

### Pack2 (A9E9) — Disk Initialization

| Routine | Selector |
|---------|----------|
| DIBadMount | 0 |
| DILoad | 2 |
| DIUnload | 4 |
| DIFormat | 6 |
| DIVerify | 8 |
| DIZero | 10 |

### Pack3 (A9EA) — Standard File

| Routine | Selector |
|---------|----------|
| SFPutFile | 1 |
| SFGetFile | 2 |
| SFPPutFile | 3 |
| SFPGetFile | 4 |

### Pack4 (A9EB) — FP68K (Floating Point)

### Pack5 (A9EC) — Elems68K (Transcendental Functions)

### Pack6 (A9ED) — International Utilities

| Routine | Selector |
|---------|----------|
| IUDateString | 0 |
| IUTimeString | 2 |
| IUMetric | 4 |
| IUGetIntl | 6 |
| IUSetIntl | 8 |
| IUMagString | 10 |
| IUMagIDString | 12 |
| IUDatePString | 14 |
| IUTimePString | 16 |

### Pack7 (A9EE) — Binary-Decimal Conversion

| Routine | Selector |
|---------|----------|
| NumToString | 0 |
| StringToNum | 1 |
| PStr2Dec | 2 |
| Dec2Str | 3 |
| CStr2Dec | 4 |

### Pack12 (A82E) — Color Picker

| Routine | Selector |
|---------|----------|
| Fix2SmallFract | 1 |
| SmallFract2Fix | 2 |
| CMY2RGB | 3 |
| RGB2CMY | 4 |
| HSL2RGB | 5 |
| RGB2HSL | 6 |
| HSV2RGB | 7 |
| RGB2HSV | 8 |
| GetColor | 9 |

### HFSDispatch (A260) — Hierarchical File System

| Routine | Selector |
|---------|----------|
| OpenWD | 1 |
| CloseWD | 2 |
| CatMove | 5 |
| DirCreate | 6 |
| GetWDInfo | 7 |
| GetFCBInfo | 8 |
| GetCatInfo | 9 |
| SetCatInfo | 10 |
| SetVolInfo | 11 |
| LockRng | 16 |
| UnlockRng | 17 |

### SCSIDispatch (A815)

| Routine | Selector |
|---------|----------|
| SCSIReset | 0 |
| SCSIGet | 1 |
| SCSISelect | 2 |
| SCSICmd | 3 |
| SCSIComplete | 4 |
| SCSIRead | 5 |
| SCSIWrite | 6 |
| SCSIInstall | 7 |
| SCSIRBlind | 8 |
| SCSIWBlind | 9 |
| SCSIStat | 10 |
| SCSISelAtn | 11 |
| SCSIMsgIn | 12 |
| SCSIMsgOut | 13 |

### ScriptUtil (A8B5) — Script Manager

| Routine | Selector |
|---------|----------|
| smFontScript | 0 |
| smIntlScript | 2 |
| smKybdScript | 4 |
| smFont2Script | 6 |
| smGetEnvirons | 8 |
| smSetEnvirons | 10 |
| smGetScript | 12 |
| smSetScript | 14 |
| smCharByte | 16 |
| smCharType | 18 |
| smPixel2Char | 20 |
| smChar2Pixel | 22 |
| smTranslit | 24 |
| smFindWord | 26 |
| smHiliteText | 28 |
| smDrawJust | 30 |
| smMeasureJust | 32 |

### SlotManager (A06E)

| Routine | Selector |
|---------|----------|
| sReadByte | 0 |
| sReadWord | 1 |
| sReadLong | 2 |
| sGetcString | 3 |
| sGetBlock | 5 |
| sFindStruct | 6 |
| sReadStruct | 7 |
| sReadInfo | 16 |
| sReadPRAMRec | 17 |
| sPutPRAMRec | 18 |
| sReadFHeader | 19 |
| sNextRsrc | 20 |
| sNextTypesRsrc | 21 |
| sRsrcInfo | 22 |
| sDisposePtr | 23 |
| sCkCardStatus | 24 |
| sReadDrvrName | 25 |
| sFindDevBase | 27 |
| InitSDeclMgr | 32 |
| sPrimaryInit | 33 |
| sCardChanged | 34 |
| sExec | 35 |
| sOffsetData | 36 |
| InitPRAMRecs | 37 |
| sReadPBSize | 38 |
| sCalcStep | 40 |
| InitsRsrcTable | 41 |
| sSearchSRT | 42 |
| sUpdateSRT | 43 |
| sCalcsPointer | 44 |
| sGetDriver | 45 |
| sPtrToSlot | 46 |
| sFindsInfoRecPtr | 47 |
| sFindsRsrcPtr | 48 |
| sdeleteSRTRec | 49 |

### Shutdown (A895)

| Routine | Selector |
|---------|----------|
| ShutDwnPower | 1 |
| ShutDwnStart | 2 |
| ShutDwnInstall | 3 |
| ShutDwnRemove | 4 |

### TEDispatch (A83D) — TextEdit Extensions

| Routine | Selector |
|---------|----------|
| TEStylePaste | 0 |
| TESetStyle | 1 |
| TEReplaceStyle | 2 |
| TEGetStyle | 3 |
| GetStyleHandle | 4 |
| SetStyleHandle | 5 |
| GetStyleScrap | 6 |
| TEStyleInsert | 7 |
| TEGetPoint | 8 |
| TEGetHeight | 9 |

### InternalWait (A07F)

| Routine | Selector |
|---------|----------|
| SetTimeout | 0 |
| GetTimeout | 1 |
