/*
	machine_config.cpp

	Model-to-feature mapping factory.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/machine_config.h"
#include "core/model_defs.h"
#include "core/wire_ids.h"

// Helper to build the Mac II VIA1 config
static VIAConfig MakeVIA1Config_MacII()
{
	VIAConfig v;
	v.oraFloatVal = 0xBF;
	v.orbFloatVal = 0xFF;
	v.oraCanIn = 0x80;	// bit 7 readable
	v.oraCanOut = 0x3F; // bits 0-5 writable
	v.orbCanIn = 0x09;	// bits 0,3 readable
	v.orbCanOut = 0xB7; // bits 0-2,4-5,7 writable
	v.ierNever0 = 0x00;
	v.ierNever1 = 0x58;
	v.cb2ModesAllowed = 0x01;
	v.ca2ModesAllowed = 0x01;
	v.portAWires = {Wire_VIA1_iA0, Wire_VIA1_iA1, Wire_VIA1_iA2, Wire_VIA1_iA3,
					Wire_VIA1_iA4, Wire_VIA1_iA5, Wire_VIA1_iA6, Wire_VIA1_iA7};
	v.portBWires = {Wire_VIA1_iB0, Wire_VIA1_iB1, Wire_VIA1_iB2, Wire_VIA1_iB3,
					Wire_VIA1_iB4, Wire_VIA1_iB5, Wire_VIA1_iB6, Wire_VIA1_iB7};
	v.cb2Wire = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

// Helper to build the Mac II VIA2 config
static VIAConfig MakeVIA2Config_MacII()
{
	VIAConfig v;
	v.oraFloatVal = 0xFF;
	v.orbFloatVal = 0xFF;
	v.oraCanIn = 0x01;	// bit 0 readable
	v.oraCanOut = 0xC0; // bits 6-7 writable
	v.orbCanIn = 0x00;
	v.orbCanOut = 0x8C; // bits 2,3,7 writable
	v.ierNever0 = 0x00;
	v.ierNever1 = 0xED;
	v.cb2ModesAllowed = 0x01;
	v.ca2ModesAllowed = 0x01;
	v.portAWires = {Wire_VBLinterrupt, Wire_VIA2_iA1, Wire_VIA2_iA2, Wire_VIA2_iA3,
					Wire_VIA2_iA4,	   Wire_VIA2_iA5, Wire_VIA2_iA6, Wire_VIA2_iA7};
	v.portBWires = {Wire_VIA2_iB0, Wire_VIA2_iB1, Wire_VIA2_iB2, Wire_VIA2_iB3,
					Wire_VIA2_iB4, Wire_VIA2_iB5, Wire_VIA2_iB6, Wire_VIA2_iB7};
	v.cb2Wire = Wire_VIA2_iCB2;
	v.interruptWire = Wire_VIA2_InterruptRequest;
	return v;
}

// Helper for Plus/128K/512K/Twiggy VIA1 config (no VIA2)
static VIAConfig MakeVIA1Config_Plus()
{
	VIAConfig v;
	v.oraFloatVal = 0xFF;
	v.orbFloatVal = 0xFF;
	v.oraCanIn = 0x80;
	v.oraCanOut = 0x7F;
	v.orbCanIn = 0x79;	// bits 0,3,4,5,6: RTC data, MouseBtnUp, MouseX2, MouseY2, H4
	v.orbCanOut = 0x87; // bits 0,1,2,7: RTC data, RTC clock, RTC enable, Sound
	v.ierNever0 = 0x02; // bit 1 always 0
	v.ierNever1 = 0x18; // bits 3,4 always 1
	v.cb2ModesAllowed = 0x01;
	v.ca2ModesAllowed = 0x01;
	/* On Plus, VIA1 port A bits 0-2 are sound volume, bit 3 is sound buffer select.
	   These wires are aliased to the functional Wire_SoundVolbX / SoundBuffer IDs
	   so that writes from VIA1 putORB/putORA reach the sound subsystem. */
	v.portAWires = {Wire_SoundVolb0, Wire_SoundVolb1, Wire_SoundVolb2, Wire_VIA1_iA3,
					Wire_VIA1_iA4,	 Wire_VIA1_iA5,	  Wire_VIA1_iA6,   Wire_VIA1_iA7};
	/* On Plus, VIA1 port B bit 3 is mouse button, bits 4-5 are mouse quadrature,
	   bit 7 is SOUND_DISABLE.  Route these to functional wire IDs. */
	v.portBWires = {Wire_VIA1_iB0, Wire_VIA1_iB1, Wire_VIA1_iB2, Wire_VIA1_iB3,
					Wire_VIA1_iB4, Wire_VIA1_iB5, Wire_VIA1_iB6, Wire_SoundDisable};
	v.cb2Wire = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

