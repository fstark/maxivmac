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
#include "core/diag.h"
#include "core/config_loader.h"
#include "core/rig.h"
#include "core/machine_config.h"
#include "core/emulator_config.h"
#include "core/ict_scheduler.h"
#include "core/state_recorder.hpp"
#include "core/md5.h"
#include "cpu/cpu.h"
#include "cpu/m68k.h"
#include "cpu/trap_tracer.h"
#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/symbols.h"
#include "lang/type_registry.h"
#include "lang/global_registry.h"

#include <cstring>
#include <memory>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

/*
	REPORT_ABNORMAL_ID unused 0x1002 - 0x10FF
*/

ICTScheduler g_ict;

// Reset memory, scheduler, and all devices to power-on state.
static void EmulatedHardwareZap()
{
	MemoryReset();
	g_ict.zap();
	if (auto *d = g_rig->findDevice<IWMDevice>()) d->reset();
	if (auto *d = g_rig->findDevice<SCCDevice>()) d->reset();
	if (auto *d = g_rig->findDevice<SCSIDevice>()) d->reset();
	if (auto *d = g_rig->findDevice<VIA1Device>()) d->zap();
	if (auto *d = g_rig->findDevice<VIA2Device>()) d->zap();
	if (auto *d = g_rig->findDevice<SonyDevice>()) d->reset();
	ExtnReset();
	g_cpu.reset();
}

static void DoMacReset()
{
	if (auto *d = g_rig->findDevice<SonyDevice>()) d->ejectAllDisks();
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

	if (g_wantMacInterrupt)
	{
		SetInterruptButton(true);
		g_wantMacInterrupt = false;
	}
	if (g_wantMacReset)
	{
		DoMacReset();
		g_wantMacReset = false;
	}
}

// Route audio sub-tick to the active sound device.
static void SubTickNotify(int SubTick)
{
	if (g_rig->config().emClassicSnd)
	{
		if (auto *d = g_rig->findDevice<SoundDevice>()) d->subTick(SubTick);
	}
	else if (g_rig->config().emASC)
	{
		if (auto *d = g_rig->findDevice<ASCDevice>()) d->subTick(SubTick);
	}
	else
	{
		UNUSED(SubTick);
	}
}

#define CYCLES_SCALED_PER_TICK (130240UL * g_rig->config().clockMult * kCycleScale)
#define CYCLES_SCALED_PER_SUB_TICK (CYCLES_SCALED_PER_TICK / kNumSubTicks)

static uint16_t s_subTickCounter;

// Advance sub-tick counter; reschedule unless final sub-tick.
static void SubTickTaskDo()
{
	SubTickNotify(s_subTickCounter);
	++s_subTickCounter;
	if (s_subTickCounter < (kNumSubTicks - 1))
	{
		/*
			final SubTick handled by SubTickTaskEnd,
			since CYCLES_SCALED_PER_SUB_TICK * kNumSubTicks
			might not equal CYCLES_SCALED_PER_TICK.
		*/

		g_ict.add(kICT_SubTick, CYCLES_SCALED_PER_SUB_TICK);
	}
}

static void SubTickTaskStart()
{
	s_subTickCounter = 0;
	g_ict.add(kICT_SubTick, CYCLES_SCALED_PER_SUB_TICK);
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
	if (++s_ticksSinceSecond >= 60)
	{
		s_ticksSinceSecond = 0;
		g_curMacDateInSeconds++;
	}
	if (auto *d = g_rig->findDevice<MouseDevice>()) d->update();
	InterruptReset_Update();
	if (g_rig->config().emClassicKbrd)
		if (auto *d = g_rig->findDevice<KeyboardDevice>()) d->update();
	if (g_rig->config().emADB)
		if (auto *d = g_rig->findDevice<ADBDevice>()) d->update();

	if (auto *d = g_rig->findDevice<VIA1Device>())
		d->iCA1_PulseNtfy(); /* Vertical Blanking Interrupt */
	if (auto *d = g_rig->findDevice<SonyDevice>()) d->update();

	if (auto *d = g_rig->findDevice<SCCDevice>()) d->serialTick();
#if EmLocalTalk
	if (auto *d = g_rig->findDevice<SCCDevice>()) d->localTalkTick();
#endif
	if (auto *d = g_rig->findDevice<RTCDevice>()) d->interrupt();
	if (g_rig->config().emVidCard)
		if (auto *d = g_rig->findDevice<VideoDevice>()) d->update();

	SubTickTaskStart();
}

