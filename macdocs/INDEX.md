# Macintosh Technical Documentation Index

This folder contains digitized Apple developer documentation from the late 1980s/early 1990s, covering the Macintosh 128K through Macintosh II. The documentation is in HTML format with preformatted text, suitable for `read_file` access using the line ranges listed below.

All files are in `macdocs/tech_doc/`.

## Quick-Reference Extracts

For the highest-traffic content, pre-extracted markdown files are available in `macdocs/ref/`:

- [HARDWARE.md](ref/HARDWARE.md) — Full Macintosh Hardware chapter (address space, video, sound, SCC, keyboard, IWM, RTC, SCSI, VIA)
- [TRAP_TABLE.md](ref/TRAP_TABLE.md) — System trap number → routine name mapping
- [GLOBAL_VARS.md](ref/GLOBAL_VARS.md) — Low-memory global variable addresses

## Cross-Reference

See [EMULATOR_MAP.md](EMULATOR_MAP.md) for mappings between emulator source files (`src/devices/`, `src/core/`) and documentation sections.

---

## im202.html — Inside Macintosh 2.0.2

**~61,700 lines.** The authoritative Macintosh API reference. Contains complete Pascal function signatures, assembly-language equates with trap numbers, data structure layouts with field offsets, and hardware register documentation. Covers Toolbox, Operating System, and hardware from Mac 128K through Mac II.

