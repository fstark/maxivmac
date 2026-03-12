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
#include "core/machine_obj.h"
#include "core/machine_config.h"
#include "core/ict_scheduler.h"
#include "cpu/cpu.h"

#include <memory>

/*
	ReportAbnormalID unused 0x1002 - 0x10FF
*/

ICTScheduler g_ict;

static void EmulatedHardwareZap(void)
{
	Memory_Reset();
	ICT_Zap();
	IWM_Reset();
	SCC_Reset();
	SCSI_Reset();
	if (g_machine->config().emVIA1) VIA1_Zap();
	if (g_machine->config().emVIA2) VIA2_Zap();
	Sony_Reset();
	Extn_Reset();
	g_cpu.reset();
}

static void DoMacReset(void)
{
	Sony_EjectAllDisks();
	EmulatedHardwareZap();
}

static void InterruptReset_Update(void)
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
		MacSound_SubTick(SubTick);
	else if (g_machine->config().emASC)
		ASC_SubTick(SubTick);
	else
		UnusedParam(SubTick);
}

#define CyclesScaledPerTick (130240UL * kMyClockMult * kCycleScale)
#define CyclesScaledPerSubTick (CyclesScaledPerTick / kNumSubTicks)

static uint16_t SubTickCounter;

static void SubTickTaskDo(void)
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

static void SubTickTaskStart(void)
{
	SubTickCounter = 0;
	ICT_add(kICT_SubTick, CyclesScaledPerSubTick);
}

static void SubTickTaskEnd(void)
{
	SubTickNotify(kNumSubTicks - 1);
}

static void SixtiethSecondNotify(void)
{
#if dbglog_HAVE && 0
	dbglog_WriteNote("begin new Sixtieth");
#endif
	Mouse_Update();
	InterruptReset_Update();
	if (g_machine->config().emClassicKbrd) KeyBoard_Update();
	if (g_machine->config().emADB) ADB_Update();

	Sixtieth_PulseNtfy(); /* Vertical Blanking Interrupt */
	Sony_Update();

#if EmLocalTalk
	LocalTalkTick();
#endif
	if (g_machine->config().emRTC) RTC_Interrupt();
	if (g_machine->config().emVidCard) Vid_Update();

	SubTickTaskStart();
}

static void SixtiethEndNotify(void)
{
	SubTickTaskEnd();
	Mouse_EndTickNotify();
	Screen_EndTickNotify();
#if dbglog_HAVE && 0
	dbglog_WriteNote("end Sixtieth");
#endif
}

static void ExtraTimeBeginNotify(void)
{
#if 0
	dbglog_writeCStr("begin extra time");
	dbglog_writeReturn();
#endif
	if (g_machine->config().emVIA1) VIA1_ExtraTimeBegin();
	if (g_machine->config().emVIA2) VIA2_ExtraTimeBegin();
}

static void ExtraTimeEndNotify(void)
{
	if (g_machine->config().emVIA1) VIA1_ExtraTimeEnd();
	if (g_machine->config().emVIA2) VIA2_ExtraTimeEnd();
#if 0
	dbglog_writeCStr("end extra time");
	dbglog_writeReturn();
#endif
}

void EmulationReserveAlloc(void)
{
	const auto& cfg = g_machine->config();
	ReserveAllocOneBlock(&RAM,
		cfg.ramSize() + RAMSafetyMarginFudge, 5, false);
	if (cfg.emVidCard)
		ReserveAllocOneBlock(&VidROM, cfg.vidROMSize, 5, false);
	if (cfg.includeVidMem)
		ReserveAllocOneBlock(&VidMem,
			cfg.vidMemSize + RAMSafetyMarginFudge, 5, true);
#if SmallGlobals
	g_cpu.reserveAlloc();
#endif
}

static bool InitEmulation(void)
{
	/* Wire ICT scheduler to CPU cycle counters */
	g_ict.setCycleAccessors(
		[]() { return g_cpu.getCyclesRemaining(); },
		[](int32_t n) { g_cpu.setCyclesRemaining(n); }
	);

	/* Register ICT task handlers */
	g_ict.registerTask(kICT_SubTick, SubTickTaskDo);
	if (g_machine->config().emClassicKbrd) {
		g_ict.registerTask(kICT_Kybd_ReceiveEndCommand, DoKybd_ReceiveEndCommand);
		g_ict.registerTask(kICT_Kybd_ReceiveCommand, DoKybd_ReceiveCommand);
	}
	if (g_machine->config().emADB)
		g_ict.registerTask(kICT_ADB_NewState, ADB_DoNewState);
	if (g_machine->config().emPMU)
		g_ict.registerTask(kICT_PMU_Task, PMU_DoTask);
	if (g_machine->config().emVIA1) {
		g_ict.registerTask(kICT_VIA1_Timer1Check, VIA1_DoTimer1Check);
		g_ict.registerTask(kICT_VIA1_Timer2Check, VIA1_DoTimer2Check);
	}
	if (g_machine->config().emVIA2) {
		g_ict.registerTask(kICT_VIA2_Timer1Check, VIA2_DoTimer1Check);
		g_ict.registerTask(kICT_VIA2_Timer2Check, VIA2_DoTimer2Check);
	}

	bool ok = true;
	if (ok && g_machine->config().emRTC) ok = RTC_Init();
	if (ok) ok = ROM_Init();
	if (ok && g_machine->config().emVidCard) ok = Vid_Init();
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

static void DoEmulateOneTick(void)
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

static bool MoreSubTicksToDo(void)
{
	bool v = false;

	if (ExtraTimeNotOver() && (ExtraSubTicksToDo > 0)) {
#if EnableAutoSlow
		if ((QuietSubTicks >= kAutoSlowSubTicks)
			&& (QuietTime >= kAutoSlowTime)
			&& ! WantNotAutoSlow)
		{
			ExtraSubTicksToDo = 0;
		} else
#endif
		{
			v = true;
		}
	}

	return v;
}

static void DoEmulateExtraTime(void)
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

static void RunEmulatedTicksToTrueTime(void)
{
	/*
		The general idea is to call DoEmulateOneTick
		once per tick.

		But if emulation is lagging, we'll try to
		catch up by calling DoEmulateOneTick multiple
		times, unless we're too far behind, in
		which case we forget it.

		If emulating one tick takes longer than
		a tick we don't want to sit here
		forever. So the maximum number of calls
		to DoEmulateOneTick is determined at
		the beginning, rather than just
		calling DoEmulateOneTick until
		CurEmulatedTime >= TrueEmulatedTime.
	*/

	int8_t n = OnTrueTime - CurEmulatedTime;

	if (n > 0) {
		DoEmulateOneTick();
		++CurEmulatedTime;

		DoneWithDrawingForTick();

		if (n > 8) {
			/* emulation not fast enough */
			n = 8;
			CurEmulatedTime = OnTrueTime - n;
		}

		if (ExtraTimeNotOver() && (--n > 0)) {
			/* lagging, catch up */

			EmVideoDisable = true;

			do {
				DoEmulateOneTick();
				++CurEmulatedTime;
			} while (ExtraTimeNotOver()
				&& (--n > 0));

			EmVideoDisable = false;
		}

		EmLagTime = n;
	}
}

static void MainEventLoop(void)
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

void ProgramEarlyInit(void)
{
	MachineConfig config = MachineConfigForModel(MacModel::II);
	s_machine = std::make_unique<Machine>(std::move(config));
	s_machine->init();
}

void ProgramCleanup(void)
{
	s_machine.reset();
}

void ProgramMain(void)
{
	if (InitEmulation())
	{
		MainEventLoop();
	}
}
