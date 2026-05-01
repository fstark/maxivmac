---
name: glossary
description: "Add, update, check, or suggest terms in the project glossary (docs/GLOSSARY.md). Use when: adding a term, auditing a file or directory for terminology drift, or discovering candidate terms to add."
argument-hint: "Operation + target, e.g. 'add IWM', 'check src/devices/', 'suggest docs/WD_DESIGN.md'"
---

# Glossary

Maintain the project glossary at
[docs/GLOSSARY.md](docs/GLOSSARY.md) — the single source of truth for
all project terminology.

## Operations

The first word of the argument selects the operation:

| Operation | Argument | What it does |
|-----------|----------|--------------|
| **add** | term name [+ optional definition] | Add or update a glossary entry |
| **check** | file or directory path | Report terminology violations and candidates |
| **suggest** | file or directory path | Propose new glossary entries |

If the argument doesn't start with a known operation, treat it as
**add**.

## Glossary Schema

Every entry follows this format:

```markdown
### Term Name

Definition (1–3 sentences, present tense, what it *is*).

Code: `Symbol` in `path/file.h` (optional)
Examples: Term1, Term2 (optional — only domain terms)
See also: Other Term (optional)
Avoid: rejected synonym, another (optional)
```

Rules:
- **Present tense only.**  No roadmap content ("planned for v1.1"),
  no version annotations.  Describe what the term means *now*.
- **Definition is prose**, not bullet points.  1–3 sentences.
- **Code**, **Examples**, **See also**, **Avoid** are all optional.
  Omit any that don't apply.
- **Avoid** lists rejected synonyms — words an agent should never use
  for this concept.

## Sections

The glossary has two sections:

| Section | What belongs here | Litmus test |
|---------|-------------------|-------------|
| **maxivmac** | Architecture, features, build/test/ship concepts | "We named this" |
| **Classic Mac** | Apple hardware chips, Mac OS concepts, system software | "You could find this in Inside Macintosh" |

Entries are **alphabetical within each section**.

If unsure which section: if the term would exist without maxivmac, it's
Classic Mac.  Otherwise it's maxivmac.

## Operation: add

1. **Read** `docs/GLOSSARY.md`.
2. **Check for duplicates.**  If the term already exists, show the
   existing entry and ask if the user wants to update it.
3. **If no definition was provided**, research the codebase:
   - Search for the term in source files, headers, and comments.
   - Read relevant files to understand what it is.
   - Draft a definition following the schema.
   - **Show the draft to the user for confirmation before writing.**
4. **Pick the correct section** using the litmus test above.
5. **Insert alphabetically** within that section.
6. **Fill optional fields** where applicable:
   - Code: if there's a corresponding class, struct, enum, or file.
   - Examples: if the term has named instances that are domain terms.
   - Avoid: if there are known synonyms that cause confusion.

## Operation: check

Scan the target file(s) for terminology drift.  Report; do not fix.

1. **Read** `docs/GLOSSARY.md` to load all terms and their Avoid
   lists.
2. **Read** the target file or directory.
   - For markdown files: check all prose.
   - For source files: check comments *and* identifiers.
3. **Report** three categories:
   - **Violations** — uses of an Avoid synonym where the preferred
     term should appear.  Include file, line, the rejected word, and
     the preferred term.
   - **Mismatches** — identifiers or descriptions that contradict a
     glossary definition (e.g. a comment calling the IWM a "floppy
     drive" when the glossary says "floppy disk controller").
   - **Candidates** — domain-specific terms used in the file that
     aren't in the glossary yet.  Only flag terms that appear
     multiple times or seem important.
4. **Format** the report as a markdown checklist grouped by category.

### Scope

- Accepts any file or directory path.
- **Exclusions** (do not check):
  - `src/cpu/` — frozen legacy 68K interpreter.
  - `src/macsrc/` — classic Macintosh source.
  - `libs/` — third-party code (ImGui, SoftFloat, Bochs, doctest).
  - `reference/` — reference branch.

When given a directory, check every `.md`, `.h`, `.cpp` file
recursively (excluding the above).

## Operation: suggest

Scan the target and propose fully-formed glossary entries.

1. **Read** `docs/GLOSSARY.md` to know what's already defined.
2. **Read** the target file(s).
3. **Identify** domain-specific terms that:
   - Appear multiple times or in prominent positions (headings,
     function names, class names).
   - Are not already in the glossary.
   - Are not self-explanatory (skip "file", "buffer", "loop").
4. **Draft** a complete entry for each candidate, following the
   schema and section rules.
5. **Present** the candidates to the user for approval.  Do not
   write to the glossary without confirmation.

## Ground Rules

- **Always read the glossary first.**  Every operation starts by
  reading `docs/GLOSSARY.md`.
- **Never write without confirmation** for add/suggest.  Show the
  draft, get approval, then write.
- **Preserve existing entries.**  Do not delete or rewrite entries
  that weren't requested.
- **Keep definitions terse.**  If you can say it in one sentence,
  don't use three.