| ID | Lines | Title | Description |
|----|-------|-------|-------------|
| im000 | 69–271 | Preface | About Inside Macintosh: structure, conventions, version numbers, language notes |
| im001 | 272–712 | A Road Map | Overview of Mac software architecture; Toolbox vs OS; simple example program |
| im002 | 713–1085 | Compatibility Guidelines | Guidelines for cross-model compatibility, memory, assembly, hardware, localization |
| im003 | 1086–3021 | The Macintosh User Interface Guidelines | Complete UI guidelines: menus, windows, dialogs, keyboard, mouse, graphics, text |
| im004 | 3022–3254 | Macintosh Memory Management — An Introduction | Brief intro to stack vs heap, pointers vs handles, general data types |
| im005 | 3255–3734 | Using Assembly Language | Trap dispatch table, trap mechanism, trap word format, calling conventions, register saving |
| im006 | 3735–6499 | QuickDraw | Core graphics: coordinate plane, points, rectangles, regions, bit maps, patterns, cursors, GrafPort, pen, transfer modes, drawing routines |
| im007 | 6500–9331 | Color QuickDraw | Color representation (RGB), CGrafPort, pixel maps, pixel patterns, transfer modes, arithmetic modes, color cursors/icons, Color QD routines |
| im008 | 9332–9826 | Graphics Devices | GDevice records, multiple screens, offscreen drawing, device routines, graphics device resources |
| im009 | 9827–11404 | TextEdit | Text editing: edit records, selection ranges, style records, cutting/pasting, styled text, TE routines |
| im010 | 11405–11892 | The Apple Desktop Bus | ADB protocol: bus commands (SendReset, Flush, Listen, Talk), device registers, addressing, ADB Manager routines, driver installation |
| im011 | 11893–17027 | The AppleTalk Manager | Network protocols: ALAP, DDP, ATP, NBP, ASP, AFP; data structures, routines, examples for each layer |
| im012 | 17028–17153 | The Binary-Decimal Conversion Package | Integer↔decimal string conversion; SANE numeric scanner/formatter routines |
| im013 | 17154–17682 | The Color Manager | Color-selection support for Color QuickDraw: color tables, inverse tables, search/complement functions |
| im014 | 17683–17879 | The Color Picker Package | Standard color-selection dialog; RGB/HSV/HSL conversion routines |
| im015 | 17880–19188 | The Control Manager | Buttons, checkboxes, scroll bars: control records, part codes, color controls, defining custom controls (CDEF) |
| im016 | 19189–20068 | The Control Panel | Writing cdev (Control Panel device) extensions: resources, messages, storage, error checking |
| im017 | 20069–20187 | The Deferred Task Manager | Deferred interrupt task execution; DTInstall; deferred task queue structure. Key for slot interrupt handling |
| im018 | 20188–20655 | The Desk Manager | Desk accessories: opening/closing, event handling, periodic actions, writing custom DAs |
| im019 | 20656–22704 | The Device Manager | Device drivers: high/low-level routines, driver structure, DCE, I/O queue, unit table, interrupts (VIA level-1, SCC level-2), Chooser |
| im020 | 22705–24192 | The Dialog Manager | Modal/modeless dialogs, alerts, item lists, dialog records, color dialogs, event handling, item manipulation |
| im021 | 24193–24720 | The Disk Driver | 3.5" floppy disk driver: direct access, control calls, bypassing File Manager |
| im022 | 24721–25016 | The Disk Initialization Package | Disk formatting/initialization routines; HFS volume formatting |
| im023 | 25017–30929 | The File Manager | Comprehensive file I/O: volumes, directories, HFS, file access modes, shared environments, AppleShare, PBGetCatInfo, all File Manager routines |
| im024 | 30930–31135 | The Finder Interface | Finder↔application interface: Desktop file, signatures, file types, bundles, icons |
| im025 | 31136–31304 | The Floating-Point Arithmetic & Transcendental Functions Packages | SANE (IEEE 754): extended-precision arithmetic, MC68881 coprocessor support |
| im026 | 31305–32967 | The Font Manager | Font numbers, font families, FOND/NFNT records, width tables, font scaling, fractional widths |
| im027 | 32968–33823 | The International Utilities Package | Country-independent formatting: dates, times, numbers, currency, string comparison, sorting |
| im028 | 33824–34655 | The List Manager Package | List display in windows: cells, selection, scrolling, custom LDEF definition procedures |
| im029 | 34656–35772 | The Macintosh Hardware | **KEY CHAPTER.** MC68000, RAM, ROM, address map, video interface, sound generator, SCC serial, mouse, keyboard protocol, IWM disk, RTC, SCSI, VIA registers, system startup. See also [ref/HARDWARE.md](ref/HARDWARE.md) |
| im030 | 35773–37396 | The Memory Manager | Heap management: pointers, handles, heap zones, block structure, master pointers, allocating/releasing, purging, grow zones |
| im031 | 37397–39519 | The Menu Manager | Menu bar, hierarchical menus, pop-up menus, color menus, keyboard equivalents, MenuInfo, custom MDEF |
| im032 | 39520–39918 | The Operating System Event Manager | Low-level event posting/removal, event queue structure, system event mask |
| im033 | 39919–41062 | The Operating System Utilities | Parameter RAM, OS queues, pointer/handle manipulation, string comparison, date/time, trap dispatch table, GetMMUMode |
| im034 | 41063–41227 | The Package Manager | Package loading mechanism; list of system packages (Standard File, Disk Init, Intl Utils, etc.) |
| im035 | 41228–41759 | The Palette Manager | Color palette management: courteous/tolerant/animating/explicit colors, palette prioritization |
| im036 | 41760–43198 | The Printing Manager | Print records, print dialogs, printing loop, PrGeneral, draft/spool modes, low-level printer driver access |
| im037 | 43199–44778 | The Resource Manager | Resource files, resource types/IDs/names, ROM resources, system resources, reading/writing/modifying resources, resource file format |
| im038 | 44779–45214 | The Scrap Manager | Clipboard (desk scrap): cutting/pasting between apps, scrap data types, private scraps |
| im039 | 45215–47914 | The Script Manager | Multi-script text: character codes, drawing direction, text partitioning, CharByte, Pixel2Char, Transliterate, GetScript, KeyScript |
| im040 | 47915–48618 | The SCSI Manager | SCSI bus communication: SCSIGet/Select/Read/Write/Complete, transfer modes, disk partitioning, driver descriptor map |
| im041 | 48619–48957 | The Segment Loader | Code segmentation: loading/unloading segments, Finder information, jump table |
| im042 | 48958–49676 | The Serial Drivers | RS422 async serial: RAM/ROM serial drivers, baud rate, handshaking, SerReset/SerSetBuf, control calls |
| im043 | 49677–49837 | The Shutdown Manager | Shutdown/restart coordination: ShutDwnPower, ShutDwnStart, ShutDwnInstall for cleanup tasks |
| im044 | 49838–51121 | The Slot Manager | NuBus slot card firmware: slot parameter blocks, SExec, SReadByte/SReadWord, declaration ROM access |
| im045 | 51122–51848 | The Sound Driver | Legacy sound: square-wave, four-tone, free-form synthesizers (superseded by Sound Manager) |
| im046 | 51849–53035 | The Sound Manager | Modern sound: note/wave-table/sampled-sound synthesizers, snd resources, SndPlay, SndDoCommand, MIDI |
| im047 | 53036–53723 | The Standard File Package | Open/Save file dialogs: SFPutFile, SFGetFile, custom dialog hooks, file type filtering |
| im048 | 53724–54175 | The Start Manager | System startup sequence: hardware diagnostics, RAM test, ROM patches, disk boot, system initialization |
| im049 | 54176–54497 | The System Error Handler | Fatal error handling: system error alerts, error recovery mechanism, alert tables |
| im050 | 54498–54614 | The System Resource File | Contents of the System file: packages, drivers, desk accessories, initialization resources |
| im051 | 54615–54750 | The Time Manager | Millisecond-precision async wakeup service: TMTask queue, InsTime/PrimeTime/RmvTime |
| im052 | 54751–55748 | The Toolbox Event Manager | Event loop: event types, priority, keyboard events, event records, modifier flags, GetNextEvent/WaitNextEvent, journaling |
| im053 | 55749–56039 | The Vertical Retrace Manager | VBL interrupt tasks: 60Hz recurrent task scheduling, VBL queue, slot-based VBL tasks |
| im054 | 56040–57967 | The Window Manager | Windows and GrafPorts: window regions, window records, color windows, defining custom WDEF, display/sizing/moving |
| im055 | 57968–58603 | Toolbox Utilities | Fixed-point arithmetic (Fixed, Fract), string manipulation, bit/byte operations, graphics utilities |
| im056 | 58604–58920 | Appendix A — Result Codes | All system result codes ordered by value, with names and descriptions |
| im057 | 58921–59556 | Appendix B — Routines That May Move or Purge Memory | Lists which Toolbox/OS routines may cause heap compaction or purging |
| im058 | 59557–61449 | Appendix C — System Traps | **Trap number → routine name mapping.** Complete list of OS and Toolbox traps. See also [ref/TRAP_TABLE.md](ref/TRAP_TABLE.md) |
| im059 | 61450–61689 | Appendix D — Global Variables | **Low-memory global addresses and meanings.** See also [ref/GLOBAL_VARS.md](ref/GLOBAL_VARS.md) |
| im060 | 61690–end | Glossary | Definitions of key terms used throughout Inside Macintosh |

