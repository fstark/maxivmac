/*
	maxivmac INIT — init.c
	INIT entry point, unified jGNEFilter, trap stub generation,
	and boot-time initialisation.

	Combines the SharedDrive INIT (ID 315) and ClipSync INIT (ID 314)
	into a single INIT resource (ID 128).
*/

#include "defs.h"

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

			/* Mount polling + guest commands (throttled to 60 ticks) */
			if (TickCount() - g->lastPollTick >= 60)
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

void main(void)
{
	char *regBase;
	Globals *g;
	Handle self;
	Ptr myINITPtr;
	short driveAvail, clipAvail;

	asm { move.l a0, myINITPtr }
	RememberA0();
	SetUpA4();

	regBase = find_reg_base();
	if (regBase == NULL) goto bail;

	dbg_log1(regBase, "maxivmac INIT: regBase=%lx", (unsigned long)regBase);

	/* Check drive protocol version — non-zero means host has shared dir */
	reg_command(regBase, 0x0200);
	driveAvail = (reg_get(regBase, 0) != 0);

	/* Check clipboard protocol version — need >= 2 for KV commands */
	reg_command(regBase, kClipVersion);
	clipAvail = ((long)reg_get(regBase, 0) >= 2);

	if (!driveAvail && !clipAvail)
	{
		dbg_log(regBase, "maxivmac INIT: nothing available, bailing");
		goto bail;
	}

	dbg_log2(regBase, "maxivmac INIT: drive=%ld clip=%ld", (long)driveAvail, (long)clipAvail);

	/* Get volume stats (for VCB fill, if drive available) */
	if (driveAvail)
		reg_command(regBase, kCmdGetVol);

	/* Allocate globals */
	g = (Globals *)NewPtrSysClear(sizeof(Globals));
	if (g == NULL)
	{
		dbg_log(regBase, "maxivmac INIT: NewPtrSys failed");
		goto bail;
	}
	g->regBase = regBase;
	g->driveCount = 0;
	if (driveAvail)
	{
		g->volFileCount = reg_get(regBase, 0);
		g->volTotalBytes = reg_get(regBase, 1);
	}

	/* Save A4 for code resource data access from stubs */
	{
		long *a4dst = &g->savedA4;
		asm {
			move.l a4dst, a0
			move.l a4, (a0)
		}
	}

	set_globals(regBase, g);

	/* Keep our code resource in memory */
	self = GetResource('INIT', 128);
	if (self != NULL)
	{
		DetachResource(self);
		HLock(self);
		HNoPurge(self);
	}

	/* Mount shared drives if available */
	if (driveAvail)
	{
		unsigned long slot;

		/* Drain the pending-mount queue */
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
		{
			dbg_log2(regBase, "maxivmac INIT: %ld files, %ld bytes",
					 g->volFileCount, g->volTotalBytes);

			/* Build dispatch tables */
			InitTrapTables();

			/* Install trap patches */
			InstallHFSPatch(regBase);

			/* Flat-file traps */
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
		}
	}

	/* Install jGNEFilter (always — needed for clip even without drives) */
	g->oldFilter = *(long *)kJGNEFilter;
	g->lastPollTick = 0;
	g->lastClipTicks = 0;
	*(long *)kJGNEFilter = (long)FilterEntry;
	dbg_log(regBase, "maxivmac INIT: jGNEFilter installed");

	dbg_log(regBase, "maxivmac INIT: done!");

bail:
	RestoreA4();
}
