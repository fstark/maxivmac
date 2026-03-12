/*
	wire_ids.h

	Superset of all wire IDs across all Mac models.
	Not all wires are used on every model — unused wires simply
	have no registered callbacks and cost nothing.

	Part of Phase 5: Multi-Model Support & Runtime Configuration.
*/

#pragma once

enum WireID {
	// Sound (compact Macs: directly driven by VIA1)
	Wire_SoundDisable,
	Wire_SoundVolb0,
	Wire_SoundVolb1,
	Wire_SoundVolb2,

	// VIA1 Port A (active-low pin names, bit 0–7)
	Wire_VIA1_iA0,     // Plus: unused. II: overlay-related. PB100: PMU bus bit 0
	Wire_VIA1_iA1,     // PB100: PMU bus bit 1
	Wire_VIA1_iA2,     // PB100: PMU bus bit 2
	Wire_VIA1_iA3,     // SCCvSync
	Wire_VIA1_iA4,     // MemOverlay
	Wire_VIA1_iA5,     // IWMvSel
	Wire_VIA1_iA6,     // PB100: PMU bus bit 6
	Wire_VIA1_iA7,     // SCCwaitrq

	// VIA1 Port B (bit 0–7)
	Wire_VIA1_iB0,     // RTCdataLine
	Wire_VIA1_iB1,     // RTCclock
	Wire_VIA1_iB2,     // RTCunEnabled
	Wire_VIA1_iB3,     // II: ADB_Int. Plus: unused.
	Wire_VIA1_iB4,     // II: ADB_st0. Plus: unused.
	Wire_VIA1_iB5,     // II: ADB_st1. Plus: unused.
	Wire_VIA1_iB6,     // PB100: PMU
	Wire_VIA1_iB7,     // Sound compat / unused

	// VIA1 CB2 / CA pulse lines
	Wire_VIA1_iCB2,    // II: ADB_Data. Plus: Keyboard data.

	// VIA2 (Mac II family only)
	Wire_VIA2_InterruptRequest,
	Wire_VIA2_iA0,     // VBL interrupt slot
	Wire_VIA2_iA1,
	Wire_VIA2_iA2,
	Wire_VIA2_iA3,
	Wire_VIA2_iA4,
	Wire_VIA2_iA5,
	Wire_VIA2_iA6,     // Addr32 related
	Wire_VIA2_iA7,     // Addr32 related
	Wire_VIA2_iB0,
	Wire_VIA2_iB1,
	Wire_VIA2_iB2,     // PowerOff
	Wire_VIA2_iB3,     // Addr32
	Wire_VIA2_iB4,
	Wire_VIA2_iB5,
	Wire_VIA2_iB6,
	Wire_VIA2_iB7,
	Wire_VIA2_iCB2,

	// VIA1 interrupt output
	Wire_VIA1_InterruptRequest,

	// SCC
	Wire_SCCInterruptRequest,

	// ADB (Mac II / SE family)
	Wire_ADBMouseDisabled,

	// Video (Mac II only)
	Wire_VBLinterrupt,
	Wire_VBLintunenbl,

	// PMU (PB100 only)
	Wire_PMU_FromReady,
	Wire_PMU_ToReady,

	kNumWires
};

// ---- Backward-compatible aliases ----
// These map the old descriptive wire names to the neutral port-pin names.
// All wire *access* should go through WireBus::get()/set(); these constants
// are just for readability and to ease the migration.

// Wire ID aliases for common signal names (used in machine.cpp, adb.cpp, etc.)
static constexpr int Wire_VIA1_iA4_MemOverlay    = Wire_VIA1_iA4;
static constexpr int Wire_VIA1_iA5_IWMvSel       = Wire_VIA1_iA5;
static constexpr int Wire_VIA1_iA7_SCCwaitrq     = Wire_VIA1_iA7;
static constexpr int Wire_VIA1_iA3_SCCvSync      = Wire_VIA1_iA3;
static constexpr int Wire_VIA1_iB0_RTCdataLine   = Wire_VIA1_iB0;
static constexpr int Wire_VIA1_iB1_RTCclock      = Wire_VIA1_iB1;
static constexpr int Wire_VIA1_iB2_RTCunEnabled  = Wire_VIA1_iB2;
static constexpr int Wire_VIA1_iB3_ADB_Int       = Wire_VIA1_iB3;
static constexpr int Wire_VIA1_iB4_ADB_st0       = Wire_VIA1_iB4;
static constexpr int Wire_VIA1_iB5_ADB_st1       = Wire_VIA1_iB5;
static constexpr int Wire_VIA1_iCB2_ADB_Data     = Wire_VIA1_iCB2;
static constexpr int Wire_VIA2_iA6_unknown        = Wire_VIA2_iA6;
static constexpr int Wire_VIA2_iA7_unknown        = Wire_VIA2_iA7;
static constexpr int Wire_VIA2_iB2_PowerOff      = Wire_VIA2_iB2;
static constexpr int Wire_VIA2_iB3_Addr32        = Wire_VIA2_iB3;
static constexpr int Wire_VIA2_iB7_unknown        = Wire_VIA2_iB7;
static constexpr int Wire_VIA2_iCB2_unknown       = Wire_VIA2_iCB2;

// Old sound wire aliases
static constexpr int Wire_unknown_SoundDisable   = Wire_SoundDisable;
static constexpr int Wire_unknown_SoundVolb0     = Wire_SoundVolb0;
static constexpr int Wire_unknown_SoundVolb1     = Wire_SoundVolb1;
static constexpr int Wire_unknown_SoundVolb2     = Wire_SoundVolb2;

// Old VIA1 port aliases (the _unknown suffix forms)
static constexpr int Wire_VIA1_iA0_unknown       = Wire_VIA1_iA0;
static constexpr int Wire_VIA1_iA1_unknown       = Wire_VIA1_iA1;
static constexpr int Wire_VIA1_iA2_unknown       = Wire_VIA1_iA2;
static constexpr int Wire_VIA1_iB7_unknown       = Wire_VIA1_iB7;