// End-of-tick: flush final sub-tick, update mouse and screen.
static void SixtiethEndNotify()
{
	SubTickTaskEnd();
	if (auto *d = g_rig->findDevice<MouseDevice>()) d->endTickNotify();
	if (auto *d = g_rig->findDevice<ScreenDevice>()) d->endTickNotify();
#if 0
	dbglog_WriteNote("end Sixtieth");
#endif
}

static void ExtraTimeBeginNotify()
{
	if (auto *d = g_rig->findDevice<VIA1Device>()) d->extraTimeBegin();
	if (auto *d = g_rig->findDevice<VIA2Device>()) d->extraTimeBegin();
}

static void ExtraTimeEndNotify()
{
	if (auto *d = g_rig->findDevice<VIA1Device>()) d->extraTimeEnd();
	if (auto *d = g_rig->findDevice<VIA2Device>()) d->extraTimeEnd();
}

// Allocate RAM, VidROM, and VidMem buffers per machine config.
bool EmulationReserveAlloc()
{
	const auto &cfg = g_rig->config();
	if (!AllocBlock(&g_ram, cfg.ramSize() + RAMSafetyMarginFudge, false)) return false;
	if (cfg.emVidCard)
		if (!AllocBlock(&g_vidROM, cfg.vidROMSize, false)) return false;
	if (cfg.includeVidMem)
		if (!AllocBlock(&g_vidMem, cfg.vidMemSize + RAMSafetyMarginFudge, true)) return false;
	return true;
}

void EmulationFreeAlloc()
{
	free(g_ram);
	g_ram = nullptr;
	free(g_vidROM);
	g_vidROM = nullptr;
	free(g_vidMem);
	g_vidMem = nullptr;
}

