/*
	state_recorder.hpp

	Non-regression test harness for the emulator.
	Records or verifies CPU state snapshots at regular intervals.
	Self-contained — no dependency on emulator internals.

	See docs/TRACE.md for the full design document.
*/

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>


// ── Modes ──────────────────────────────────────────────

enum class RecorderMode {
	Off,
	Record,
	Verify,
};

enum class OnMismatch {
	ExitNonZero,    // print + exit(1) — CI / test.sh
	Print,          // print + keep going — interactive debug
};

enum class TextLog {
	None,
	CpuOnly,        // text CPU snapshots only
	CpuAndIo,       // text CPU + every I/O access
};


// ── The class ──────────────────────────────────────────

class StateRecorder {
public:
	StateRecorder() = default;
	~StateRecorder();

	StateRecorder(const StateRecorder&) = delete;
	StateRecorder& operator=(const StateRecorder&) = delete;

	struct Config {
		RecorderMode  mode             = RecorderMode::Off;
		std::string   goldenPath;
		std::string   textPath;         // "" → stderr when textLog != None
		TextLog       textLog          = TextLog::None;
		OnMismatch    onMismatch       = OnMismatch::ExitNonZero;
		uint32_t      snapshotInterval = 100'000;
		uint64_t      maxInstructions  = 50'000'000;

		uint32_t      modelId          = 0;
		uint8_t       speedValue       = 0;
		uint32_t      ramSize          = 0;
		uint16_t      screenWidth      = 0;
		uint16_t      screenHeight     = 0;
		uint8_t       screenDepth      = 0;
		uint8_t       romHash[16]      = {};
		uint8_t       diskHash[16]     = {};
	};

	// ── Read golden-file header without full init ──
	struct HeaderInfo {
		uint32_t modelId          = 0;
		uint8_t  speedValue       = 0;
		uint32_t ramSize          = 0;
		uint16_t screenWidth      = 0;
		uint16_t screenHeight     = 0;
		uint8_t  screenDepth      = 0;
		uint32_t snapshotInterval = 0;
		uint64_t maxInstructions  = 0;
		uint8_t  romHash[16]      = {};
		uint8_t  diskHash[16]     = {};
	};
	static bool readHeader(const std::string& path, HeaderInfo& out);

	bool init(const Config& cfg);

	// ── Per-instruction hook (CPU loop) ──
	// instructionCount is still uint32_t from the emulator; we widen to 64 internally.
	void cpu(uint32_t instructionCount,
	         uint32_t pc, uint16_t sr,
	         const uint32_t d[8], const uint32_t a[8]);

	// ── Per-I/O-access hook (MMDV_Access) ──
	void io(uint32_t instructionCount,
	        uint32_t address, uint32_t data,
	        bool write, bool byteSize, const char* deviceName);

	bool active() const;

private:
	void cpuSlow(uint32_t instructionCount,
	             uint32_t pc, uint16_t sr,
	             const uint32_t d[8], const uint32_t a[8]);
	void ioSlow(uint32_t instructionCount,
	            uint32_t address, uint32_t data,
	            bool write, bool byteSize, const char* deviceName);
	void textIo(uint32_t icount, uint32_t addr, uint32_t data,
	            bool write, bool byteSize, const char* device);
	void finish();

	RecorderMode  mode_              = RecorderMode::Off;
	OnMismatch    onMismatch_        = OnMismatch::ExitNonZero;
	TextLog       textLog_           = TextLog::None;
	uint32_t      snapshotInterval_  = 100'000;
	uint64_t      maxInstructions_   = 50'000'000;
	uint32_t      nextSnapshot_      = 0;
	uint32_t      snapshotIndex_     = 0;
	uint32_t      snapshotCount_     = 0;

	uint32_t      ioCrc_             = 0;   // rolling CRC32, reset each snapshot

	FILE*         goldenFile_        = nullptr;
	FILE*         textFile_          = nullptr; // nullptr → stderr

	void*         goldenSnaps_       = nullptr; // CpuSnapshot[], opaque here
	bool          finished_          = false;
};

extern StateRecorder g_recorder;
