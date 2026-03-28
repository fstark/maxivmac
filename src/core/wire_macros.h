/*
	Wire accessor macros — backward-compatible #define aliases for
	the superset wire enum in wire_ids.h.
*/

#pragma once

/* the Wire variables are 1/0, not true/false */

/* Wire IDs now come from the superset enum in wire_ids.h.
   The #define aliases below provide backward compatibility. */
#include "core/wire_ids.h"

#define SoundDisable (Wires[Wire_SoundDisable])
#define SoundVolb0 (Wires[Wire_SoundVolb0])
#define SoundVolb1 (Wires[Wire_SoundVolb1])
#define SoundVolb2 (Wires[Wire_SoundVolb2])

#define VIA1_iA0 (Wires[Wire_VIA1_iA0])
#define VIA1_iA1 (Wires[Wire_VIA1_iA1])
#define VIA1_iA2 (Wires[Wire_VIA1_iA2])
#define VIA1_iA3 (Wires[Wire_VIA1_iA3])
#define VIA1_iA4 (Wires[Wire_VIA1_iA4])
#define VIA1_iA5 (Wires[Wire_VIA1_iA5])
#define VIA1_iA6 (Wires[Wire_VIA1_iA6])
#define VIA1_iA7 (Wires[Wire_VIA1_iA7])

#define VIA1_iB0 (Wires[Wire_VIA1_iB0])
#define VIA1_iB1 (Wires[Wire_VIA1_iB1])
#define VIA1_iB2 (Wires[Wire_VIA1_iB2])
#define VIA1_iB3 (Wires[Wire_VIA1_iB3])
#define VIA1_iB4 (Wires[Wire_VIA1_iB4])
#define VIA1_iB5 (Wires[Wire_VIA1_iB5])
#define VIA1_iB6 (Wires[Wire_VIA1_iB6])
#define VIA1_iB7 (Wires[Wire_VIA1_iB7])

#define VIA1_iCB2 (Wires[Wire_VIA1_iCB2])

/* Named signal aliases */
#define MemOverlay (Wires[Wire_MemOverlay])
#define IWMvSel (Wires[Wire_VIA1_iA5])
#define SCCwaitrq (Wires[Wire_VIA1_iA7])
#define RTCdataLine (Wires[Wire_VIA1_iB0])
#define RTCclock (Wires[Wire_VIA1_iB1])
#define RTCunEnabled (Wires[Wire_VIA1_iB2])
#define ADB_Int (Wires[Wire_VIA1_iB3])
#define ADB_st0 (Wires[Wire_VIA1_iB4])
#define ADB_st1 (Wires[Wire_VIA1_iB5])
#define ADB_Data (Wires[Wire_VIA1_iCB2])
#define Addr32 (Wires[Wire_VIA2_iB3])

/* VIA2 aliases */
#define VIA2_iA0 (Wires[Wire_VIA2_iA0])
#define VIA2_iA6 (Wires[Wire_VIA2_iA6])
#define VIA2_iA7 (Wires[Wire_VIA2_iA7])
#define VIA2_iB2 (Wires[Wire_VIA2_iB2])
#define VIA2_iB3 (Wires[Wire_VIA2_iB3])
#define VIA2_iB7 (Wires[Wire_VIA2_iB7])
#define VIA2_iCB2 (Wires[Wire_VIA2_iCB2])

#define VIA2_InterruptRequest (Wires[Wire_VIA2_InterruptRequest])
#define VIA1_InterruptRequest (Wires[Wire_VIA1_InterruptRequest])
#define SCCInterruptRequest (Wires[Wire_SCCInterruptRequest])
#define ADBMouseDisabled (Wires[Wire_ADBMouseDisabled])
#define Vid_VBLinterrupt (Wires[Wire_VBLinterrupt])
#define Vid_VBLintunenbl (Wires[Wire_VBLintunenbl])

/* ChangeNtfy aliases — map wire names to callback function names */
#define VIA1_iA4_ChangeNtfy MemOverlay_ChangeNtfy
#define VIA2_iA7_ChangeNtfy Addr32_ChangeNtfy
#define VIA2_iA6_ChangeNtfy Addr32_ChangeNtfy
#define VIA2_iB2_ChangeNtfy PowerOff_ChangeNtfy
#define VIA2_iB3_ChangeNtfy Addr32_ChangeNtfy
#define VIA2_interruptChngNtfy VIAorSCCinterruptChngNtfy
#define VIA1_interruptChngNtfy VIAorSCCinterruptChngNtfy
#define SCCinterruptChngNtfy VIAorSCCinterruptChngNtfy