/*
	Wire the ICT scheduler to the CPU, register all device
	task handlers, initialize RTC/g_rom/Video, build the
	address space, and perform hardware zap.
*/
bool InitEmulation()
{
	/* Load type definitions for structured memory display (must precede trap defs) */
	{
		auto &tr = g_typeRegistry();
		tr.init({get_vm_byte, get_vm_word, get_vm_long});
		int n = tr.load("data/debug/types.def");
		int e = tr.loadErrors("data/debug/errors.def");
		if (n > 0) std::fprintf(stderr, "type_registry: loaded %d types, %d error codes\n", n, e);
	}

	/* Load low-memory global definitions (must follow type registry) */
	{
		int n = g_globalRegistry().load("data/debug/globals.def", g_typeRegistry());
		if (n > 0) std::fprintf(stderr, "global_registry: loaded %d globals\n", n);
	}

	/* Load external trap definitions for the hierarchical tracer */
	{
		int n = g_trapDefs.load("data/debug/traps.def");
		int e = g_trapDefs.loadErrors("data/debug/errors.def");
		if (n > 0) std::fprintf(stderr, "trap_defs: loaded %d traps, %d error codes\n", n, e);
	}

	/* Print symbol counts to the debugger console, now that all registries are loaded */
	if (auto *dbg = Debugger::instance())
	{
		dbg->io().write("Loaded %d trap symbols, %d low-memory globals\n", SymbolsTrapCount(),
						SymbolsGlobalCount());
	}

	/* Wire ICT scheduler to CPU cycle counters */
	g_ict.setCycleAccessors([]() { return g_cpu.getCyclesRemaining(); },
							[](int32_t n) { g_cpu.setCyclesRemaining(n); });

	/* Register ICT task handlers */
	g_ict.registerTask(kICT_SubTick, SubTickTaskDo);
	if (g_rig->config().emClassicKbrd)
	{
		g_ict.registerTask(kICT_Kybd_ReceiveEndCommand,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<KeyboardDevice>())
								   d->receiveEndCommand();
						   });
		g_ict.registerTask(kICT_Kybd_ReceiveCommand,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<KeyboardDevice>())
								   d->receiveCommand();
						   });
	}
	if (g_rig->config().emADB)
		g_ict.registerTask(kICT_ADB_NewState,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<ADBDevice>()) d->doNewState();
						   });
	if (g_rig->config().emPMU)
		g_ict.registerTask(kICT_PMU_Task,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<PMUDevice>()) d->doTask();
						   });
	if (g_rig->config().emVIA1)
	{
		g_ict.registerTask(kICT_VIA1_Timer1Check,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<VIA1Device>()) d->doTimer1Check();
						   });
		g_ict.registerTask(kICT_VIA1_Timer2Check,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<VIA1Device>()) d->doTimer2Check();
						   });
	}
	if (g_rig->config().emVIA2)
	{
		g_ict.registerTask(kICT_VIA2_Timer1Check,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<VIA2Device>()) d->doTimer1Check();
						   });
		g_ict.registerTask(kICT_VIA2_Timer2Check,
						   []()
						   {
							   if (auto *d = g_rig->findDevice<VIA2Device>()) d->doTimer2Check();
						   });
	}

	bool ok = true;
	if (ok && g_rig->config().emRTC)
	{
		auto *rtc = g_rig->findDevice<RTCDevice>();
		ok = rtc ? rtc->init() : false;
	}
	if (ok)
	{
		auto *rom = g_rig->findDevice<ROMDevice>();
		ok = rom ? rom->init() : false;
	}
	if (ok && g_rig->config().emVidCard)
	{
		auto *vid = g_rig->findDevice<VideoDevice>();
		ok = vid ? vid->init() : false;
	}
	if (ok) ok = AddrSpac_Init();
	if (ok)
	{
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
	do
	{
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

		if (NewQuietTime > g_quietTime)
		{
			/* if not overflow */
			g_quietTime = NewQuietTime;
		}
	}
	{
		uint32_t NewQuietSubTicks = g_quietSubTicks + kNumSubTicks;

		if (NewQuietSubTicks > g_quietSubTicks)
		{
			/* if not overflow */
			g_quietSubTicks = NewQuietSubTicks;
		}
	}

	SixtiethSecondNotify();

	m68k_go_nCycles_1(CYCLES_SCALED_PER_TICK);

	SixtiethEndNotify();

	if ((uint8_t)-1 == g_speedValue)
	{
		s_extraSubTicksToDo = (uint32_t)-1;
	}
	else
	{
		uint32_t ExtraAdd = (kNumSubTicks << g_speedValue) - kNumSubTicks;
		uint32_t ExtraLimit = ExtraAdd << 3;

		s_extraSubTicksToDo += ExtraAdd;
		if (s_extraSubTicksToDo > ExtraLimit)
		{
			s_extraSubTicksToDo = ExtraLimit;
		}
	}
}

static bool MoreSubTicksToDo()
{
	return s_extraSubTicksToDo > 0;
}

void DoEmulateExtraTime()
{
	/* Run extra sub-ticks for speed multiplier (anything over 1x).
	   VIA timers are frozen for the duration. */

	if (MoreSubTicksToDo())
	{
		ExtraTimeBeginNotify();
		do
		{
			{
				uint32_t NewQuietSubTicks = g_quietSubTicks + 1;

				if (NewQuietSubTicks > g_quietSubTicks)
				{
					/* if not overflow */
					g_quietSubTicks = NewQuietSubTicks;
				}
			}
			m68k_go_nCycles_1(CYCLES_SCALED_PER_SUB_TICK);
			--s_extraSubTicksToDo;
		} while (MoreSubTicksToDo());
		ExtraTimeEndNotify();
	}
}

void RunEmulatedTicksToTrueTime()
{
	/* Run exactly one tick plus its extra-time sub-ticks (speed
	   multiplier).  The backend calls this once per tick; there
	   is no catch-up loop.  If the host falls behind wall-clock
	   time, ticks are simply dropped. */

	DoEmulateOneTick();
	DoEmulateExtraTime();

	DoneWithDrawingForTick();
}

/* MainEventLoop has been removed — the backend now drives the loop. */

