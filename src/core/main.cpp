/*
	PROGMAIN.c

	Copyright (C) 2009 Bernd Schmidt, Philip Cummins, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

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
#include "core/ict_scheduler.h"
#include "core/state_recorder.hpp"
#include "core/md5.h"
#include "cpu/cpu.h"

#include <memory>

/*
	ReportAbnormalID unused 0x1002 - 0x10FF
*/

ICTScheduler g_ict;

static void EmulatedHardwareZap()
{
	Memory_Reset();
	ICT_Zap();
	if (auto* d = g_machine->findDevice<IWMDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCCDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<SCSIDevice>()) d->reset();
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->zap();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->zap();
	if (auto* d = g_machine->findDevice<SonyDevice>()) d->reset();
	Extn_Reset();
	g_cpu.reset();
}

static void DoMacReset()
{
	if (auto* d = g_machine->findDevice<SonyDevice>()) d->ejectAllDisks();
	EmulatedHardwareZap();
}

static void InterruptReset_Update()
{
	SetInterruptButton(false);
		/*
			in case has been set. so only stays set
			for 60th of a second.
		*/

	if (WantMacInterrupt) {
		SetInterruptButton(true);
		WantMacInterrupt = false;
	}
	if (WantMacReset) {
		DoMacReset();
		WantMacReset = false;
	}
}

static void SubTickNotify(int SubTick)
{
#if 0
	dbglog_writeCStr("ending sub tick ");
	dbglog_writeNum(SubTick);
	dbglog_writeReturn();
#endif
	if (g_machine->config().emClassicSnd)
		if (auto* d = g_machine->findDevice<SoundDevice>()) d->subTick(SubTick);
	else if (g_machine->config().emASC)
		if (auto* d = g_machine->findDevice<ASCDevice>()) d->subTick(SubTick);
	else
		UnusedParam(SubTick);
}

#define CyclesScaledPerTick (130240UL * g_machine->config().clockMult * kCycleScale)
#define CyclesScaledPerSubTick (CyclesScaledPerTick / kNumSubTicks)

static uint16_t SubTickCounter;

static void SubTickTaskDo()
{
	SubTickNotify(SubTickCounter);
	++SubTickCounter;
	if (SubTickCounter < (kNumSubTicks - 1)) {
		/*
			final SubTick handled by SubTickTaskEnd,
			since CyclesScaledPerSubTick * kNumSubTicks
			might not equal CyclesScaledPerTick.
		*/

		ICT_add(kICT_SubTick, CyclesScaledPerSubTick);
	}
}

static void SubTickTaskStart()
{
	SubTickCounter = 0;
	ICT_add(kICT_SubTick, CyclesScaledPerSubTick);
}

static void SubTickTaskEnd()
{
	SubTickNotify(kNumSubTicks - 1);
}

static int ticksSinceSecond = 0;

