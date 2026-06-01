/*
	bp_power.h — Power-off breakpoint support
*/
#pragma once

// Called when guest requests power-off. Fires Kind::PowerOff breakpoints.
void CheckPowerOffBreakpoints();
