/*
	Model-independent device/display configuration.

	WantAbnormalReports and EmLocalTalk are now passed as
	compile definitions (-D) from CMakeLists.txt.
*/

#pragma once

#define dbglog_HAVE 1

constexpr int NumDrives = 6;

/* vMacScreenWidth/Height/Depth are now runtime — see platform.h */
/* kROM_Size is now runtime — see MachineConfig::romSize */

constexpr int NumPbufs = 4;
