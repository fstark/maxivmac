/*
	maxivmac INIT — init.c
	INIT entry point, unified jGNEFilter, trap stub generation,
	and boot-time initialisation.
*/

#include "defs.h"
#include "version.h"

/* ================================================================ */
/*            Dynamic 68k stub generation                           */
/* ================================================================ */

/*
	Generate a small 68k code stub in the system heap for one
	flat-file OS trap. The stub:

	  MOVEM.L D0-D2/A0-A1, -(SP)  ; save regs (20 bytes)
	  MOVE.W  D1, -(SP)            ; push trapWord (D1.W set by ROM dispatcher)
	  MOVE.L  A0, -(SP)            ; push pb arg
	  JSR     dispatchAddr          ; call DispatchFlat
	  ADDQ.L  #6, SP               ; pop args
	  TST.W   D0                   ; handled?
	  BNE.S   @pass                ; no — pass through
	  MOVEM.L (SP)+, D0-D2/A0-A1  ; restore regs
	  MOVE.W  16(A0), D0           ; D0 = ioResult
	  RTS                          ; return to caller
	@pass:
	  MOVEM.L (SP)+, D0-D2/A0-A1  ; restore regs
	  MOVE.L  #oldAddr, -(SP)     ; push old trap address
	  RTS                          ; jump to original

	Byte layout:
	  0: MOVEM.L D0-D2/A0-A1,-(SP) 48E7 E0C0       4
	  4: MOVE.W  D1,-(SP)           3F01            2
	  6: MOVE.L  A0,-(SP)           2F08            2
	  8: JSR     abs.L              4EB9 xxxx xxxx  6
	 14: ADDQ.L  #6,SP              5C8F            2
	 16: TST.W   D0                 4A40            2
	 18: BNE.S   +10                660A            2
	 20: MOVEM.L (SP)+,D0-D2/A0-A1 4CDF 0307       4
	 24: MOVE.W  16(A0),D0         3028 0010       4
	 28: RTS                        4E75            2
	 30: MOVEM.L (SP)+,D0-D2/A0-A1 4CDF 0307       4
	 34: MOVE.L  #imm,-(SP)         2F3C xxxx xxxx  6
	 40: RTS                        4E75            2
	 Total: 42 bytes
*/
static Ptr MakeFlatStub(long dispatchAddr, long oldAddr)
{
	Ptr p;
	short *w;

	p = NewPtrSys(42);
	if (p == NULL) return NULL;
	w = (short *)p;

	/* MOVEM.L D0-D2/A0-A1, -(SP) */
	*w++ = 0x48E7;
	*w++ = (short)0xE0C0;

	/* MOVE.W D1, -(SP) — push trap word from D1 */
	*w++ = 0x3F01;

	/* MOVE.L A0, -(SP) */
	*w++ = 0x2F08;

	/* JSR dispatchAddr (absolute long) */
	*w++ = 0x4EB9;
	*(long *)w = dispatchAddr;
	w += 2;

	/* ADDQ.L #6, SP */
	*w++ = 0x5C8F;

	/* TST.W D0 */
	*w++ = 0x4A40;

	/* BNE.S @pass — displacement +10 = $0A (skip 4+4+2 bytes) */
	*w++ = 0x660A;

	/* MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF;
	*w++ = 0x0307;

	/* MOVE.W 16(A0), D0 — ioResult */
	*w++ = 0x3028;
	*w++ = 0x0010;

	/* RTS */
	*w++ = 0x4E75;

	/* @pass: MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF;
	*w++ = 0x0307;

	/* MOVE.L #oldAddr, -(SP) */
	*w++ = 0x2F3C;
	*(long *)w = oldAddr;
	w += 2;

	/* RTS (= JMP to old trap) */
	*w++ = 0x4E75;

	return p;
}

