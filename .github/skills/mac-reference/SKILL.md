---
name: mac-reference
description: "Look up classic Macintosh technical documentation from Inside Macintosh, Technical Notes, and Q&A. Use when: answering questions about Mac hardware internals, Toolbox API, trap numbers, VIA/SCC/IWM/SCSI registers, memory map, system traps, global variables, classic Mac app development, Human Interface Guidelines, or any Macintosh 128K through Mac II technical detail. Also use when: hacking emulator device code and needing to understand the real hardware behavior."
argument-hint: "Topic or question, e.g. 'VIA register layout' or 'how does the SCSI Manager work'"
---

# Macintosh Technical Reference Lookup

Look up classic Apple Macintosh developer documentation to answer questions about hardware internals, Toolbox/OS APIs, system traps, and application development for the Mac 128K through Macintosh II.

## Documentation Set

Four HTML files in `macdocs/tech_doc/`, read with `read_file` using line ranges from the index:

| File | Content | Use For |
|------|---------|---------|
| `im202.html` | Inside Macintosh — 62 chapters, ~61,700 lines | API specs, hardware details, assembly equates, data structures |
| `tn405.html` | Technical Notes — 201 notes, ~20,300 lines | Workarounds, bugs, practical code, file formats |
| `qa405.html` | Q&A Stack — 295 entries, ~4,100 lines | Hardware-specific troubleshooting, constraints |
| `hin202.html` | Human Interface Notes — 14 sections, ~978 lines | UI guidelines for Mac app development |

## Lookup Procedure

### Step 1: Check quick-reference files first

For the most common lookups, pre-extracted files exist:

- `macdocs/ref/HARDWARE.md` — Complete Macintosh Hardware chapter (address space, video, sound, SCC, keyboard, IWM, RTC, SCSI, VIA)
- `macdocs/ref/TRAP_TABLE.md` — System trap numbers → routine names
- `macdocs/ref/GLOBAL_VARS.md` — Low-memory global variable addresses

If the question is about hardware registers, address map, or trap numbers, read the appropriate ref file directly.

### Step 2: Read the index

Read `macdocs/INDEX.md` to find the relevant chapter/section. The index has every chapter listed with its line range. Search for keywords matching the topic.

### Step 3: Read the documentation

Use `read_file` with the exact line range from the index. For large chapters (>500 lines), start with the chapter's table of contents (first 30–50 lines after the chapter heading) to find the specific subsection.

### Step 4: For emulator-specific lookups

Read `macdocs/EMULATOR_MAP.md` to find which documentation sections relate to a specific emulator source file (e.g., `src/devices/via.h` → im029 VIA sections).

## Document Routing

| Question Type | Primary Doc | Secondary |
|---------------|-------------|-----------|
| Hardware registers, address map, timing | im029 (Macintosh Hardware) or `ref/HARDWARE.md` | — |
| Toolbox API signatures, data structures | im202.html (relevant manager chapter) | tn405.html |
| Trap numbers, trap dispatch | im058 (Appendix C) or `ref/TRAP_TABLE.md` | im005 |
| Low-memory globals | im059 (Appendix D) or `ref/GLOBAL_VARS.md` | — |
| Result/error codes | im056 (Appendix A) | — |
| Workaround for a known bug | tn405.html | qa405.html |
| Hardware Q&A (specific problem) | qa405.html | im029 |
| UI guidelines for Mac apps | hin202.html | im003 |
| File/resource format | tn405.html (various format notes) | im037 |
| Serial/SCC | im029 §SCC + im042 (Serial Drivers) | tn088 |
| Disk/floppy/IWM | im029 §Disk + im021 (Disk Driver) | tn036 |
| SCSI | im029 §SCSI + im040 (SCSI Manager) | tn096 |
| Memory management | im030 (Memory Manager) | im004, tn176 |
| Keyboard/ADB | im010 (ADB) + im029 §Keyboard | tn160 |
| Sound | im029 §Sound + im046 (Sound Manager) | im045 |
| Video/display | im029 §Video + im008 (Graphics Devices) | tn120 |

## Interpreting the Content

The HTML files contain preformatted text with minimal markup. Key patterns:

- **Pascal function signatures**: `PROCEDURE FooBar (param: Type; VAR result: Type);`
- **Assembly equates**: `fieldName  EQU  $offset  ;[type] description`
- **Trap macros**: Names like `_GetResource`, `_NewHandle` — the underscore prefix indicates a trap
- **Data structures**: Pascal `TYPE ... RECORD ... END;` declarations with field-by-field documentation
- **Cross-references**: `<a href="#imXXX">` links between chapters; `<a href="tn405.html#tnXXX">` links to tech notes
- **Figure references**: `<img src="im202-figs-N-M.png">` — figures are PNG images, not readable as text

## Quick Device → Chapter Cheat Sheet

| Emulator Device | Key Chapter (im202.html lines) |
|----------------|-------------------------------|
| VIA1/VIA2 | im029 lines 35353–35772 |
| SCC | im029 lines 34948–35035 |
| Keyboard | im029 lines 35036–35170 |
| IWM/Floppy | im029 lines 35171–35288 |
| SCSI | im029 lines 35289–35352 |
| RTC | im029 lines 35289–35352 |
| Video | im029 lines 34806–34871 |
| Sound | im029 lines 34872–34947 |
| ADB | im010 lines 11405–11892 |
| ASC | im045 lines 51122–51848, im046 lines 51849–53035 |
