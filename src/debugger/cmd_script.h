// Script commands — public declarations for wait/timeout/fail.
#pragma once

#include <cstdint>

using ScaledCycleCount = uint64_t;

// Check all scriptOwned breakpoints for timeout expiry.
// Called from the tick handler (60 Hz).
void CheckScriptTimeouts();

// Get/set the default timeout budget for wait commands.
ScaledCycleCount ScriptDefaultTimeout();