/*
	Generate an _HFSDispatch stub. Same structure as flat stub
	but reads D0.W as the HFS selector instead of D1.W.

	Byte layout:
	  0: MOVEM.L D0-D2/A0-A1,-(SP) 48E7 E0C0       4
	  4: MOVE.W  D0,-(SP)           3F00            2
	  6: MOVE.L  A0,-(SP)           2F08            2
	  8: JSR     abs.L              4EB9 xxxx xxxx  6
	 14: ADDQ.L  #6,SP              5C8F            2
	 16: TST.W   D0                 4A40            2
	 18: BNE.S   +10                660A            2
	 20: MOVEM.L (SP)+,D0-D2/A0-A1 4CDF 0307       4
	 24: MOVE.W  16(A0),D0         3028 0010       4
	 28: RTS                        4E75            2
	 30: MOVEM.L (SP)+,D0-D2/A0-A1 4CDF 0307       4
	 34: MOVE.L  #imm,-(SP)         2F3C xxxx xxxx  6
	 40: RTS                        4E75            2
	 Total: 42 bytes
*/
static Ptr MakeHFSStub(long dispatchAddr, long oldAddr)
{
	Ptr p;
	short *w;

	p = NewPtrSys(42);
	if (p == NULL) return NULL;
	w = (short *)p;

	/* MOVEM.L D0-D2/A0-A1, -(SP) */
	*w++ = 0x48E7;
	*w++ = (short)0xE0C0;

	/* MOVE.W D0, -(SP) — push selector */
	*w++ = 0x3F00;

	/* MOVE.L A0, -(SP) — push pb */
	*w++ = 0x2F08;

	/* JSR dispatchAddr */
	*w++ = 0x4EB9;
	*(long *)w = dispatchAddr;
	w += 2;

	/* ADDQ.L #6, SP */
	*w++ = 0x5C8F;

	/* TST.W D0 */
	*w++ = 0x4A40;

	/* BNE.S @pass — displacement +10 = $0A */
	*w++ = 0x660A;

	/* MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF;
	*w++ = 0x0307;

	/* MOVE.W 16(A0), D0 — ioResult */
	*w++ = 0x3028;
	*w++ = 0x0010;

	/* RTS */
	*w++ = 0x4E75;

	/* @pass: MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF;
	*w++ = 0x0307;

	/* MOVE.L #oldAddr, -(SP) */
	*w++ = 0x2F3C;
	*(long *)w = oldAddr;
	w += 2;

	/* RTS */
	*w++ = 0x4E75;

	return p;
}

/* ================================================================ */
/*                         Trap installation                        */
/* ================================================================ */

/*
	Install one flat-file trap patch.
	trapWord is the full trap word ($A0xx), e.g. $A000 for _Open.
*/
static void InstallFlatPatch(unsigned short trapWord, char *regBase)
{
	long oldAddr;
	long dispAddr;
	Ptr stub;

	oldAddr = (long)NGetTrapAddress(trapWord, OSTrap);
	dispAddr = (long)DispatchFlat;
	stub = MakeFlatStub(dispAddr, oldAddr);
	if (stub == NULL)
	{
		dbg_log1(regBase, "INIT: stub alloc failed for %lx", (long)trapWord);
		return;
	}
	NSetTrapAddress((long)stub, trapWord, OSTrap);
}

static void InstallHFSPatch(char *regBase)
{
	long oldAddr;
	long dispAddr;
	Ptr stub;

	oldAddr = (long)NGetTrapAddress(0xA260, OSTrap);
	dispAddr = (long)DispatchHFS;
	stub = MakeHFSStub(dispAddr, oldAddr);
	if (stub == NULL)
	{
		dbg_log(regBase, "INIT: HFS stub alloc failed");
		return;
	}
	NSetTrapAddress((long)stub, 0xA260, OSTrap);
	dbg_log1(regBase, "INIT: HFS patch at %lx", (long)stub);
}

/* ================================================================ */
/*                 Polling helpers (called from filter)             */
/* ================================================================ */

static void PollMounts(Globals *g)
{
	reg_command(g->regBase, kExtFSPollMount);
	{
		unsigned long slot = reg_get(g->regBase, 0);
		if (slot != 0xFFFFFFFFUL)
		{
			short s = (short)slot;
			short vRefNum = (short)reg_get(g->regBase, 1);
			short driveNum = (short)reg_get(g->regBase, 2);
			MountNewDrive(g, s, vRefNum, driveNum);
			PostEvent(diskEvt, driveNum);
		}
	}
}