static void SixtiethSecondNotify()
{
#if dbglog_HAVE && 0
	dbglog_WriteNote("begin new Sixtieth");
#endif
	if (++ticksSinceSecond >= 60) {
		ticksSinceSecond = 0;
		CurMacDateInSeconds++;
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

static void SixtiethEndNotify()
{
	SubTickTaskEnd();
	if (auto* d = g_machine->findDevice<MouseDevice>()) d->endTickNotify();
	if (auto* d = g_machine->findDevice<ScreenDevice>()) d->endTickNotify();
#if dbglog_HAVE && 0
	dbglog_WriteNote("end Sixtieth");
#endif
}

static void ExtraTimeBeginNotify()
{
#if 0
	dbglog_writeCStr("begin extra time");
	dbglog_writeReturn();
#endif
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->extraTimeBegin();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->extraTimeBegin();
}

static void ExtraTimeEndNotify()
{
	if (auto* d = g_machine->findDevice<VIA1Device>()) d->extraTimeEnd();
	if (auto* d = g_machine->findDevice<VIA2Device>()) d->extraTimeEnd();
#if 0
	dbglog_writeCStr("end extra time");
	dbglog_writeReturn();
#endif
}

bool EmulationReserveAlloc()
{
	const auto& cfg = g_machine->config();
	if (!AllocBlock(&RAM,
		cfg.ramSize() + RAMSafetyMarginFudge, false))
		return false;
	if (cfg.emVidCard)
		if (!AllocBlock(&VidROM, cfg.vidROMSize, false))
			return false;
	if (cfg.includeVidMem)
		if (!AllocBlock(&VidMem,
			cfg.vidMemSize + RAMSafetyMarginFudge, true))
			return false;
	return true;
}

void EmulationFreeAlloc()
{
	free(RAM); RAM = nullptr;
	free(VidROM); VidROM = nullptr;
	free(VidMem); VidMem = nullptr;
}

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

static void m68k_go_nCycles_1(uint32_t n)
{
	uint32_t n2;
	uint32_t StopiCount = g_ict.nextCount + n;
	do {
		g_ict.doCurrentTasks();
		n2 = g_ict.doGetNext(n);
#if dbglog_HAVE && 0
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

static uint32_t ExtraSubTicksToDo = 0;

static void DoEmulateOneTick()
{
#if EnableAutoSlow
	{
		uint32_t NewQuietTime = QuietTime + 1;

		if (NewQuietTime > QuietTime) {
			/* if not overflow */
			QuietTime = NewQuietTime;
		}
	}
#endif
#if EnableAutoSlow
	{
		uint32_t NewQuietSubTicks = QuietSubTicks + kNumSubTicks;

		if (NewQuietSubTicks > QuietSubTicks) {
			/* if not overflow */
			QuietSubTicks = NewQuietSubTicks;
		}
	}
#endif

	SixtiethSecondNotify();

	m68k_go_nCycles_1(CyclesScaledPerTick);

	SixtiethEndNotify();

	if ((uint8_t) -1 == SpeedValue) {
		ExtraSubTicksToDo = (uint32_t) -1;
	} else {
		uint32_t ExtraAdd = (kNumSubTicks << SpeedValue) - kNumSubTicks;
		uint32_t ExtraLimit = ExtraAdd << 3;

		ExtraSubTicksToDo += ExtraAdd;
		if (ExtraSubTicksToDo > ExtraLimit) {
			ExtraSubTicksToDo = ExtraLimit;
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

	if (ExtraSubTicksToDo > 0) {
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
#if EnableAutoSlow
			{
				uint32_t NewQuietSubTicks = QuietSubTicks + 1;

				if (NewQuietSubTicks > QuietSubTicks) {
					/* if not overflow */
					QuietSubTicks = NewQuietSubTicks;
				}
			}
#endif
			m68k_go_nCycles_1(CyclesScaledPerSubTick);
			--ExtraSubTicksToDo;
		} while (MoreSubTicksToDo());
		ExtraTimeEndNotify();
	}
}

static uint32_t CurEmulatedTime = 0;
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
		that TrueEmulatedTime says are due, without
		any cap or wall-clock gating.  This ensures
		the emulated tick count (and therefore the
		entire instruction stream) is deterministic
		regardless of host speed.
	*/

	int16_t n = (int16_t)(OnTrueTime - CurEmulatedTime);

	if (n > 0) {
		DoEmulateOneTick();
		++CurEmulatedTime;

		DoneWithDrawingForTick();

		if (--n > 0) {
			EmVideoDisable = true;

			do {
				DoEmulateOneTick();
				++CurEmulatedTime;
			} while (--n > 0);

			EmVideoDisable = false;
		}

		EmLagTime = 0;
	}
}

static void MainEventLoop()
{
	for (; ; ) {
		WaitForNextTick();
		if (ForceMacOff) {
			return;
		}

		RunEmulatedTicksToTrueTime();

		DoEmulateExtraTime();
	}
}

static std::unique_ptr<Machine> s_machine;
static LaunchConfig s_launchConfig;
static MachineConfig s_machineConfig;

const LaunchConfig& GetLaunchConfig()
{
	return s_launchConfig;
}

void ProgramEarlyInit(int argc, char* argv[])
{
	s_launchConfig = ParseCommandLine(argc, argv);

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
		SpeedValue = goldenHdr.speedValue;
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
		rc.speedValue = SpeedValue;
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