---

## tn405.html — Technical Notes Stack 4.0.5

**~20,300 lines.** 201 technical notes covering workarounds, bugs, practical code patterns, file formats, hardware specifics, and compatibility advice. Variable depth: some are brief redirects to Inside Macintosh, others contain extensive code examples and hardware details.

### Highlights for Emulator Development

| ID | Lines | Title | Why |
|----|-------|-------|-----|
| tn002 | 360–581 | Compatibility Guidelines | Software compatibility rules |
| tn010 | 787–1038 | Pinouts | DB-9/DB-25 connector pin mappings for serial, mouse, keyboard, floppy |
| tn021 | 1401–1651 | QuickDraw's Internal Picture Definition | PICT format internals |
| tn036 | 2351–2498 | Drive Queue Elements | Drive queue data structure for disk emulation |
| tn037 | 2499–2510 | Differentiating Between Logic Boards | Hardware identification |
| tn038 | 2511–2552 | The ROM Debugger | Built-in ROM debugger operation |
| tn041 | 2602–2890 | Drawing Into an Off-Screen Bitmap | Off-screen bitmap techniques |
| tn057 | 3984–3995 | Macintosh Plus Overview | Plus-specific hardware details |
| tn065 | 4082–4178 | Macintosh Plus Pinouts | Plus-specific connector pinouts |
| tn086 | 6036–6272 | MacPaint Document Format | MacPaint file format specification |
| tn088 | 6307–6591 | Signals | Serial signal details |
| tn096 | 8087–8360 | SCSI Bugs | Known SCSI Manager bugs and workarounds |
| tn100 | 8393–8421 | Compatibility with Large-Screen Displays | Screen size assumptions |
| tn113 | 9016–9045 | Boot Blocks | Boot block format and parameters |
| tn117 | 9200–9760 | Compatibility — Why & How | Extensive compatibility analysis with memory/screen details |
| tn120 | 9786–11917 | Principia Off-Screen Graphics Environments | Comprehensive off-screen drawing architecture (2131 lines!) |
| tn139 | 14048–14085 | Macintosh Plus ROM Versions | ROM version identification |
| tn160 | 15358–15522 | Key Mapping | Keyboard key code mapping |
| tn176 | 17148–17524 | Macintosh Memory Configurations | Memory layout for various Mac models |

