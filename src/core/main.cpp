/*
	PROGram MAIN.
*/

#include "core/common.h"

#include "devices/via.h"
#include "devices/via2.h"
#include "devices/iwm.h"
#include "devices/scc.h"
#include "devices/rtc.h"
#include "devices/rom.h"
#include "devices/scsi.h"
#include "devices/sony.h"
#include "devices/screen.h"
#include "devices/video.h"
#include "devices/keyboard.h"
#include "devices/pmu.h"
#include "devices/adb.h"
#include "devices/sound.h"
#include "devices/asc.h"
#include "devices/mouse.h"


#include "core/main.h"
#include "core/config_loader.h"
#include "core/machine_obj.h"
#include "core/machine_config.h"
#include "core/emulator_config.h"
#include "core/ict_scheduler.h"
#include "core/state_recorder.hpp"
#include "core/md5.h"
#include "cpu/cpu.h"

#include <memory>

/*
	ReportAbnormalID unused 0x1002 - 0x10FF
*/

ICTScheduler g_ict;

// Reset memory, scheduler, and all devices to power-on state.
static void EmulatedHardwareZap()
{
	memoryReset();
	g_ict.zap();
	if (auto* d = g_machine->findDevice<IWMDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCCDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCSIDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->zap();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->zap();
	if (auto* d = g_machine->findDevice<SonyDevice>()) d->reset();
	extnReset();
	g_cpu.reset();
}

static void DoMacReset()
{
	if (auto* d = g_machine->findDevice<SonyDevice>()) d->ejectAllDisks();
	EmulatedHardwareZap();
}

// Process interrupt-button and reset-button requests once per tick.
static void InterruptReset_Update()
{
	SetInterruptButton(false);
		/*
			in case has been set. so only stays set
			for 60th of a second.
		*/

	if (g_wantMacInterrupt) {
		SetInterruptButton(true);
		g_wantMacInterrupt = false;
	}
	if (g_wantMacReset) {
		DoMacReset();
		g_wantMacReset = false;
	}
}

// Route audio sub-tick to the active sound device.
static void SubTickNotify(int SubTick)
{
	if (g_machine->config().emClassicSnd) {
		if (auto* d = g_machine->findDevice<SoundDevice>()) d->subTick(SubTick);
	} else if (g_machine->config().emASC) {
		if (auto* d = g_machine->findDevice<ASCDevice>()) d->subTick(SubTick);
	} else {
		UNUSED(SubTick);
	}
}

#define CyclesScaledPerTick (130240UL * g_machine->config().clockMult * kCycleScale)
#define CyclesScaledPerSubTick (CyclesScaledPerTick / kNumSubTicks)

static uint16_t s_subTickCounter;

// Advance sub-tick counter; reschedule unless final sub-tick.
static void SubTickTaskDo()
{
	SubTickNotify(s_subTickCounter);
	++s_subTickCounter;
	if (s_subTickCounter < (kNumSubTicks - 1)) {
		/*
			final SubTick handled by SubTickTaskEnd,
			since CyclesScaledPerSubTick * kNumSubTicks
			might not equal CyclesScaledPerTick.
		*/

		g_ict.add(kICT_SubTick, CyclesScaledPerSubTick);
	}
}

static void SubTickTaskStart()
{
	s_subTickCounter = 0;
	g_ict.add(kICT_SubTick, CyclesScaledPerSubTick);
}

static void SubTickTaskEnd()
{
	SubTickNotify(kNumSubTicks - 1);
}

static int s_ticksSinceSecond = 0;

/*
	Begin-of-tick processing: advance the real-time clock,
	poll mouse/keyboard/ADB, fire VBI, update Sony,
	and start the sub-tick chain.
*/
static void SixtiethSecondNotify()
{
#if 0
	dbglog_WriteNote("begin new Sixtieth");
#endif
	if (++s_ticksSinceSecond >= 60) {
		s_ticksSinceSecond = 0;
		g_curMacDateInSeconds++;
	}
	if (auto* d = g_machine->findDevice<MouseDevice>()) d->update();
	InterruptReset_Update();
	if (g_machine->config().emClassicKbrd)
		if (auto* d = g_machine->findDevice<KeyboardDevice>()) d->update();
	if (g_machine->config().emADB)
		if (auto* d = g_machine->findDevice<ADBDevice>()) d->update();

	if (auto* d = g_machine->findDevice<VIA1Device>()) d->iCA1_PulseNtfy(); /* Vertical Blanking Interrupt */
	if (auto* d = g_machine->findDevice<SonyDevice>()) d->update();

#if EmLocalTalk
	if (auto* d = g_machine->findDevice<SCCDevice>()) d->localTalkTick();
#endif
	if (auto* d = g_machine->findDevice<RTCDevice>()) d->interrupt();
	if (g_machine->config().emVidCard)
		if (auto* d = g_machine->findDevice<VideoDevice>()) d->update();

	SubTickTaskStart();
}

// End-of-tick: flush final sub-tick, update mouse and screen.
static void SixtiethEndNotify()
{
	SubTickTaskEnd();
	if (auto* d = g_machine->findDevice<MouseDevice>()) d->endTickNotify();
	if (auto* d = g_machine->findDevice<ScreenDevice>()) d->endTickNotify();
#if 0
	dbglog_WriteNote("end Sixtieth");
#endif
}

static void ExtraTimeBeginNotify()
{
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->extraTimeBegin();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->extraTimeBegin();
}

static void ExtraTimeEndNotify()
{
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->extraTimeEnd();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->extraTimeEnd();
}

// Allocate RAM, VidROM, and VidMem buffers per machine config.
bool EmulationReserveAlloc()
{
	const auto& cfg = g_machine->config();
	if (!AllocBlock(&g_ram,
		cfg.ramSize() + RAMSafetyMarginFudge, false))
		return false;
	if (cfg.emVidCard)
		if (!AllocBlock(&g_vidROM, cfg.vidROMSize, false))
			return false;
	if (cfg.includeVidMem)
		if (!AllocBlock(&g_vidMem,
			cfg.vidMemSize + RAMSafetyMarginFudge, true))
			return false;
	return true;
}

void EmulationFreeAlloc()
{
	free(g_ram); g_ram = nullptr;
	free(g_vidROM); g_vidROM = nullptr;
	free(g_vidMem); g_vidMem = nullptr;
}

/*
	Wire the ICT scheduler to the CPU, register all device
	task handlers, initialize RTC/g_rom/Video, build the
	address space, and perform hardware zap.
*/
static bool InitEmulation()
{
	/* Wire ICT scheduler to CPU cycle counters */
	g_ict.setCycleAccessors(
		[]() { return g_cpu.getCyclesRemaining(); },
		[](int32_t n) { g_cpu.setCyclesRemaining(n); }
	);

	/* Register ICT task handlers */
	g_ict.registerTask(kICT_SubTick, SubTickTaskDo);
	if (g_machine->config().emClassicKbrd) {
		g_ict.registerTask(kICT_Kybd_ReceiveEndCommand, [](){ if (auto* d = g_machine->findDevice<KeyboardDevice>()) d->receiveEndCommand(); });
		g_ict.registerTask(kICT_Kybd_ReceiveCommand, [](){ if (auto* d = g_machine->findDevice<KeyboardDevice>()) d->receiveCommand(); });
	}
	if (g_machine->config().emADB)
		g_ict.registerTask(kICT_ADB_NewState, [](){ if (auto* d = g_machine->findDevice<ADBDevice>()) d->doNewState(); });
	if (g_machine->config().emPMU)
		g_ict.registerTask(kICT_PMU_Task, [](){ if (auto* d = g_machine->findDevice<PMUDevice>()) d->doTask(); });
	if (g_machine->config().emVIA1) {
		g_ict.registerTask(kICT_VIA1_Timer1Check, [](){ if (auto* d = g_machine->findDevice<VIA1Device>()) d->doTimer1Check(); });
		g_ict.registerTask(kICT_VIA1_Timer2Check, [](){ if (auto* d = g_machine->findDevice<VIA1Device>()) d->doTimer2Check(); });
	}
	if (g_machine->config().emVIA2) {
		g_ict.registerTask(kICT_VIA2_Timer1Check, [](){ if (auto* d = g_machine->findDevice<VIA2Device>()) d->doTimer1Check(); });
		g_ict.registerTask(kICT_VIA2_Timer2Check, [](){ if (auto* d = g_machine->findDevice<VIA2Device>()) d->doTimer2Check(); });
	}

	bool ok = true;
	if (ok && g_machine->config().emRTC) {
		auto* rtc = g_machine->findDevice<RTCDevice>();
		ok = rtc ? rtc->init() : false;
	}
	if (ok) { auto* rom = g_machine->findDevice<ROMDevice>(); ok = rom ? rom->init() : false; }
	if (ok && g_machine->config().emVidCard) {
		auto* vid = g_machine->findDevice<VideoDevice>();
		ok = vid ? vid->init() : false;
	}
	if (ok) ok = AddrSpac_Init();
	if (ok) {
		EmulatedHardwareZap();
		return true;
	}
	return false;
}

/*
	Run the CPU for n cycles, interleaving ICT task dispatch.
	Each iteration runs until the next scheduled task, then
	checks and dispatches due tasks before continuing.
*/
static void m68k_go_nCycles_1(uint32_t n)
{
	uint32_t n2;
	uint32_t StopiCount = g_ict.nextCount + n;
	do {
		g_ict.doCurrentTasks();
		n2 = g_ict.doGetNext(n);
#if 0
		dbglog_StartLine();
		dbglog_writeCStr("before m68k_go_nCycles, nextCount:");
		dbglog_writeHex(g_ict.nextCount);
		dbglog_writeCStr(", n2:");
		dbglog_writeHex(n2);
		dbglog_writeCStr(", n:");
		dbglog_writeHex(n);
		dbglog_writeReturn();
#endif
		g_ict.nextCount += n2;
		g_cpu.go_nCycles(n2);
		n = StopiCount - g_ict.nextCount;
	} while (n != 0);
}

static uint32_t s_extraSubTicksToDo = 0;

/*
	Emulate one 60 Hz tick: run SixtiethSecondNotify, execute the
	cycle budget, then SixtiethEndNotify.  Extra sub-ticks are
	accumulated for speed multipliers.
*/
static void DoEmulateOneTick()
{
	{
		uint32_t NewQuietTime = g_quietTime + 1;

		if (NewQuietTime > g_quietTime) {
			/* if not overflow */
			g_quietTime = NewQuietTime;
		}
	}
	{
		uint32_t NewQuietSubTicks = g_quietSubTicks + kNumSubTicks;

		if (NewQuietSubTicks > g_quietSubTicks) {
			/* if not overflow */
			g_quietSubTicks = NewQuietSubTicks;
		}
	}

	SixtiethSecondNotify();

	m68k_go_nCycles_1(CyclesScaledPerTick);

	SixtiethEndNotify();

	if ((uint8_t) -1 == g_speedValue) {
		s_extraSubTicksToDo = (uint32_t) -1;
	} else {
		uint32_t ExtraAdd = (kNumSubTicks << g_speedValue) - kNumSubTicks;
		uint32_t ExtraLimit = ExtraAdd << 3;

		s_extraSubTicksToDo += ExtraAdd;
		if (s_extraSubTicksToDo > ExtraLimit) {
			s_extraSubTicksToDo = ExtraLimit;
		}
	}
}

static bool MoreSubTicksToDo()
{
	/* Always complete all extra sub-ticks regardless of wall clock,
	   so the emulated cycle count per tick is deterministic.
	   Original code gated this on ExtraTimeNotOver() which made
	   the number of sub-ticks vary with host speed. */
	bool v = false;

	if (s_extraSubTicksToDo > 0) {
		v = true;
	}

	return v;
}

static void DoEmulateExtraTime()
{
	/*
		DoEmulateExtraTime is used for
		anything over emulation speed
		of 1x. It periodically calls
		ExtraTimeNotOver and stops
		when this returns false (or it
		is finished with emulating the
		extra time).
	*/

	if (MoreSubTicksToDo()) {
		ExtraTimeBeginNotify();
		do {
			{
				uint32_t NewQuietSubTicks = g_quietSubTicks + 1;

				if (NewQuietSubTicks > g_quietSubTicks) {
					/* if not overflow */
					g_quietSubTicks = NewQuietSubTicks;
				}
			}
			m68k_go_nCycles_1(CyclesScaledPerSubTick);
			--s_extraSubTicksToDo;
		} while (MoreSubTicksToDo());
		ExtraTimeEndNotify();
	}
}

static uint32_t s_curEmulatedTime = 0;
	/*
		The number of ticks that have been
		emulated so far.

		That is, the number of times
		"DoEmulateOneTick" has been called.
	*/

static void RunEmulatedTicksToTrueTime()
{
	/*
		Always emulate exactly the number of ticks
		that g_trueEmulatedTime says are due, without
		any cap or wall-clock gating.  This ensures
		the emulated tick count (and therefore the
		entire instruction stream) is deterministic
		regardless of host speed.
	*/

	int16_t n = (int16_t)(g_onTrueTime - s_curEmulatedTime);

	if (n > 0) {
		DoEmulateOneTick();
		++s_curEmulatedTime;

		DoneWithDrawingForTick();

		if (--n > 0) {
			g_emVideoDisable = true;

			do {
				DoEmulateOneTick();
				++s_curEmulatedTime;
			} while (--n > 0);

			g_emVideoDisable = false;
		}

		g_emLagTime = 0;
	}
}

// Main run loop: wait for host tick, run emulation, repeat.
static void MainEventLoop()
{
	for (; ; ) {
		WaitForNextTick();
		if (g_forceMacOff) {
			return;
		}

		RunEmulatedTicksToTrueTime();

		DoEmulateExtraTime();
	}
}

static std::unique_ptr<Machine> s_machine;
static LaunchConfig s_launchConfig;
static MachineConfig s_machineConfig;
static EmulatorConfig s_emulatorConfig;

const LaunchConfig& GetLaunchConfig()
{
	return s_launchConfig;
}

const EmulatorConfig& GetEmulatorConfig()
{
	return s_emulatorConfig;
}

EmulatorConfig& GetEmulatorConfigMut()
{
	return s_emulatorConfig;
}

/*
	Parse CLI args, configure machine and emulator, set up
	the state recorder, and resolve g_rom/disk paths.
*/
void ProgramEarlyInit(int argc, char* argv[])
{
	s_launchConfig = ParseCommandLine(argc, argv);
	s_emulatorConfig = BuildEmulatorConfig(s_launchConfig);

	if (s_launchConfig.langSet) {
		SetLanguage(s_launchConfig.lang);
	}

	if (s_launchConfig.help) {
		PrintUsage(argv[0]);
		// Help flag will be checked by caller to exit early
	}

	/* Set up instruction-logging window from CLI args */
	if (s_launchConfig.logCount > 0) {
		g_LogStart = s_launchConfig.logStart;
		g_LogEnd   = s_launchConfig.logStart + s_launchConfig.logCount;
	}

	/* When verifying, source emulation params from the golden file */
	StateRecorder::HeaderInfo goldenHdr;
	bool haveGoldenHdr = false;
	if (!s_launchConfig.verifyPath.empty()) {
		if (!StateRecorder::readHeader(s_launchConfig.verifyPath, goldenHdr)) {
			std::fprintf(stderr, "Cannot read golden file header, aborting.\n");
			std::exit(1);
		}
		haveGoldenHdr = true;
		s_launchConfig.model = static_cast<MacModel>(goldenHdr.modelId);
		g_speedValue = goldenHdr.speedValue;
		g_SkipThrottle = true;
	}

	/* Set up StateRecorder from CLI args */
	s_machineConfig = BuildMachineConfig(s_launchConfig);

	/* Override MachineConfig with exact values from golden header */
	if (haveGoldenHdr) {
		uint32_t ram = goldenHdr.ramSize;
		if (s_machineConfig.ramBSize > 0) {
			s_machineConfig.ramASize = ram / 2;
			s_machineConfig.ramBSize = ram / 2;
		} else {
			s_machineConfig.ramASize = ram;
		}
		s_machineConfig.screenWidth  = goldenHdr.screenWidth;
		s_machineConfig.screenHeight = goldenHdr.screenHeight;
		s_machineConfig.screenDepth  = goldenHdr.screenDepth;
	}

	{
		StateRecorder::Config rc;

		if (!s_launchConfig.recordPath.empty()) {
			rc.mode = RecorderMode::Record;
			rc.goldenPath = s_launchConfig.recordPath;
		} else if (!s_launchConfig.verifyPath.empty()) {
			rc.mode = RecorderMode::Verify;
			rc.goldenPath = s_launchConfig.verifyPath;
		}

		if (!s_launchConfig.tracePath.empty()) {
			rc.textLog = TextLog::CpuAndIo;
			rc.textPath = s_launchConfig.tracePath;
		} else if (!s_launchConfig.traceCpuPath.empty()) {
			rc.textLog = TextLog::CpuOnly;
			rc.textPath = s_launchConfig.traceCpuPath;
		}

		if (s_launchConfig.snapshotInterval > 0)
			rc.snapshotInterval = s_launchConfig.snapshotInterval;
		if (s_launchConfig.maxInstructions > 0)
			rc.maxInstructions = s_launchConfig.maxInstructions;

		rc.modelId = static_cast<uint32_t>(s_launchConfig.model);
		rc.speedValue = g_speedValue;
		rc.ramSize = s_machineConfig.ramSize();
		rc.screenWidth = s_machineConfig.screenWidth;
		rc.screenHeight = s_machineConfig.screenHeight;
		rc.screenDepth = s_machineConfig.screenDepth;

		// Hash ROM file
		std::string resolvedRom = ResolveRomPath(s_launchConfig.romPath, s_launchConfig.model, s_launchConfig.romDir);
		if (!resolvedRom.empty())
			md5_file(resolvedRom.c_str(), rc.romHash);
		// Hash first disk
		if (!s_launchConfig.diskPaths.empty())
			md5_file(s_launchConfig.diskPaths[0].c_str(), rc.diskHash);

		if (rc.mode != RecorderMode::Off || rc.textLog != TextLog::None) {
			if (!g_recorder.init(rc)) {
				std::fprintf(stderr, "StateRecorder init failed, aborting.\n");
				std::exit(1);
			}
		}
	}

	s_machine = std::make_unique<Machine>(std::move(s_machineConfig));
	s_machine->init();
}

void ProgramCleanup()
{
	s_machine.reset();
}

void ProgramMain()
{
	if (InitEmulation())
	{
		MainEventLoop();
	}
}
