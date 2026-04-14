---
name: code-commenter
description: "Add or improve comments in C/C++ source files following project conventions. Use when: commenting code, adding documentation to headers or implementation files, reviewing comment quality, documenting a module, adding file headers."
argument-hint: "Path to file(s) or directory to comment, e.g. src/core/globglue.cpp"
---

# Code Commenter

Add clear, concise comments to C++ source files following project
conventions in [STYLE.md](docs/STYLE.md) and [NAMING.md](docs/NAMING.md).
This is a **C++23 project** — use modern terminology (e.g. "returns",
not "returns a pointer to"; reference `std::string_view` not
`const char *` in comment text when describing interfaces).

Prefer `//` for single-line annotations and `/* */` for multi-line
blocks.  Never use Doxygen markup (`/** */`, `@param`, `@return`,
`\brief`).

## Scope

These rules apply to all code under `src/` **except**:

| Directory | Guidance |
|-----------|----------|
| `src/cpu/` | Match surrounding style.  Do not mass-comment legacy instruction handlers. |
| `src/macsrc/` | Classic Macintosh source — do not modify. |
| Third-party code | SoftFloat, Bochs FPU, ImGui — keep upstream style. |

## Ground Rules

- **Read before writing.** Always read the file (and its counterpart header/implementation) before adding comments. Understand what the code does — do not guess.
- **Never paraphrase the code.** Comments explain *why* or *what role* something plays, not *what the syntax does*. `// increment i` on `i++` is forbidden.
- **Preserve existing good comments.** Do not delete or rewrite comments that are already accurate and clear. Only add missing ones or fix wrong ones.
- **Match surrounding style.** If the file uses `/* */` everywhere, do not introduce `//` blocks (and vice versa). When in doubt, use `//` comments.
- **Do not comment trivial code.** Getters, setters, simple constructors, one-line forwarding functions, and obvious boilerplate need no implementation comment.
- **Keep English terse and direct.** No filler words. Write like a telegraph, not an essay.
- **Verify existing comment for correctness.** If you see a comment that is wrong, misleading or does not follow the rules, fix it. Do not delete it without replacement unless it is clearly redundant.

## Comment Placement Rules

### 1. File Header (every `.h`, `.c`, `.cpp` file)

Place a block comment at the very top of the file (before `#pragma once` / include guards). Maximum 8 lines. Structure:

```c
/*
	<Module Name> — <one-line role summary>

	<Optional 1-5 lines of context: what hardware/subsystem this models,
	 key design decisions, relationship to other modules, or non-obvious
	 constraints. Omit if the one-liner is sufficient.>
*/
```

Rules:
- First line: module name + dash + role. Keep it under 80 characters.
- Use tab indentation inside the block, matching the project convention.
- Do not list authors, dates, copyright, or license here.
- If the file already has a correct header, leave it alone.

### 2. Section Markers (headers and large implementation files)

Group related declarations or definitions under section markers:

```c
/* --- Interrupt handling --- */
```

Use these sparingly — only when a file has 3+ distinct logical sections.

### 3. Class / Struct / Enum Comment (headers)

Place a comment block immediately before the declaration. Exactly 2-3 lines. Describe *what it represents*, not how it is implemented.

```c
/*
	Represents the Macintosh IWM (Integrated Woz Machine) floppy controller.
	Mediates between the emulated CPU bus and disk image I/O.
*/
struct IWM {
```

For small enums or trivial structs, a single `//` line suffices:

```c
// Supported display depths for screen emulation.
enum ScreenDepth { ... };
```

### 4. Function and Method Declarations (headers)

Place a `//` comment on the line(s) immediately before the declaration. One or two lines maximum. Describe *what* the function does, not *how*.

```c
// Mount a disk image and notify the guest OS.
bool MountDisk(std::string_view path);

// Convert a Mac-epoch timestamp to Unix time.  Returns -1 on overflow.
int32_t MacToUnix(uint32_t macTime);
```

Rules:
- Start with a verb (Mount, Convert, Return, Initialize, …).
- Mention return semantics only if non-obvious (error codes, null, sentinel values).
- If a group of closely related functions is already under a section marker that explains them, individual one-liners may be omitted for trivially named functions (e.g. `GetWidth()`, `GetHeight()`).

### 5. Function Definitions (implementation files)

**Non-trivial functions:** Place a `/* */` block comment before the definition. Describe *how* the function works — the algorithm, the key steps, any tricky aspects. Typically 2-6 lines.

```c
/*
	Walk the ROM patch table and apply each patch by overwriting the
	target address.  Patches are applied in order; later entries may
	override earlier ones.  Skip any patch whose target falls outside
	the current ROM size.
*/
void ApplyROMPatches(uint8_t *rom, uint32_t romSize)
{
```

**Trivial functions:** No comment needed. A function is trivial if its body is ≤ 5 lines of straightforward logic with no non-obvious side effects.

**Static / file-local helpers:** A single `//` line is enough if the name is not self-documenting.

### 6. Inline Comments

Use sparingly, only for:
- Non-obvious magic numbers: `mask &= 0x1F; /* IWM register is 5 bits */`
- Hardware quirks or spec references: `/* See Inside Macintosh vol. III, p. 36 */`
- Workarounds: `/* gcc miscompiles this at -O2 without the volatile */`

Never add inline comments that merely restate the code.

## Procedure

When asked to comment a file or set of files:

1. **Read the target file(s)** fully. If it is a header, also read its implementation (and vice versa) for context.
2. **Read [STYLE.md](docs/STYLE.md)** (Comments section) and [NAMING.md](docs/NAMING.md) if you have not already in this session.
3. **Assess what is missing.** Check each rule above and note which comments are absent or wrong.
4. **Write comments** following the rules. Make edits in the file directly — do not produce a separate document.
5. **Review.** Re-read each added comment and ask: "Does this say something the code does not already say?" Delete any that fail this test.

## Anti-Patterns to Avoid

| Bad | Why | Instead |
|-----|-----|---------|
| `/* This function returns the value */` | Restates code | Explain *what* value and *why* |
| `// TODO: clean up` without context | Vague, useless | `// TODO: replace linear scan with hash lookup for large ROM tables` |
| `/** @brief Gets X @return X */` | Doxygen noise, says nothing | `// Return X.` |
| Long comment blocks on simple getters | Over-commenting | Omit entirely |
| `// --------------------` decorative lines | Visual clutter | Use `/* --- Section --- */` only for logical grouping |
| Comments that describe *the language* | Patronizing | Never write `// declare a variable` |
