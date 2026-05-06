// Text breakpoint matching — fires when trap params contain target text.
#pragma once

#include <string_view>

// Called from TrapTracer::formatParam() when a Text-typed param is formatted.
// Checks incoming text against all active text breakpoints and fires any that match.
// Thread safety: called from the CPU thread only.
void ScriptCaptureText(std::string_view utf8Text, std::string_view trapName);

// Toggle live text display to console.
void ScriptShowTextSet(bool on);
bool ScriptShowTextGet();
