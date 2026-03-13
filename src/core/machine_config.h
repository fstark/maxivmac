/*
	machine_config.h

	Runtime machine configuration.
	Replaces compile-time #defines from CNFUDPIC.h with a struct
	that can be populated per-model at runtime.

	Part of Phase 4: Device Interface & Machine Object.
*/

#pragma once
#include <cstdint>
#include <array>

// VIA port configuration — replaces compile-time VIA1_ORA_FloatVal etc.
struct VIAConfig {
	uint8_t oraFloatVal    = 0xFF;
	uint8_t orbFloatVal    = 0xFF;
	uint8_t oraCanIn       = 0x00;
	uint8_t oraCanOut      = 0x00;
	uint8_t orbCanIn       = 0x00;
	uint8_t orbCanOut      = 0x00;
	uint8_t ierNever0      = 0x00;
	uint8_t ierNever1      = 0x00;
	uint8_t cb2ModesAllowed = 0x01;
	uint8_t ca2ModesAllowed = 0x01;

	// Wire mapping: which WireID is connected to each port bit
	// -1 means unconnected (no wire)
	std::array<int, 8> portAWires = {-1, -1, -1, -1, -1, -1, -1, -1};
	std::array<int, 8> portBWires = {-1, -1, -1, -1, -1, -1, -1, -1};
	int cb2Wire       = -1;   // WireID for CB2
	int interruptWire = -1;   // WireID for interrupt request output
};

// Model IDs — same values as the existing #defines in machine.h
enum class MacModel : int {
	Twig43   = 0,
	Twiggy   = 1,
	Mac128K  = 2,
	Mac512Ke = 3,
	Kanji    = 4,
	Plus     = 5,
	SE       = 6,
	SEFDHD   = 7,
	Classic  = 8,
	PB100    = 9,
	II       = 10,
	IIx      = 11,
};

struct MachineConfig {
	MacModel model       = MacModel::II;
	bool     use68020    = true;
	bool     emFPU       = true;
	bool     emMMU       = false;

	uint32_t ramASize    = 0x00400000;  // 4 MB
	uint32_t ramBSize    = 0x00400000;  // 4 MB
	uint32_t vidMemSize  = 0x00080000;  // 512 KB
	uint32_t vidROMSize  = 0x000800;

	// ROM configuration (set by model factory)
	uint32_t    romSize     = 0x00040000;  // 256 KB default (Mac II)
	uint32_t    romBase     = 0x00800000;  // Mac II ROM base
	const char* romFileName = "MacII.ROM";

	// Derived feature flags (set by model factory)
	bool emVIA1         = true;
	bool emVIA2         = true;
	bool emADB          = true;
	bool emClassicKbrd  = false;
	bool emRTC          = true;
	bool emPMU          = false;
	bool emASC          = true;
	bool emClassicSnd   = false;
	bool emVidCard      = true;
	bool includeVidMem  = true;

	int  clockMult      = 2;
	int  autoSlowSubTicks = 16384;
	int  autoSlowTime   = 60;

	int  maxATTListN    = 20;

	// Screen
	uint16_t screenWidth  = 640;
	uint16_t screenHeight = 480;
	uint8_t  screenDepth  = 3;  // log2 bpp: 0=1bpp, 3=8bpp

	// VIA port configuration (populated by model factory)
	VIAConfig via1Config;
	VIAConfig via2Config;

	// Helper predicates
	bool isIIFamily() const {
		return model == MacModel::II || model == MacModel::IIx;
	}
	bool isCompactMac() const {
		return static_cast<int>(model) <= static_cast<int>(MacModel::Plus);
	}
	bool isSEFamily() const {
		return model == MacModel::SE || model == MacModel::SEFDHD
		    || model == MacModel::Classic;
	}
	bool isSEOrLater() const {
		return static_cast<int>(model) >= static_cast<int>(MacModel::SE);
	}
	uint32_t ramSize() const { return ramASize + ramBSize; }
};

// Factory: populate derived flags from model
MachineConfig MachineConfigForModel(MacModel model);