static std::unique_ptr<Rig> s_rig;
static LaunchConfig s_launchConfig;
static MachineConfig s_machineConfig;
static EmulatorConfig s_emulatorConfig;

const LaunchConfig &GetLaunchConfig()
{
	return s_launchConfig;
}

void SetLaunchConfig(const LaunchConfig &lc)
{
	/* Merge selector choices onto the CLI-parsed config.
	   The selector only sets model-specific fields (model, rom, ram,
	   speed, disks).  CLI flags like --silent, --romdir, --title,
	   --fullscreen, --scale must survive. */
	s_launchConfig.model = lc.model;
	s_launchConfig.modelExplicit = lc.modelExplicit;
	s_launchConfig.romPath = lc.romPath;
	s_launchConfig.ramMB = lc.ramMB;
	s_launchConfig.speed = lc.speed;
	s_launchConfig.diskPaths = lc.diskPaths;

	s_machineConfig = BuildMachineConfig(s_launchConfig);
	s_emulatorConfig = BuildEmulatorConfig(s_launchConfig);
	s_rig = std::make_unique<Rig>(s_machineConfig);
	s_rig->init();
}

const EmulatorConfig &GetEmulatorConfig()
{
	return s_emulatorConfig;
}

EmulatorConfig &GetEmulatorConfigMut()
{
	return s_emulatorConfig;
}

/*
	Parse CLI args and set up non-model-dependent state.
	When a model is known (--model or --verify), also creates the Rig.
	When no model is specified, the Rig is created later by
	SetLaunchConfig() after the user picks one in the selector.
*/
void ProgramEarlyInit(int argc, char *argv[])
{
	/* Intercept "debug" subcommand before normal arg parsing */
	if (argc >= 2 && std::strcmp(argv[1], "debug") == 0)
		std::exit(DebugClientMain(argc - 1, argv + 1));

	s_launchConfig = ParseCommandLine(argc, argv);
	s_emulatorConfig = BuildEmulatorConfig(s_launchConfig);

	if (s_launchConfig.help)
	{
		PrintUsage(argv[0]);
		// Help flag will be checked by caller to exit early
	}

	/* Set up instruction-logging window from CLI args */
	if (s_launchConfig.logCount > 0)
	{
		g_logStart = s_launchConfig.logStart;
		g_logEnd = s_launchConfig.logStart + s_launchConfig.logCount;
	}

	/* Initialize debugger if requested */
	if (s_launchConfig.debugger)
	{
		Debugger::create();
		g_debuggerActive = true;
	}
	else if (!s_launchConfig.debugServerPath.empty())
	{
#ifndef _WIN32
		auto path = s_launchConfig.debugServerPath;
		if (path == "auto") path = "/tmp/maxivmac-dbg-" + std::to_string(getpid()) + ".sock";
		int listenFd = CreateListenSocket(path);
		if (listenFd < 0) std::exit(1);
		std::fprintf(stderr, "debugserver: listening on %s\n", path.c_str());
		Debugger::create(CreateSocketIO(listenFd).release());
		g_debuggerActive = true;
#else
		std::fprintf(stderr, "Debug server is not supported on Windows.\n");
		std::exit(1);
#endif
	}

	/* No model specified — defer machine creation to SetLaunchConfig().
	   --verify implies a model (read from the golden header below). */
	if (!s_launchConfig.modelExplicit && s_launchConfig.verifyPath.empty()) return;

	/* When verifying, source emulation params from the golden file */
	StateRecorder::HeaderInfo goldenHdr;
	bool haveGoldenHdr = false;
	if (!s_launchConfig.verifyPath.empty())
	{
		if (!StateRecorder::readHeader(s_launchConfig.verifyPath, goldenHdr))
		{
			std::fprintf(stderr, "Cannot read golden file header, aborting.\n");
			std::exit(1);
		}
		haveGoldenHdr = true;
		s_launchConfig.model = static_cast<MacModel>(goldenHdr.modelId);
		s_launchConfig.modelExplicit = true;
		g_speedValue = goldenHdr.speedValue;
		g_skipThrottle = true;
	}

	/* Set up StateRecorder from CLI args */
	s_machineConfig = BuildMachineConfig(s_launchConfig);

	/* Override MachineConfig with exact values from golden header */
	if (haveGoldenHdr)
	{
		uint32_t ram = goldenHdr.ramSize;
		if (s_machineConfig.ramBSize > 0)
		{
			s_machineConfig.ramASize = ram / 2;
			s_machineConfig.ramBSize = ram / 2;
		}
		else
		{
			s_machineConfig.ramASize = ram;
		}
		s_machineConfig.screenWidth = goldenHdr.screenWidth;
		s_machineConfig.screenHeight = goldenHdr.screenHeight;
		s_machineConfig.screenDepth = goldenHdr.screenDepth;
	}

	{
		StateRecorder::Config rc;

		if (!s_launchConfig.recordPath.empty())
		{
			rc.mode = RecorderMode::Record;
			rc.goldenPath = s_launchConfig.recordPath;
		}
		else if (!s_launchConfig.verifyPath.empty())
		{
			rc.mode = RecorderMode::Verify;
			rc.goldenPath = s_launchConfig.verifyPath;
		}

		if (!s_launchConfig.tracePath.empty())
		{
			rc.textLog = TextLog::CpuAndIo;
			rc.textPath = s_launchConfig.tracePath;
		}
		else if (!s_launchConfig.traceCpuPath.empty())
		{
			rc.textLog = TextLog::CpuOnly;
			rc.textPath = s_launchConfig.traceCpuPath;
		}

		if (s_launchConfig.snapshotInterval > 0)
			rc.snapshotInterval = s_launchConfig.snapshotInterval;
		if (s_launchConfig.maxInstructions > 0) rc.maxInstructions = s_launchConfig.maxInstructions;

		rc.modelId = static_cast<uint32_t>(s_launchConfig.model);
		rc.speedValue = g_speedValue;
		rc.ramSize = s_machineConfig.ramSize();
		rc.screenWidth = s_machineConfig.screenWidth;
		rc.screenHeight = s_machineConfig.screenHeight;
		rc.screenDepth = s_machineConfig.screenDepth;

		// Hash ROM file
		std::string resolvedRom =
			ResolveRomPath(s_launchConfig.romPath, s_launchConfig.model, s_launchConfig.romDir);
		if (!resolvedRom.empty()) md5_file(resolvedRom.c_str(), rc.romHash);
		// Hash first disk
		if (!s_launchConfig.diskPaths.empty())
			md5_file(s_launchConfig.diskPaths[0].c_str(), rc.diskHash);

		if (rc.mode != RecorderMode::Off || rc.textLog != TextLog::None)
		{
			if (!g_recorder.init(rc))
			{
				std::fprintf(stderr, "StateRecorder init failed, aborting.\n");
				std::exit(1);
			}
		}
	}

	s_rig = std::make_unique<Rig>(std::move(s_machineConfig));
	s_rig->init();
}

