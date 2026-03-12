/*
	machine_config.cpp

	Model-to-feature mapping factory.
	Encodes the configuration that currently lives in CNFUDPIC.h
	and the setup_t tool.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/machine_config.h"

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
			break;

		case MacModel::Mac128K:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00020000; // 128K
			c.ramBSize    = 0;
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
			break;

		case MacModel::Mac512Ke:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00080000; // 512K
			c.ramBSize    = 0;
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
			break;

		case MacModel::Kanji:
		case MacModel::Plus:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00400000; // 4 MB
			c.ramBSize    = 0;
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
			break;

		case MacModel::SE:
		case MacModel::SEFDHD:
		case MacModel::Classic:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00400000; // 4 MB
			c.ramBSize    = 0;
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
			break;

		case MacModel::PB100:
			c.use68020    = false;
			c.emFPU       = false;
			c.emMMU       = false;
			c.ramASize    = 0x00400000; // 4 MB
			c.ramBSize    = 0;
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
			break;

		case MacModel::II:
			// defaults are already Mac II
			break;

		case MacModel::IIx:
			c.model       = MacModel::IIx;
			c.use68020    = true;  // actually 68030
			c.emFPU       = true;
			c.emMMU       = false;
			c.ramASize    = 0x00400000;
			c.ramBSize    = 0x00400000;
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
			break;
	}

	return c;
}