static void PollGuestCmd(Globals *g)
{
	Str255 pathBuf;
	struct
	{
		Ptr namePtr;  /* pointer to Pascal string filename */
		short config; /* sound/screen buffer config (0=normal) */
	} launchPB;

	pathBuf[0] = 0;
	reg_set(g->regBase, 0, (unsigned long)pathBuf);
	reg_command(g->regBase, kExtFSGuestCmd);
	{
		unsigned short cmd = reg_result(g->regBase);
		if (cmd == 1)
		{
			/* Split path at last ':' — SetVol to parent dir,
			   then _Launch with just the filename.
			   e.g. "Macintosh:Think C:THINK C 5.0"
			   -> SetVol("Macintosh:Think C")
			   -> _Launch("THINK C 5.0") */
			short lastColon = 0;
			short i;
			for (i = 1; i <= pathBuf[0]; i++)
				if (pathBuf[i] == ':') lastColon = i;

			if (lastColon > 0)
			{
				Str255 dirPath;
				Str255 appName;
				/* dirPath = everything up to (not including) the last colon */
				dirPath[0] = lastColon - 1;
				for (i = 1; i < lastColon; i++)
					dirPath[i] = pathBuf[i];
				/* appName = everything after the last colon */
				appName[0] = pathBuf[0] - lastColon;
				for (i = 1; i <= appName[0]; i++)
					appName[i] = pathBuf[lastColon + i];

				/* Set working directory to app's parent folder */
				{
					HParamBlockRec vpb;
					mem_zero((char *)&vpb, sizeof(vpb));
					vpb.volumeParam.ioNamePtr = (StringPtr)dirPath;
					vpb.volumeParam.ioVRefNum = 0;
					PBHSetVol(&vpb, false);
				}

				/* Launch with just the filename */
				launchPB.namePtr = (Ptr)appName;
				launchPB.config = 0;
				asm {
					LEA     launchPB, A0
					DC.W    0xA9F2  ; _Launch
				}
			}
			else
			{
				/* No colon — bare filename, use current WD */
				launchPB.namePtr = (Ptr)pathBuf;
				launchPB.config = 0;
				asm {
					LEA     launchPB, A0
					DC.W    0xA9F2  ; _Launch
				}
			}
		}
		else if (cmd == 2)
		{
			/* Terminate current application, return to Finder. */
			ExitToShell();
		}
		else if (cmd == 3)
		{
			/* Clean shutdown — on Plus/SE shows "safe to turn off",
			   on Mac II powers off via ADB power manager. */
			ShutDwnPower();
		}
	}
}

/* ================================================================ */
/*                    Unified jGNEFilter                            */
/* ================================================================ */

void FilterEntry(void)
{
	long oldFilter;

	asm { MOVEM.L D0-D2/A0-A2, -(SP) }
	SetUpA4();

	{
		Globals *g = get_globals();
		if (g != NULL)
		{
			/* Clipboard sync (internally throttled to 30 ticks) */
			SyncClipboard(g);

			/* Mount polling + guest commands (every tick) */
			if (TickCount() != g->lastPollTick)
			{
				g->lastPollTick = TickCount();
				PollMounts(g);
				PollGuestCmd(g);
			}
		}
		oldFilter = (g != NULL) ? g->oldFilter : 0;
	}

	RestoreA4();
	asm { MOVEM.L (SP)+, D0-D2/A0-A2 }

	/* Chain to previous filter */
	if (oldFilter != 0)
	{
		asm {
			MOVE.L  oldFilter, A0
			UNLK    A6
			JMP     (A0)
		}
	}
}

/* ================================================================ */
/*                         INIT entry point                         */
/* ================================================================ */

static Boolean ShiftKeyDown(void)
{
	KeyMap keys;
	GetKeys(keys);
	/* Shift = bit 56 of the KeyMap (byte 7, bit 0) */
	return (((unsigned char *)keys)[7] & 0x01) != 0;
}