// Helper for SE/SEFDHD/Classic VIA1 (similar to II but no VIA2, ADB on VIA1)
static VIAConfig MakeVIA1Config_SE()
{
	VIAConfig v;
	v.oraFloatVal = 0xFF;
	v.orbFloatVal = 0xFF;
	v.oraCanIn = 0x80;
	v.oraCanOut = 0x7F;
	v.orbCanIn = 0x09;	// bits 0,3: RTC data, ADB_Int
	v.orbCanOut = 0xF7; // bits 0-2,4-5,7: RTC, ADB_st0, ADB_st1, Sound
	v.ierNever0 = 0x00;
	v.ierNever1 = 0x18; // bits 3,4 always 1
	v.cb2ModesAllowed = 0x01;
	v.ca2ModesAllowed = 0x01;
	/* SE/Classic: port A bits 0-2 are sound volume (same as Plus) */
	v.portAWires = {Wire_SoundVolb0, Wire_SoundVolb1, Wire_SoundVolb2, Wire_VIA1_iA3,
					Wire_VIA1_iA4,	 Wire_VIA1_iA5,	  Wire_VIA1_iA6,   Wire_VIA1_iA7};
	/* SE/Classic: port B bit 7 is SOUND_DISABLE (same as Plus).
	   Bits 3-5 are ADB (ADB_Int, ADB_st0, ADB_st1), not mouse quadrature. */
	v.portBWires = {Wire_VIA1_iB0, Wire_VIA1_iB1, Wire_VIA1_iB2, Wire_VIA1_iB3,
					Wire_VIA1_iB4, Wire_VIA1_iB5, Wire_VIA1_iB6, Wire_SoundDisable};
	v.cb2Wire = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

// PB100 VIA1 config (PMU on port A)
static VIAConfig MakeVIA1Config_PB100()
{
	VIAConfig v;
	v.oraFloatVal = 0xFF;
	v.orbFloatVal = 0xFF;
	v.oraCanIn = 0xFF;	// all of port A readable (PMU bus)
	v.oraCanOut = 0xFF; // all of port A writable (PMU bus)
	v.orbCanIn = 0x02;	// bit 1: PMU interrupt
	v.orbCanOut = 0xFD; // all except bit 1
	v.ierNever0 = 0x00;
	v.ierNever1 = 0x0C;
	v.cb2ModesAllowed = 0x03;
	v.ca2ModesAllowed = 0x03;
	v.portAWires = {Wire_VIA1_iA0, Wire_VIA1_iA1, Wire_VIA1_iA2, Wire_VIA1_iA3,
					Wire_VIA1_iA4, Wire_VIA1_iA5, Wire_VIA1_iA6, Wire_VIA1_iA7};
	/* PB100 uses Port B bit 0 for PmuToReady and bit 1 for
	   PmuFromReady — wire them directly to the PMU signals so
	   VIA reads/writes hit the same wire the PMU uses. */
	v.portBWires = {Wire_PMU_ToReady, Wire_PMU_FromReady, Wire_VIA1_iB2, Wire_VIA1_iB3,
					Wire_VIA1_iB4,	  Wire_VIA1_iB5,	  Wire_VIA1_iB6, Wire_VIA1_iB7};
	v.cb2Wire = Wire_VIA1_iCB2;
	v.interruptWire = Wire_VIA1_InterruptRequest;
	return v;
}

// --- VIA dispatcher functions ---

static VIAConfig MakeVIA1ConfigFor(MacModel model)
{
	switch (model)
	{
		case MacModel::Twig43:
		case MacModel::Twiggy:
		case MacModel::Mac128K:
		case MacModel::Mac512Ke:
		case MacModel::Kanji:
		case MacModel::Plus:
			return MakeVIA1Config_Plus();

		case MacModel::SE:
		case MacModel::SEFDHD:
		case MacModel::Classic:
			return MakeVIA1Config_SE();

		case MacModel::PB100:
			return MakeVIA1Config_PB100();

		case MacModel::II:
		case MacModel::IIx:
			return MakeVIA1Config_MacII();
	}
	return MakeVIA1Config_Plus(); // unreachable
}

static VIAConfig MakeVIA2ConfigFor(MacModel model)
{
	switch (model)
	{
		case MacModel::II:
		case MacModel::IIx:
			return MakeVIA2Config_MacII();
		default:
			return VIAConfig{};
	}
}

// --- New table-driven implementation ---

MachineConfig MachineConfigForModel(MacModel model)
{
	const ModelDef *def = ModelDefFor(model);
	assert(def && "MachineConfigForModel: no ModelDef for model");

	MachineConfig c;
	c.model = def->id;
	c.use68020 = def->use68020;
	c.emFPU = def->emFPU;
	c.emMMU = def->emMMU;
	c.ramASize = def->ramASize;
	c.ramBSize = def->ramBSize;
	c.romSize = def->rom.size;
	c.romBase = def->rom.base;
	c.romFileName = def->rom.filename.data();
	c.extnBlockBase = def->extnBlockBase;
	c.extnLn2Spc = def->extnLn2Spc;
	c.emVIA1 = def->emVIA1;
	c.emVIA2 = def->emVIA2;
	c.emADB = def->emADB;
	c.emClassicKbrd = def->emClassicKbrd;
	c.emRTC = def->emRTC;
	c.emPMU = def->emPMU;
	c.emASC = def->emASC;
	c.emClassicSnd = def->emClassicSnd;
	c.emVidCard = def->emVidCard;
	c.includeVidMem = def->includeVidMem;
	c.vidMemSize = def->vidMemSize;
	c.vidROMSize = def->vidROMSize;
	c.clockMult = def->clockMult;
	c.autoSlowSubTicks = def->autoSlowSubTicks;
	c.autoSlowTime = def->autoSlowTime;
	c.maxATTListN = def->maxATTListN;
	c.screenWidth = def->screen.width;
	c.screenHeight = def->screen.height;
	c.screenDepth = def->screen.depth;

	c.via1Config = MakeVIA1ConfigFor(model);
	c.via2Config = MakeVIA2ConfigFor(model);
	return c;
}
