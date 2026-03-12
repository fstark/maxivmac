#!/usr/bin/env python3
"""Transform scc.h and scc.cpp to wrap SCC in SCCDevice class.

Strategy: Since SCC has ~3000 lines and complex conditional compilation,
we keep helper functions as file-scope statics that access state through
g_scc global pointer. The SCCDevice class owns the state and provides
the Device interface. Public methods delegate internally.
"""
import os, re

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---- scc.h ----
scc_h = r"""/*
	SCCEMDEV.h

	Copyright (C) 2004 Philip Cummins, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

#pragma once

#include "devices/device.h"
#include <cstdint>

class SCCDevice : public Device {
public:
	// Device interface
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override {} // SCC has no separate zap
	void reset() override;
	const char* name() const override { return "SCC"; }

	bool interruptsEnabled();

#if EmLocalTalk
	void localTalkTick();
#endif
};

// Global singleton pointer (for backward compatibility during migration)
extern SCCDevice* g_scc;

// Backward-compatible free function API (forwards to g_scc)
extern void SCC_Reset(void);
extern uint32_t SCC_Access(uint32_t Data, bool WriteMem, uint32_t addr);
extern bool SCC_InterruptsEnabled(void);

#if EmLocalTalk
extern void LocalTalkTick(void);
#endif
"""

with open(os.path.join(BASE, "src", "devices", "scc.h"), "w") as f:
    f.write(scc_h.lstrip("\n"))
print("scc.h written")


# ---- Transform scc.cpp ----
# Read original
with open(os.path.join(BASE, "src", "devices", "scc.cpp"), "r") as f:
    lines = f.readlines()

out = []
skip_until_extern_semicolon = False
in_struct_channel = False
in_struct_scc = False
brace_depth = 0

i = 0
while i < len(lines):
    line = lines[i]
    stripped = line.strip()

    # Add g_scc declaration and note after include of scc.h
    if stripped == '#include "devices/scc.h"':
        out.append(line)
        out.append("\n")
        out.append("/* Global singleton */\n")
        out.append("SCCDevice* g_scc = nullptr;\n")
        i += 1
        continue

    # Remove the static SCC_Ty SCC; declaration
    if stripped == "static SCC_Ty SCC;":
        out.append("/* SCC state is now in SCCDevice (g_scc->scc_) */\n")
        i += 1
        continue

    # Replace SCC. with g_scc->scc_.
    # But NOT in strings or comments where it's part of something else
    modified = line.replace("SCC.", "g_scc->scc_.")
    # Fix any over-replacements in string literals like "SCC sending"
    # The pattern SCC. only appears as struct access, string uses are like "SCC " not "SCC."
    # But check for g_scc->scc_. in dbglog strings - those should be OK since
    # the original code uses SCC.MIE etc. not "SCC." in strings

    # Replace the 4 public functions to be class methods + forwarding
    # We'll handle this at the end by appending forwarding functions

    # Replace `extern bool SCC_InterruptsEnabled(void)` function definition
    if stripped.startswith("extern bool SCC_InterruptsEnabled(void)"):
        out.append("bool SCCDevice::interruptsEnabled()\n")
        i += 1
        # next line should be {
        if i < len(lines) and lines[i].strip() == "{":
            out.append("{\n")
            i += 1
            # next line is return SCC.MIE -> return scc_.MIE
            if i < len(lines):
                out.append("\treturn scc_.MIE;\n")
                i += 1  # skip original return line
                # next is }
                if i < len(lines) and lines[i].strip() == "}":
                    out.append("}\n")
                    i += 1
        continue

    # Replace void SCC_Reset(void) function definition
    if stripped == "void SCC_Reset(void)":
        out.append("void SCCDevice::reset()\n")
        modified_line = True
        i += 1
        # Copy the rest of the function body, replacing SCC. refs
        while i < len(lines):
            fline = lines[i]
            fline = fline.replace("SCC.", "scc_.")
            # Replace internal calls
            fline = fline.replace("SCC_InitChannel(", "SCC_InitChannel(g_scc, ")
            fline = fline.replace("SCC_ResetChannel(", "SCC_ResetChannel(g_scc, ")
            # Hmm this won't work well. Let me use a different approach.
            out.append(fline)
            i += 1
            if fline.strip() == "}" and not any(c == '{' for c in fline[:fline.index('}')]):
                # Check if this is the closing brace
                break
        continue

    # Actually, let me take a step back. The approach of keeping helpers as
    # file-scope statics that access g_scc-> is simpler.
    # I'll just do these replacements:
    # 1. SCC. -> g_scc->scc_.
    # 2. Public functions become class methods + forwarding stubs
    # 3. Static LocalTalk variables -> g_scc->member

    out.append(modified)
    i += 1

# Hmm, this approach is getting complex. Let me restart with a cleaner strategy.

# Actually I realize the above approach is getting messy with the line-by-line
# processing. Let me use a full-text replacement approach instead.

print("(Restarting with full-text approach...)")
