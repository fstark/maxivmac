# Debugger Startup Script — Detailed Design

Execute a `.dbg` script file server-side when the debugger starts,
before the first interactive prompt.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. User-Facing Behavior

```
maxivmac --debugger --dbg-script=shared.dbg disk.hfs
maxivmac --debugserver --dbg-script=debug_create.dbg disk.hfs
```

`--dbg-script=FILE` loads FILE and executes every line as a debugger
command **immediately after** `Debugger::create()` completes and
**before** the first `commandLoop()` prompt.  Multiple `--dbg-script`
flags are processed in order.

Additionally, a `source` command is added to the interactive prompt
so scripts can be loaded at any time:

```
(dbg) source shared.dbg
```

Script parsing rules (same as existing client-side scripts in
`dbg_client.cpp`):

- Lines starting with `#` are comments.
- Empty / whitespace-only lines are skipped.
- The `commands … end` block is handled by `CmdCommands`, which
  already reads lines from `DbgIO` — no special script-side logic
  needed because `sourceFile()` feeds lines through the same
  `executeCommands()` path.

---

## 2. Public Interface

One new free function in `debugger.h` and one new command handler:

```cpp
// Load and execute every line of a .dbg script file.
// Prints an error via dbg.io() if the file cannot be opened.
// Returns false on file-open failure, true otherwise.
bool SourceFile(Debugger &dbg, std::string_view path);
```

New interactive command:

```
source <path>
```

---

## 3. Reuse: `executeCommands()`

`Debugger::executeCommands(const std::vector<std::string> &cmds)`
already does tokenize → dispatch → bail-on-state-change for
breakpoint auto-commands.  `SourceFile` should just read lines from
disk, strip comments/blanks, and hand the result to
`executeCommands()`.

The only gap: `executeCommands()` doesn't skip `#` comment lines
(the tokenizer turns `#` into an operator token, which fails
dispatch).  Fix: add a one-line guard at the top of the loop in
`executeCommands()`:

```cpp
if (auto p = line.find_first_not_of(" \t"); p != std::string::npos && line[p] == '#')
    continue;
```

This gives us `#` comment support everywhere — scripts, breakpoint
auto-commands, **and** paste into the interactive prompt.

---

## 4. Integration Points

### 4.1 — LaunchConfig (src/core/config_loader.h, L63 area)

Add one field:

```cpp
std::vector<std::string> dbgScripts;   // --dbg-script=FILE (repeatable)
```

### 4.2 — ParseCommandLine (src/core/config_loader.cpp)

Add parsing for `--dbg-script=`:

```cpp
if (strncmp(arg, "--dbg-script=", 13) == 0)
{
    lc.dbgScripts.emplace_back(arg + 13);
    continue;
}
```

Add help text line:

```
"  --dbg-script=FILE  Execute .dbg script at debugger startup (repeatable)\n"
```

### 4.3 — ProgramEarlyInit (src/core/main.cpp, after Debugger::create())

After the debugger is created, source each script:

```cpp
for (auto &script : s_launchConfig.dbgScripts)
    SourceFile(*Debugger::instance(), script);
```

### 4.4 — Command table (src/debugger/debugger.cpp)

Add a `source` entry to `s_commands[]`:

```cpp
{"source", "so", CmdSource, "Execute commands from a file",
 "source <path>\n  Read and execute debugger commands from a file.\n"},
```

### 4.5 — CmdSource handler (src/debugger/cmd_exec.cpp)

```cpp
void CmdSource(Debugger &dbg, const std::vector<Token> &args)
{
    if (args.empty() || args[0].kind == Token::Kind::End)
    {
        dbg.io().write("Usage: source <path>\n");
        return;
    }
    SourceFile(dbg, args[0].text);
}
```

### 4.6 — SourceFile implementation (src/debugger/debugger.cpp)

```cpp
bool SourceFile(Debugger &dbg, std::string_view path)
{
    std::string p(path);
    std::FILE *f = std::fopen(p.c_str(), "r");
    if (!f)
    {
        dbg.io().write("source: cannot open '%s'\n", p.c_str());
        return false;
    }

    std::vector<std::string> cmds;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), f))
    {
        size_t len = std::strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        cmds.emplace_back(buf);
    }
    std::fclose(f);

    dbg.executeCommands(cmds);
    return true;
}
```

That's it — blank lines and `#` comments are handled by
`executeCommands()`, so `SourceFile` just reads lines and forwards
them.

`commands … end` blocks in scripts won't work in this first cut
because `CmdCommands` reads subsequent lines from `DbgIO`
(stdin/socket), not from the vector.  The existing client-side
`--script=FILE` over socket handles that case.

---

## 5. Files Changed

| File | Change |
|------|--------|
| `src/core/config_loader.h` | Add `dbgScripts` field to `LaunchConfig` |
| `src/core/config_loader.cpp` | Parse `--dbg-script=`, add help text |
| `src/core/main.cpp` | Call `SourceFile()` after `Debugger::create()` |
| `src/debugger/debugger.h` | Declare `SourceFile()` |
| `src/debugger/debugger.cpp` | Implement `SourceFile()`, add `source` command + `CmdSource` forward decl |
| `src/debugger/cmd_exec.cpp` | Implement `CmdSource` handler |

No new files.  ~25 lines of new code total.

---

## 6. Testing

Manual:

```bash
# Single script
./maxivmac --debugger --dbg-script=shared.dbg MacII.ROM disk.hfs

# Multiple scripts
./maxivmac --debugger --dbg-script=shared.dbg --dbg-script=debug_create.dbg MacII.ROM disk.hfs

# Interactive source
(dbg) source shared.dbg
```

Verify:
1. Script commands execute and print `+ <line>` echo.
2. Breakpoints/traces from script are active.
3. `run`/`continue` in script stops sourcing and starts execution.
4. Bad path prints error, debugger remains at prompt.
5. Comments and blank lines are silently skipped.