### Full Technical Notes Listing

| ID | Lines | Title |
|----|-------|-------|
| tn000 | 315–347 | About Macintosh Technical Notes |
| tn001 | 348–359 | Desk Accessories and System Resources |
| tn002 | 360–581 | Compatibility Guidelines |
| tn003 | 582–610 | Command-Shift-Number Keys |
| tn004 | 611–635 | Error Returns from GetNewDialog |
| tn005 | 636–657 | Using Modeless Dialogs from Desk Accessories |
| tn006 | 658–693 | Shortcut for Owned Resources |
| tn007 | 694–733 | A Few Quick Debugging Tips |
| tn008 | 734–744 | RecoverHandle Bug in AppleTalk Pascal Interfaces |
| tn009 | 745–786 | Will Your AppleTalk Application Support Internets? |
| tn010 | 787–1038 | Pinouts |
| tn011 | 1039–1066 | Memory-Based MacWrite Format |
| tn012 | 1067–1094 | Disk-Based MacWrite Format |
| tn013 | 1095–1121 | MacWrite Clipboard Format |
| tn014 | 1122–1133 | The INIT 31 Mechanism |
| tn015 | 1134–1142 | Finder 4.1 |
| tn016 | 1143–1152 | MacWorks XL |
| tn017 | 1153–1169 | Low-Level Print Driver Calls |
| tn018 | 1170–1214 | TextEdit Conversion Utility |
| tn019 | 1215–1258 | How To Produce Continuous Sound Without Clicking |
| tn020 | 1259–1400 | Data Servers on AppleTalk |
| tn021 | 1401–1651 | QuickDraw's Internal Picture Definition |
| tn022 | 1652–1665 | TEScroll Bug |
| tn023 | 1666–1708 | Life With Font/DA Mover — Desk Accessories |
| tn024 | 1709–1780 | Available Volumes |
| tn025 | 1781–1820 | Don't Depend on Register A5 Within Trap Patches |
| tn026 | 1821–1898 | Character vs. String Operations in QuickDraw |
| tn027 | 1899–1935 | MacDraw's PICT File Format |
| tn028 | 1936–1970 | Finders and Foreign Drives |
| tn029 | 1971–2061 | Resources Contained in the Desktop File |
| tn030 | 2062–2102 | Font Height Tables |
| tn032 | 2103–2119 | Reserved Resource Types |
| tn033 | 2120–2141 | ImageWriter II Paper Motion |
| tn034 | 2142–2340 | User Items in Dialogs |
| tn035 | 2341–2350 | DrawPicture Problem |
| tn036 | 2351–2498 | Drive Queue Elements |
| tn037 | 2499–2510 | Differentiating Between Logic Boards |
| tn038 | 2511–2552 | The ROM Debugger |
| tn039 | 2553–2565 | Segment Loader Patch |
| tn040 | 2566–2601 | Finder Flags |
| tn041 | 2602–2890 | Drawing Into an Off-Screen Bitmap |
| tn042 | 2891–2935 | Pascal Routines Passed by Pointer |
| tn043 | 2936–2947 | Calling LoadSeg |
| tn044 | 2948–3005 | HFS Compatibility |
| tn045 | 3006–3015 | Inside Macintosh Quick Reference |
| tn046 | 3016–3037 | Separate Resource Files |
| tn047 | 3038–3589 | Customizing Standard File |
| tn048 | 3590–3700 | Bundles |
| tn050 | 3701–3719 | Calling SetResLoad |
| tn051 | 3720–3742 | Debugging With PurgeMem and CompactMem |
| tn052 | 3743–3756 | Calling _Launch From a High-Level Language |
| tn053 | 3757–3820 | MoreMasters Revisited |
| tn054 | 3821–3830 | Limit to Size of Resources |
| tn055 | 3831–3951 | Drawing Icons |
| tn056 | 3952–3983 | Break/CTS Device Driver Event Structure |
| tn057 | 3984–3995 | Macintosh Plus Overview |
| tn058 | 3996–4005 | International Utilities Bug |
| tn059 | 4006–4024 | Pictures and Clip Regions |
| tn060 | 4025–4035 | Drawing Characters into a Narrow GrafPort |
| tn061 | 4036–4045 | GetItemStyle Bug |
| tn062 | 4046–4057 | Don't Use Resource Header Application Bytes |
| tn063 | 4058–4070 | WriteResource Bug Patch |
| tn064 | 4071–4081 | IAZNotify |
| tn065 | 4082–4178 | Macintosh Plus Pinouts |
| tn066 | 4179–4225 | Determining Which File System is Active |
| tn067 | 4226–4269 | Finding the "Blessed Folder" |
| tn068 | 4270–4544 | Searching Volumes — Solutions and Problems |
| tn069 | 4545–4663 | Setting ioFDirIndex in PBGetCatInfo Calls |
| tn070 | 4664–4725 | Forcing Disks to be Either 400K or 800K |
| tn071 | 4726–4868 | Finding Drivers in the Unit Table |
| tn072 | 4869–5048 | Optimizing for the LaserWriter — Techniques |
| tn073 | 5049–5152 | Color Printing |
| tn074 | 5153–5171 | Don't Use the Resource Fork for Data |
| tn075 | 5172–5289 | Apple's Multidisk Installer |
| tn076 | 5290–5299 | The Macintosh Plus Update Installation Script |
| tn077 | 5300–5527 | HFS Ruminations |
| tn078 | 5528–5577 | Resource Manager Tips |
| tn079 | 5578–5762 | _ZoomWindow |
| tn080 | 5763–5818 | Standard File Tips |
| tn081 | 5819–5904 | Caching |
| tn082 | 5905–5951 | TextEdit — Advice & Descent |
| tn083 | 5952–5963 | System Heap Size Warning |
| tn084 | 5964–6004 | Edit File Format |
| tn085 | 6005–6035 | GetNextEvent; Blinking Apple Menu |
| tn086 | 6036–6272 | MacPaint Document Format |
| tn087 | 6273–6306 | Error in FCBPBRec |
| tn088 | 6307–6591 | Signals |
| tn089 | 6592–6601 | DrawPicture Bug |
| tn090 | 6602–6611 | SANE Incompatibilities |
| tn091 | 6612–7256 | Optimizing for the LaserWriter — Picture Comments |
| tn092 | 7257–7311 | The Appearance of Text |
| tn093 | 7312–7394 | MPW — {$LOAD}; _DataInit; %_MethTables |
| tn094 | 7395–7432 | Tags |
| tn095 | 7433–8086 | How To Add Items to the Print Dialogs |
| tn096 | 8087–8360 | SCSI Bugs |
| tn097 | 8361–8370 | PrSetError Problem |
| tn098 | 8371–8380 | Short-Circuit Booleans in Lisa Pascal |
| tn099 | 8381–8392 | Standard File Bug in System 3.2 |
| tn100 | 8393–8421 | Compatibility with Large-Screen Displays |
| tn101 | 8422–8543 | CreateResFile and the Poor Man's Search Path |
| tn102 | 8544–8674 | HFS Elucidations |
| tn103 | 8675–8714 | MaxApplZone & MoveHHi from Assembly Language |
| tn104 | 8715–8802 | MPW — Accessing Globals From Assembly Language |
| tn105 | 8803–8820 | MPW Object Pascal Without MacApp |
| tn106 | 8821–8852 | The Real Story — VCBs and Drive Numbers |
| tn107 | 8853–8872 | Nulls in Filenames |
| tn108 | 8873–8961 | AddDrive, DrvrInstall and DrvrRemove |
| tn109 | 8962–8973 | Bug in MPW 1.0 Language Libraries |
| tn110 | 8974–8985 | MPW — Writing Stand-Alone Code |
| tn111 | 8986–9000 | MoveHHi and SetResPurge |
| tn112 | 9001–9015 | FindDItem |
| tn113 | 9016–9045 | Boot Blocks |
| tn114 | 9046–9061 | AppleShare and Old Finders |
| tn115 | 9062–9120 | Application Configuration with Stationery Pads |
| tn116 | 9121–9199 | AppleShare-able Apps & the Resource Manager |
| tn117 | 9200–9760 | Compatibility — Why & How |
| tn118 | 9761–9773 | How to Check and Handle Printing Errors |
| tn119 | 9774–9785 | Determining If Color QuickDraw Exists |
| tn120 | 9786–11917 | Principia Off-Screen Graphics Environments |
| tn121 | 11918–11949 | Using the High-Level AppleTalk Routines |
| tn122 | 11950–11980 | Device-Independent Printing |
| tn123 | 11981–12045 | Bugs in LaserWriter ROMs |
| tn124 | 12046–12060 | Low-Level Printing Calls With AppleTalk ImageWriters |
| tn125 | 12061–12082 | Effect of Spool-a-page/Print-a-page on Shared Printers |
| tn126 | 12083–12320 | Sub(Launching) from a High-Level Language |
| tn127 | 12321–12352 | TextEdit EOL Ambiguity |
| tn128 | 12353–12561 | PrGeneral |
| tn129 | 12562–12676 | _Gestalt & _SysEnvirons — A Never-Ending Story |
| tn130 | 12677–12692 | Clearing ioCompletion |
| tn131 | 12693–12754 | TextEdit Bugs in System 4.2 |
| tn132 | 12755–12807 | AppleTalk Interface Update |
| tn133 | 12808–12830 | Am I Talking To A Spooler? |
| tn134 | 12831–13269 | Hard Disk Medic & Booting Camp |
| tn135 | 13270–13963 | Getting through CUSToms |
| tn136 | 13964–13995 | Register A5 Within GrowZone Functions |
| tn137 | 13996–14022 | AppleShare 1.1 Server FPMove Bug |
| tn138 | 14023–14047 | Using KanjiTalk with a non-Japanese Macintosh Plus |
| tn139 | 14048–14085 | Macintosh Plus ROM Versions |
| tn140 | 14086–14109 | Why PBHSetVol is Dangerous |
| tn141 | 14110–14127 | Maximum Number of Resources in a File |
| tn142 | 14128–14164 | Avoid Use of Network Events |
| tn143 | 14165–14175 | Don't Call ADBReInit on the SE with System 4.1 |
| tn144 | 14176–14302 | Macintosh Color Monitor Connections |
| tn145 | 14303–14314 | Debugger FKEY |
| tn146 | 14315–14414 | Notes on MPW's -mc68881 Option |
| tn147 | 14415–14440 | Finder Notes — "Get Info" Default & Icon Masks |
| tn148 | 14441–14478 | Suppliers for Macintosh II Board Developers |
| tn149 | 14479–14507 | Document Names and the Printing Manager |
| tn150 | 14508–14517 | Macintosh SE Disk Driver Bug |
| tn151 | 14518–14542 | System Error 33, "zcbFree has gone negative" |
| tn152 | 14543–14608 | Using Laser Prep Routines |
| tn153 | 14609–14657 | Changes in International Utilities and Resources |
| tn154 | 14658–14873 | Displaying Large PICT Files |
| tn155 | 14874–14894 | Handles and Pointers — Identity Crisis |
| tn156 | 14895–15001 | Checking for Specific Functionality |
| tn157 | 15002–15082 | Problem with GetVInfo |
| tn158 | 15083–15326 | Frequently Asked MultiFinder Questions |
| tn159 | 15327–15357 | Hard Disk Hacking |
| tn160 | 15358–15522 | Key Mapping |
| tn161 | 15523–16114 | A Printing Loop That Cares… |
| tn162 | 16115–16124 | MPW 2.0 Pascal Compiler Bug |
| tn163 | 16125–16224 | Adding Color With CopyBits |
| tn164 | 16225–16258 | MPW C Functions — To declare or not to declare… |
| tn165 | 16259–16293 | Creating Files Inside an AppleShare Drop Folder |
| tn166 | 16294–16361 | MPW C Functions Using Strings or Points as Arguments |
| tn167 | 16362–16405 | AppleShare Foreground Applications |
| tn168 | 16406–16635 | HyperCard And You: Economy Edition |
| tn169 | 16636–16649 | HyperCard 1.0.1 and 1.1 Anomalies |
| tn170 | 16650–16664 | HyperCard File Format |
| tn171 | 16665–16850 | Things You Wanted to Know About _PackBits |
| tn172 | 16851–16882 | Parameters for MDEF Message #3 |
| tn173 | 16883–16950 | PrGeneral Bug |
| tn174 | 16951–17034 | Accessing the Script Manager Print Action Routine |
| tn175 | 17035–17147 | SetLineWidth Revealed |
| tn176 | 17148–17524 | Macintosh Memory Configurations |
| tn177 | 17525–17563 | Problem with WaitNextEvent in MultiFinder 1.0 |
| tn178 | 17564–17903 | Modifying the Standard String Comparison |
| tn179 | 17904–17917 | Setting ioNamePtr in File Manager Calls |
| tn180 | 17918–18320 | MultiFinder Miscellanea |
| tn181 | 18321–18353 | Every Picture [Comment] Tells Its Story |
| tn182 | 18354–18465 | How to Construct Word-Break Tables |
| tn183 | 18466–18561 | Position-Independent PostScript |
| tn184 | 18562–18750 | Notification Manager |
| tn185 | 18751–18776 | OpenRFPerm — What your mother never told you |
| tn186 | 18777–18850 | Lock, the UnlockRange |
| tn187 | 18851–18863 | Don't Look at ioPosOffset |
| tn188 | 18864–18877 | ChangedResource — Too much of a good thing |
| tn189 | 18878–19076 | Version Territory |
| tn190 | 19077–19098 | Working Directories and MultiFinder |
| tn191 | 19099–19212 | Font Names |
| tn192 | 19213–19322 | Surprises in LaserWriter 5.0 and Newer |
| tn193 | 19323–19380 | So Many Bitmaps, So Little Time |
| tn194 | 19381–19439 | WMgrPortability |
| tn195 | 19440–19462 | ASP and AFP Description Discrepancies |
| tn196 | 19463–19492 | 'CDEF' Parameters and Bugs |
| tn197 | 19493–19581 | Chooser Enhancements |
| tn198 | 19582–19730 | Font/DA Mover, Styled Fonts, and 'NFNT's |
| tn199 | 19731–19755 | KillNBP Clarification |
| tn200 | 19756–20331 | MPW 2.0.2 Bugs |
| tn201 | 20332–end | ReadPacket Clarification |

