/*
	machine_config.cpp

	Model-to-feature mapping factory.
	Encodes the configuration that currently lives in CNFUDPIC.h
	and the setup_t tool.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/machine_config.h"
#include "core/wire_ids.h"

// Helper to build the Mac II VIA1 config
static VIAConfig MakeVIA1Config_MacII() {
	VIAConfig v;
	v.oraFloatVal      = 0xBF;
	v.orbFloatVal      = 0xFF;
	v.oraCanIn         = 0x80;  // bit 7 readable
	v.oraCanOut        = 0x3F;  // bits 0-5 writable
	v.orbCanIn         = 0x09;  // bits 0,3 readable
	v.orbCanOut        = 0xB7;  // bits 0-2,4-5,7 writable
	v.ierNever0        = 0x00;
	v.ierNever1        = 0x58;
	v.cb2ModesAllowed  = 0x01;
	v.ca2ModesAllowed  = 0x01;
	v.portAWires = {
		Wire_VIA1_iA0, Wire_VIA1_iA1, Wire_VIA1_iA2, Wire_VIA1_iA3,
		Wire_VIA1_iA4, Wire_VIA1_iA5, Wire_VIA1_iA6, Wire_VIA1_iA7
	};
	v.portBWires = {
		Wire_VIA1_iB0, Wire_VIA1_iB1, Wire_VIA1_iB2, Wire_VIA1_iB3,
		Wire_VIA1_iB4, Wire_VIA1_iB5, Wire_VIA1_iB6, Wire_VIA1_iB7
	};
	v.cb2Wire       = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

// Helper to build the Mac II VIA2 config
static VIAConfig MakeVIA2Config_MacII() {
	VIAConfig v;
	v.oraFloatVal      = 0xFF;
	v.orbFloatVal      = 0xFF;
	v.oraCanIn         = 0x01;  // bit 0 readable
	v.oraCanOut        = 0xC0;  // bits 6-7 writable
	v.orbCanIn         = 0x00;
	v.orbCanOut        = 0x8C;  // bits 2,3,7 writable
	v.ierNever0        = 0x00;
	v.ierNever1        = 0xED;
	v.cb2ModesAllowed  = 0x01;
	v.ca2ModesAllowed  = 0x01;
	v.portAWires = {
		Wire_VIA2_iA0, Wire_VIA2_iA1, Wire_VIA2_iA2, Wire_VIA2_iA3,
		Wire_VIA2_iA4, Wire_VIA2_iA5, Wire_VIA2_iA6, Wire_VIA2_iA7
	};
	v.portBWires = {
		Wire_VIA2_iB0, Wire_VIA2_iB1, Wire_VIA2_iB2, Wire_VIA2_iB3,
		Wire_VIA2_iB4, Wire_VIA2_iB5, Wire_VIA2_iB6, Wire_VIA2_iB7
	};
	v.cb2Wire       = Wire_VIA2_iCB2;
	v.interruptWire = Wire_VIA2_InterruptRequest;
	return v;
}

// Helper for Plus/128K/512K/Twiggy VIA1 config (no VIA2)
static VIAConfig MakeVIA1Config_Plus() {
	VIAConfig v;
	v.oraFloatVal      = 0xFF;
	v.orbFloatVal      = 0xFF;
	v.oraCanIn         = 0x80;
	v.oraCanOut        = 0x7F;
	v.orbCanIn         = 0x00;
	v.orbCanOut        = 0xFF;
	v.ierNever0        = 0x00;
	v.ierNever1        = 0x00;
	v.cb2ModesAllowed  = 0x01;
	v.ca2ModesAllowed  = 0x01;
	v.portAWires = {
		Wire_VIA1_iA0, Wire_VIA1_iA1, Wire_VIA1_iA2, Wire_VIA1_iA3,
		Wire_VIA1_iA4, Wire_VIA1_iA5, Wire_VIA1_iA6, Wire_VIA1_iA7
	};
	v.portBWires = {
		Wire_VIA1_iB0, Wire_VIA1_iB1, Wire_VIA1_iB2, Wire_VIA1_iB3,
		Wire_VIA1_iB4, Wire_VIA1_iB5, Wire_VIA1_iB6, Wire_VIA1_iB7
	};
	v.cb2Wire       = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

// Helper for SE/SEFDHD/Classic VIA1 (similar to II but no VIA2, ADB on VIA1)
static VIAConfig MakeVIA1Config_SE() {
	VIAConfig v;
	v.oraFloatVal      = 0xFF;
	v.orbFloatVal      = 0xFF;
	v.oraCanIn         = 0x80;
	v.oraCanOut        = 0x7F;
	v.orbCanIn         = 0x08;
	v.orbCanOut        = 0xB7;
	v.ierNever0        = 0x00;
	v.ierNever1        = 0x00;
	v.cb2ModesAllowed  = 0x01;
	v.ca2ModesAllowed  = 0x01;
	v.portAWires = {
		Wire_VIA1_iA0, Wire_VIA1_iA1, Wire_VIA1_iA2, Wire_VIA1_iA3,
		Wire_VIA1_iA4, Wire_VIA1_iA5, Wire_VIA1_iA6, Wire_VIA1_iA7
	};
	v.portBWires = {
		Wire_VIA1_iB0, Wire_VIA1_iB1, Wire_VIA1_iB2, Wire_VIA1_iB3,
		Wire_VIA1_iB4, Wire_VIA1_iB5, Wire_VIA1_iB6, Wire_VIA1_iB7
	};
	v.cb2Wire       = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

// PB100 VIA1 config (PMU on port A)
static VIAConfig MakeVIA1Config_PB100() {
	VIAConfig v;
	v.oraFloatVal      = 0xFF;
	v.orbFloatVal      = 0xFF;
	v.oraCanIn         = 0xFF;  // all of port A readable (PMU bus)
	v.oraCanOut        = 0xFF;  // all of port A writable (PMU bus)
	v.orbCanIn         = 0x00;
	v.orbCanOut        = 0xFF;
	v.ierNever0        = 0x00;
	v.ierNever1        = 0x00;
	v.cb2ModesAllowed  = 0x01;
	v.ca2ModesAllowed  = 0x01;
	v.portAWires = {
		Wire_VIA1_iA0, Wire_VIA1_iA1, Wire_VIA1_iA2, Wire_VIA1_iA3,
		Wire_VIA1_iA4, Wire_VIA1_iA5, Wire_VIA1_iA6, Wire_VIA1_iA7
	};
	v.portBWires = {
		Wire_VIA1_iB0, Wire_VIA1_iB1, Wire_VIA1_iB2, Wire_VIA1_iB3,
		Wire_VIA1_iB4, Wire_VIA1_iB5, Wire_VIA1_iB6, Wire_VIA1_iB7
	};
	v.cb2Wire       = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

MachineConfig MachineConfigForModel(MacModel model)
{
	MachineConfig c;
	c.model = model;

	switch (model) {
		case MacModel::Twig43:
		case MacModel::Twiggy:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00020000; // 128K
			c.ramBSize    = 0;
			c.romSize     = 0x00010000; // 64 KB
			c.romBase     = 0x00400000;
			c.romFileName = "Twiggy.ROM";
			c.emVIA1      = true;
			c.emVIA2      = false;
			c.emADB       = false;
			c.emClassicKbrd = true;
			c.emASC       = false;
			c.emClassicSnd  = true;
			c.emVidCard   = false;
			c.includeVidMem = false;
			c.vidMemSize  = 0;
			c.vidROMSize  = 0;
			c.maxATTListN = 16;
			c.screenWidth  = 512;
			c.screenHeight = 342;
			c.screenDepth  = 0;
			c.via1Config  = MakeVIA1Config_Plus();
			break;

		case MacModel::Mac128K:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00020000; // 128K
			c.ramBSize    = 0;
			c.romSize     = 0x00010000; // 64 KB
			c.romBase     = 0x00400000;
			c.romFileName = "Mac128K.ROM";
			c.emVIA1      = true;
			c.emVIA2      = false;
			c.emADB       = false;
			c.emClassicKbrd = true;
			c.emASC       = false;
			c.emClassicSnd  = true;
			c.emVidCard   = false;
			c.includeVidMem = false;
			c.vidMemSize  = 0;
			c.vidROMSize  = 0;
			c.maxATTListN = 16;
			c.screenWidth  = 512;
			c.screenHeight = 342;
			c.screenDepth  = 0;
			c.via1Config  = MakeVIA1Config_Plus();
			break;

		case MacModel::Mac512Ke:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00080000; // 512K
			c.ramBSize    = 0;
			c.romSize     = 0x00020000; // 128 KB
			c.romBase     = 0x00400000;
			c.romFileName = "vMac.ROM";
			c.emVIA1      = true;
			c.emVIA2      = false;
			c.emADB       = false;
			c.emClassicKbrd = true;
			c.emASC       = false;
			c.emClassicSnd  = true;
			c.emVidCard   = false;
			c.includeVidMem = false;
			c.vidMemSize  = 0;
			c.vidROMSize  = 0;
			c.maxATTListN = 16;
			c.screenWidth  = 512;
			c.screenHeight = 342;
			c.screenDepth  = 0;
			c.via1Config  = MakeVIA1Config_Plus();
			break;

		case MacModel::Kanji:
		case MacModel::Plus:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00400000; // 4 MB
			c.ramBSize    = 0;
			c.romSize     = 0x00020000; // 128 KB
			c.romBase     = 0x00400000;
			c.romFileName = "vMac.ROM";
			c.emVIA1      = true;
			c.emVIA2      = false;
			c.emADB       = false;
			c.emClassicKbrd = true;
			c.emASC       = false;
			c.emClassicSnd  = true;
			c.emVidCard   = false;
			c.includeVidMem = false;
			c.vidMemSize  = 0;
			c.vidROMSize  = 0;
			c.maxATTListN = 16;
			c.screenWidth  = 512;
			c.screenHeight = 342;
			c.screenDepth  = 0;
			c.via1Config  = MakeVIA1Config_Plus();
			break;

		case MacModel::SE:
		case MacModel::SEFDHD:
		case MacModel::Classic:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00400000; // 4 MB
			c.ramBSize    = 0;
			c.romSize     = 0x00040000; // 256 KB
			c.romBase     = 0x00400000;
			c.romFileName = "MacSE.ROM";
			c.emVIA1      = true;
			c.emVIA2      = false;
			c.emADB       = true;
			c.emClassicKbrd = false;
			c.emASC       = false;
			c.emClassicSnd  = true;
			c.emVidCard   = false;
			c.includeVidMem = false;
			c.vidMemSize  = 0;
			c.vidROMSize  = 0;
			c.maxATTListN = 16;
			c.screenWidth  = 512;
			c.screenHeight = 342;
			c.screenDepth  = 0;
			c.via1Config  = MakeVIA1Config_SE();
			break;

		case MacModel::PB100:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00400000; // 4 MB
			c.ramBSize    = 0;
			c.romSize     = 0x00040000; // 256 KB
			c.romBase     = 0x00400000;
			c.romFileName = "PB100.ROM";
			c.emVIA1      = true;
			c.emVIA2      = false;
			c.emADB       = false;
			c.emClassicKbrd = false;
			c.emPMU       = true;
			c.emASC       = true;
			c.emClassicSnd  = false;
			c.emVidCard   = false;
			c.includeVidMem = true;
			c.vidMemSize  = 0x00008000;
			c.vidROMSize  = 0;
			c.maxATTListN = 16;
			c.screenWidth  = 640;
			c.screenHeight = 400;
			c.screenDepth  = 0;
			c.via1Config  = MakeVIA1Config_PB100();
			break;

		case MacModel::II:
			// defaults are already Mac II (romSize=256KB, romBase=0x800000)
			c.romFileName = "MacII.ROM";
			c.via1Config  = MakeVIA1Config_MacII();
			c.via2Config  = MakeVIA2Config_MacII();
			break;

		case MacModel::IIx:
			c.model       = MacModel::IIx;
			c.use68020    = true;  // actually 68030
			c.emFPU       = true;
			c.emMMU       = false;
			c.ramASize    = 0x00400000;
			c.ramBSize    = 0x00400000;
			c.romSize     = 0x00040000; // 256 KB
			c.romBase     = 0x00800000;
			c.romFileName = "MacIIx.ROM";
			c.emVIA1      = true;
			c.emVIA2      = true;
			c.emADB       = true;
			c.emClassicKbrd = false;
			c.emASC       = true;
			c.emClassicSnd  = false;
			c.emVidCard   = true;
			c.includeVidMem = true;
			c.vidMemSize  = 0x00080000;
			c.vidROMSize  = 0x000800;
			c.maxATTListN = 20;
			c.via1Config  = MakeVIA1Config_MacII();
			c.via2Config  = MakeVIA2Config_MacII();
			break;
	}

	return c;
}