void main(void)
{
	char *regBase;
	Globals *g;
	Handle self;
	Ptr myINITPtr;
	SysEnvRec env;
	FCBPBRec fcbPB;
	Str63 initName;
	short homeRef;
	unsigned long slot;

	asm { move.l a0, myINITPtr }
	RememberA0();
	SetUpA4();
	DriveRememberA4();

	regBase = find_reg_base();
	if (regBase == NULL) goto bail;

	/* Shift-skip: standard Mac INIT convention */
	if (ShiftKeyDown())
	{
		dbg_log(regBase, "maxivmac INIT: skipped by user (Shift held)");
		goto bail;
	}

	/* Locate our own resource and capture file info BEFORE detach */
	self = GetResource('INIT', kInitResID);
	if (self == NULL)
	{
		dbg_log(regBase, "maxivmac INIT: GetResource failed");
		goto bail;
	}
	homeRef = HomeResFile(self);

	/* Get file spec via PBGetFCBInfo */
	initName[0] = 0;
	mem_zero((char *)&fcbPB, sizeof(fcbPB));
	fcbPB.ioNamePtr = (StringPtr)initName;
	fcbPB.ioRefNum = homeRef;
	fcbPB.ioFCBIndx = 0;  /* use ioRefNum, not index */
	PBGetFCBInfoSync(&fcbPB);

	/* Detach and lock the code resource */
	DetachResource(self);
	HLock(self);
	HNoPurge(self);

	/* Get guest environment */
	SysEnvirons(1, &env);

	dbg_log1(regBase, "maxivmac INIT: regBase=%lx", (unsigned long)regBase);

	/* Allocate globals */
	g = (Globals *)NewPtrSysClear(sizeof(Globals));
	if (g == NULL)
	{
		dbg_log(regBase, "maxivmac INIT: NewPtrSys failed");
		goto bail;
	}
	g->regBase = regBase;
	g->driveCount = 0;
	g->initVRefNum = fcbPB.ioFCBVRefNum;
	g->initDirID = fcbPB.ioFCBParID;
	pstr_copy((char *)g->initFileName, (char *)initName);

	/* Save A4 */
	{
		long *a4dst = &g->savedA4;
		asm {
			move.l a4dst, a0
			move.l a4, (a0)
		}
	}

	set_globals(regBase, g);

	/* ---- Identify to host ---- */
	{
		static char versionStr[] = kInitVersion;

		reg_set(regBase, 0, (unsigned long)kApiVersion);
		reg_set(regBase, 1, (unsigned long)versionStr);
		reg_set(regBase, 2, (unsigned long)g->initVRefNum);
		reg_set(regBase, 3, (unsigned long)g->initDirID);
		reg_set(regBase, 4, (unsigned long)g->initFileName);
		reg_set(regBase, 5, (unsigned long)env.machineType);
		reg_set(regBase, 6, (unsigned long)env.systemVersion);
		reg_command(regBase, kCmdInitIdent);

		if (reg_result(regBase) != 0)
		{
			dbg_log(regBase, "maxivmac INIT: API rejected by host, bailing");
			goto bail;
		}
	}

	dbg_log1(regBase, "maxivmac INIT: version=%s",
			 (unsigned long)kInitVersion + 1);  /* skip Pascal length byte */

	/* ---- Install traps (unconditional) ---- */
	InitTrapTables();
	InstallHFSPatch(regBase);
	InstallFlatPatch(0xA000, regBase); /* _Open */
	InstallFlatPatch(0xA001, regBase); /* _Close */
	InstallFlatPatch(0xA002, regBase); /* _Read */
	InstallFlatPatch(0xA003, regBase); /* _Write */
	InstallFlatPatch(0xA007, regBase); /* _GetVolInfo */
	InstallFlatPatch(0xA008, regBase); /* _Create */
	InstallFlatPatch(0xA009, regBase); /* _Delete */
	InstallFlatPatch(0xA00A, regBase); /* _OpenRF */
	InstallFlatPatch(0xA00B, regBase); /* _Rename */
	InstallFlatPatch(0xA00C, regBase); /* _GetFileInfo */
	InstallFlatPatch(0xA00D, regBase); /* _SetFileInfo */
	InstallFlatPatch(0xA00E, regBase); /* _UnmountVol */
	InstallFlatPatch(0xA010, regBase); /* _Allocate */
	InstallFlatPatch(0xA011, regBase); /* _GetEOF */
	InstallFlatPatch(0xA012, regBase); /* _SetEOF */
	InstallFlatPatch(0xA013, regBase); /* _FlushVol */
	InstallFlatPatch(0xA014, regBase); /* _GetVol */
	InstallFlatPatch(0xA015, regBase); /* _SetVol */
	InstallFlatPatch(0xA017, regBase); /* _Eject */
	InstallFlatPatch(0xA018, regBase); /* _GetFPos */
	InstallFlatPatch(0xA044, regBase); /* _SetFPos */
	InstallFlatPatch(0xA045, regBase); /* _FlushFile */
	dbg_log(regBase, "maxivmac INIT: traps patched");

	/* ---- Drain pending mount queue ---- */
	reg_command(regBase, kExtFSPollMount);
	slot = reg_get(regBase, 0);
	while (slot != 0xFFFFFFFFUL)
	{
		short s = (short)slot;
		short vRefNum = (short)reg_get(regBase, 1);
		short driveNum = (short)reg_get(regBase, 2);
		MountNewDrive(g, s, vRefNum, driveNum);
		reg_command(regBase, kExtFSPollMount);
		slot = reg_get(regBase, 0);
	}

	if (g->driveCount > 0)
		dbg_log1(regBase, "maxivmac INIT: %ld drives mounted",
				 (long)g->driveCount);

	/* ---- Install jGNEFilter ---- */
	g->oldFilter = *(long *)kJGNEFilter;
	g->lastPollTick = 0;
	g->lastClipTicks = 0;
	*(long *)kJGNEFilter = (long)FilterEntry;
	dbg_log(regBase, "maxivmac INIT: jGNEFilter installed");

	dbg_log(regBase, "maxivmac INIT: done!");

bail:
	RestoreA4();
}