---

## qa405.html — Q&A Stack 4.0.5

**~4,100 lines.** 295 Q&A entries organized by category. Problem-focused: specific hardware constraints, compatibility issues, configuration details. Good for when you hit a specific problem.

### Categories (Table of Contents)

| Category | TOC Line | Content Lines | Topics |
|----------|----------|---------------|--------|
| Devices & Hardware | 14 | 1065–2160 | A/ROSE, ADB, CD-ROM, Custom ICs, Device Manager, Disk Driver, Display/Video, Electrical Issues, Input Devices, Interrupt Handling, IOPs, NuBus |
| Graphics, Imaging & Printing | 231 | 2160–2520 | Color Manager, Fonts, Palette Manager, Picture utilities, Printing |
| Networking & Communications | 388 | 2520–2780 | AppleTalk, Serial, Ethernet |
| Operating System | 488 | 2780–3200 | File Manager, Memory Manager, Process Manager, Gestalt |
| Platforms & Tools | 653 | 3200–3600 | MPW, MacApp, HyperCard, ResEdit |
| Toolbox & IAC | 874 | 3600–end | Dialog Manager, Menu Manager, Window Manager, Apple Events |

### Key Q&A Entries for Emulator Development

| ID | Lines | Topic |
|----|-------|-------|
| qa6–qa20 | 1136–1355 | Apple Desktop Bus — ADB init, addressing, timing, power, LEDs, conflict resolution |
| qa39–qa40 | 1589–1606 | Custom ICs — RBV chip VIA emulation, IIci/IIsi video |
| qa42–qa49 | 1607–1852 | Device Manager — gdRefNum, driver resources, IODone, journaling |
| qa50–qa59 | 1853–2037 | Disk Driver — System 7 compat, floppy access, RAM disk, ejection, 800K sector mapping |
| qa65–qa87 | 2083–2378 | Display & Video — video RAM, CLUT, multiple displays, NuBus video, slot configuration ROM |
| qa88–qa97 | 2379–2517 | Electrical Issues — Power Manager, ESD, power specs, auto-restart |
| qa105 | 2617–2645 | How the Macintosh mouse/cursor mechanism works |
| qa106–qa123 | 2646–2822 | Interrupt Handling — VIA2 timer, VBL tasks, slot interrupts, interrupt time restrictions |

---

## hin202.html — Human Interface Notes 2.0.2

**~978 lines.** 14 sections on Macintosh UI guidelines. Relevant when building applications to run in the emulator, not for emulator internals.

| ID | Lines | Title |
|----|-------|-------|
| hin000 | 22–50 | Welcome to Human Interface Notes |
| hin001 | 51–175 | User Observation — Guidelines for Apple Developers |
| hin002 | 176–240 | Design Principles of On-Line Help and Tutorial |
| hin003 | 241–308 | Movable Modal Dialogs |
| hin004 | 309–370 | The "Cancel" Copy |
| hin005 | 371–460 | The Zoom Box Demystified |
| hin006 | 461–537 | Window Positions |
| hin007 | 538–610 | Keyboard Equivalents |
| hin008 | 611–650 | Alert Box Guidelines |
| hin009 | 651–705 | Pop-Up Menus |
| hin010 | 706–755 | Mixed Setting Indicator for Controls |
| hin011 | 756–840 | Indicating Mixed Formats in Menus |
| hin012 | 841–925 | Specifying a Folder |
| hin013 | 926–end | Unavailable Document Fonts |