void ProgramCleanup()
{
	s_rig.reset();
}

bool ProgramMain()
{
	if (!InitEmulation()) return false;

	if (s_launchConfig.traceTraps)
	{
		g_tracer.addAllTraps();
		g_tracer.enable(true);
	}

	/* Apply --diag= subsystem flags */
	if (!s_launchConfig.diagSubsystems.empty())
	{
		if (s_launchConfig.diagSubsystems == "all")
		{
			Diag().setAll(true);
		}
		else
		{
			/* Parse comma-separated list */
			std::string list = s_launchConfig.diagSubsystems;
			size_t pos = 0;
			while (pos < list.size())
			{
				size_t comma = list.find(',', pos);
				if (comma == std::string::npos) comma = list.size();
				std::string name = list.substr(pos, comma - pos);
				DiagSubsystem s;
				if (DiagConfig::fromName(name.c_str(), s))
					Diag().set(s, true);
				else
					fprintf(stderr, "Warning: unknown diag subsystem '%s'\n", name.c_str());
				pos = comma + 1;
			}
		}
	}

	/* Execute startup debug scripts (after registries so trap names resolve) */
	if (auto *dbg = Debugger::instance())
	{
		for (auto &script : s_launchConfig.dbgScripts)
			SourceFile(*dbg, script);
	}

	return true;
}
