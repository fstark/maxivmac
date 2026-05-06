/*
	maxivmac INIT — drive.c
	SharedDrive trap handlers, FCB management, VCB/DQE allocation,
	dispatch tables, and MountNewDrive.
*/

#include "defs.h"

/* ---- FCB management ---- */

static short AllocFCB(Ptr vcb, unsigned long cnid, unsigned long eof, unsigned char flags)
{
	Ptr fcbBuf = *(Ptr *)kFCBSPtr;
	short fcbLen, i;
	Ptr fcb;
	if (fcbBuf == NULL) return 0;
	fcbLen = *(short *)fcbBuf;
	for (i = 2; i < fcbLen; i += kFCBLen)
	{
		fcb = fcbBuf + i;
		if (*(long *)(fcb + kFCBFlNum) == 0)
		{
			*(long *)(fcb + kFCBFlNum) = cnid;
			*(char *)(fcb + kFCBFlags) = flags;
			*(char *)(fcb + kFCBTypByt) = 0;
			*(long *)(fcb + kFCBEOF) = eof;
			*(long *)(fcb + kFCBPLen) = eof;
			*(long *)(fcb + kFCBCrPs) = 0;
			*(long *)(fcb + kFCBVPtr) = (long)vcb;
			return i;
		}
	}
	return 0;
}

static void FreeFCB(short refNum)
{
	Ptr fcbBuf = *(Ptr *)kFCBSPtr;
	if (fcbBuf != NULL) *(long *)(fcbBuf + refNum + kFCBFlNum) = 0;
}

static Ptr GetFCB(short refNum)
{
	Ptr fcbBuf = *(Ptr *)kFCBSPtr;
	if (fcbBuf == NULL) return NULL;
	return fcbBuf + refNum;
}

static Boolean IsOurVCB(Ptr vcb, Globals *g)
{
	short i;
	for (i = 0; i < g->driveCount; i++)
		if (g->vcb[i] == vcb) return true;
	return false;
}

static Boolean IsOurFCB(short refNum)
{
	Globals *g = get_globals();
	Ptr fcb;
	Ptr fcbVCB;
	if (g == NULL) return false;
	fcb = GetFCB(refNum);
	if (fcb == NULL)
	{
		dbg_log1(g->regBase, "IsOurFCB(%ld): fcb==NULL", (long)refNum);
		return false;
	}
	fcbVCB = *(Ptr *)(fcb + kFCBVPtr);
	if (!IsOurVCB(fcbVCB, g))
	{
		dbg_log2(g->regBase, "IsOurFCB(%ld): VCBPtr=%lx NOT ours", (long)refNum, (long)fcbVCB);
		dbg_log2(g->regBase, "  g->vcb[0]=%lx driveCount=%ld", (long)g->vcb[0],
				 (long)g->driveCount);
		return false;
	}
	return true;
}

/* Resolve a vRefNum to one of our VCB pointers.
   Handles raw vRefNums (-32000..-32007), drive numbers (8..15),
   and vRefNum 0 (default volume via DefVCBPtr). Returns NULL if
   the vRefNum doesn't match any of our volumes. */
static Ptr FindVCB(short vRefNum, Globals *g)
{
	short i;
	/* vRefNum 0 -> DefVCBPtr */
	if (vRefNum == 0)
	{
		Ptr def = *(Ptr *)kDefVCBPtr;
		for (i = 0; i < g->driveCount; i++)
			if (g->vcb[i] == def) return def;
		return NULL;
	}
	/* Direct match against each slot's vRefNum and driveNum */
	for (i = 0; i < g->driveCount; i++)
	{
		if (g->vcb[i] != NULL)
		{
			short vr = *(short *)(g->vcb[i] + 78);
			short dn = *(short *)(g->vcb[i] + 72);
			if (vRefNum == vr || vRefNum == dn) return g->vcb[i];
		}
	}
	return NULL;
}

/* ================================================================ */
/*                 Trap handlers                                    */
/* ================================================================ */

static OSErr TrapClose(char *pb, Globals *g, short isHFS)
{
	short refNum = *(short *)(pb + pb_ioRefNum);
	Ptr fcb = GetFCB(refNum);
	if (fcb == NULL) return kRfNumErr;
	reg_set(g->regBase, 0, *(unsigned long *)(fcb + kFCBHostHandle));
	reg_command(g->regBase, kCmdClose);
	FreeFCB(refNum);
	return kNoErr;
}

static OSErr TrapRead(char *pb, Globals *g, short isHFS)
{
	short refNum = *(short *)(pb + pb_ioRefNum);
	unsigned long buffer = *(unsigned long *)(pb + pb_ioBuffer);
	long reqCount = *(long *)(pb + pb_ioReqCount);
	short posMode = *(short *)(pb + pb_ioPosMode);
	long posOffset = *(long *)(pb + pb_ioPosOffset);
	Ptr fcb;
	long mark, eof, handle;
	unsigned long actual;

	fcb = GetFCB(refNum);
	if (fcb == NULL) return kRfNumErr;

	mark = *(long *)(fcb + kFCBCrPs);
	eof = *(long *)(fcb + kFCBEOF);
	handle = *(long *)(fcb + kFCBHostHandle);

	switch (posMode & 0x03)
	{
		case 1:
			mark = posOffset;
			break;
		case 2:
			mark = eof + posOffset;
			break;
		case 3:
			mark += posOffset;
			break;
	}
	if (mark < 0)
	{
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = 0;
		*(long *)(fcb + kFCBCrPs) = 0;
		return kPosErr;
	}
	if (mark > eof) mark = eof;

	if (reqCount < 0)
	{
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		*(long *)(fcb + kFCBCrPs) = mark;
		return kParamErr;
	}
	if (reqCount == 0)
	{
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		*(long *)(fcb + kFCBCrPs) = mark;
		return kNoErr;
	}
	{
		short hitEof = 0;
		if (mark + reqCount > eof)
		{
			reqCount = eof - mark;
			hitEof = 1;
		}

		reg_set(g->regBase, 0, handle);
		reg_set(g->regBase, 1, (unsigned long)mark);
		reg_set(g->regBase, 2, (unsigned long)reqCount);
		reg_set(g->regBase, 3, buffer);
		reg_command(g->regBase, kCmdRead);
		actual = reg_get(g->regBase, 0);

		mark += actual;
		*(long *)(fcb + kFCBCrPs) = mark;
		*(long *)(pb + pb_ioActCount) = actual;
		*(long *)(pb + pb_ioPosOffset) = mark;
		if (actual < (unsigned long)reqCount) return kEofErr;
		if (hitEof) return kEofErr;
		return kNoErr;
	}
}

static OSErr TrapWrite(char *pb, Globals *g, short isHFS)
{
	short refNum = *(short *)(pb + pb_ioRefNum);
	unsigned long buffer = *(unsigned long *)(pb + pb_ioBuffer);
	long reqCount = *(long *)(pb + pb_ioReqCount);
	short posMode = *(short *)(pb + pb_ioPosMode);
	long posOffset = *(long *)(pb + pb_ioPosOffset);
	Ptr fcb;
	long mark, eof, handle;
	unsigned long actual;

	fcb = GetFCB(refNum);
	if (fcb == NULL) return kRfNumErr;

	mark = *(long *)(fcb + kFCBCrPs);
	eof = *(long *)(fcb + kFCBEOF);
	handle = *(long *)(fcb + kFCBHostHandle);

	switch (posMode & 0x03)
	{
		case 1:
			mark = posOffset;
			break;
		case 2:
			mark = eof + posOffset;
			break;
		case 3:
			mark += posOffset;
			break;
	}
	if (mark < 0)
	{
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = 0;
		return kPosErr;
	}

	if (reqCount < 0)
	{
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		return kParamErr;
	}
	if (reqCount == 0)
	{
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		return kNoErr;
	}

	reg_set(g->regBase, 0, handle);
	reg_set(g->regBase, 1, (unsigned long)mark);
	reg_set(g->regBase, 2, (unsigned long)reqCount);
	reg_set(g->regBase, 3, buffer);
	reg_command(g->regBase, kCmdWrite);
	actual = reg_get(g->regBase, 0);

	mark += actual;
	if (mark > eof)
	{
		eof = mark;
		*(long *)(fcb + kFCBEOF) = eof;
		*(long *)(fcb + kFCBPLen) = eof;
	}
	*(long *)(fcb + kFCBCrPs) = mark;
	*(long *)(pb + pb_ioActCount) = actual;
	*(long *)(pb + pb_ioPosOffset) = mark;
	return (actual < (unsigned long)reqCount) ? kIoErr : kNoErr;
}

static OSErr TrapGetEOF(char *pb, Globals *g, short isHFS)
{
	Ptr fcb = GetFCB(*(short *)(pb + pb_ioRefNum));
	if (fcb == NULL) return kRfNumErr;
	*(long *)(pb + pb_ioMisc) = *(long *)(fcb + kFCBEOF);
	return kNoErr;
}

static OSErr TrapSetEOF(char *pb, Globals *g, short isHFS)
{
	short refNum = *(short *)(pb + pb_ioRefNum);
	long newEOF = *(long *)(pb + pb_ioMisc);
	Ptr fcb = GetFCB(refNum);
	if (fcb == NULL) return kRfNumErr;

	reg_set(g->regBase, 0, *(unsigned long *)(fcb + kFCBHostHandle));
	reg_set(g->regBase, 1, (unsigned long)newEOF);
	reg_command(g->regBase, kCmdSetEOF);

	*(long *)(fcb + kFCBEOF) = newEOF;
	*(long *)(fcb + kFCBPLen) = newEOF;
	return kNoErr;
}

static OSErr TrapGetFPos(char *pb, Globals *g, short isHFS)
{
	Ptr fcb = GetFCB(*(short *)(pb + pb_ioRefNum));
	if (fcb == NULL) return kRfNumErr;
	*(long *)(pb + pb_ioPosOffset) = *(long *)(fcb + kFCBCrPs);
	*(long *)(pb + pb_ioReqCount) = 0;
	*(long *)(pb + pb_ioActCount) = 0;
	return kNoErr;
}

static OSErr TrapSetFPos(char *pb, Globals *g, short isHFS)
{
	short posMode = *(short *)(pb + pb_ioPosMode);
	long posOffset = *(long *)(pb + pb_ioPosOffset);
	Ptr fcb = GetFCB(*(short *)(pb + pb_ioRefNum));
	long mark, eof;
	if (fcb == NULL) return kRfNumErr;

	mark = *(long *)(fcb + kFCBCrPs);
	eof = *(long *)(fcb + kFCBEOF);
	switch (posMode & 0x03)
	{
		case 1:
			mark = posOffset;
			break;
		case 2:
			mark = eof + posOffset;
			break;
		case 3:
			mark += posOffset;
			break;
	}
	if (mark < 0)
	{
		*(long *)(fcb + kFCBCrPs) = 0;
		*(long *)(pb + pb_ioPosOffset) = 0;
		return kPosErr;
	}
	if (mark > eof)
	{
		*(long *)(fcb + kFCBCrPs) = eof;
		*(long *)(pb + pb_ioPosOffset) = eof;
		return kEofErr;
	}
	*(long *)(fcb + kFCBCrPs) = mark;
	*(long *)(pb + pb_ioPosOffset) = mark;
	return kNoErr;
}

static OSErr TrapFlushFile(char *pb, Globals *g, short isHFS)
{
	return kNoErr;
}

static OSErr TrapAllocate(char *pb, Globals *g, short isHFS)
{
	return kNoErr;
}

static OSErr TrapOpen(char *pb, Globals *g, short isHFS)
{
	unsigned char perm = *(unsigned char *)(pb + pb_ioPermssn);

	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_Open);
	if ((unsigned short)reg_result(g->regBase) == kNotOurs) return kPassThrough;
	if (reg_result(g->regBase) != 0) return host_err(g->regBase);

	{
		unsigned long handle = reg_get(g->regBase, 0);
		long size = (long)reg_get(g->regBase, 1);
		unsigned long cnid = reg_get(g->regBase, 2);
		short slot = (short)(handle >> 28);
		Ptr vcb = g->vcb[slot];
		unsigned char flags;

		/* Map permission to FCB flags */
		if (perm == kFsRdPerm)
			flags = 0x00; /* read only */
		else
			flags = 0x01; /* fcbWriteMask */

		{
			short refNum = AllocFCB(vcb, cnid, size, flags);
			if (refNum == 0)
			{
				reg_set(g->regBase, 0, handle);
				reg_command(g->regBase, kCmdClose);
				return kTmfoErr;
			}
			{
				Ptr fcb = GetFCB(refNum);
				unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
				*(long *)(fcb + kFCBHostHandle) = handle;
				*(long *)(fcb + kFCBDirID) = (long)reg_get(g->regBase, 3);
				pstr_copy_max(fcb + kFCBCName, (char *)nameAddr, 31);
			}
			*(short *)(pb + pb_ioRefNum) = refNum;
		}
		/* Keep vcbNxtCNID current */
		{
			long nextCNID = *(long *)(vcb + kVcbNxtCNID);
			if ((long)cnid >= nextCNID) *(long *)(vcb + kVcbNxtCNID) = (long)(cnid + 1);
		}
	}
	return kNoErr;
}

static OSErr TrapOpenRF(char *pb, Globals *g, short isHFS)
{
	unsigned char perm = *(unsigned char *)(pb + pb_ioPermssn);

	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_OpenRF);
	if ((unsigned short)reg_result(g->regBase) == kNotOurs) return kPassThrough;
	if (reg_result(g->regBase) != 0) return host_err(g->regBase);

	{
		unsigned long handle = reg_get(g->regBase, 0);
		long size = (long)reg_get(g->regBase, 1);
		unsigned long cnid = reg_get(g->regBase, 2);
		short slot = (short)(handle >> 28);
		Ptr vcb = g->vcb[slot];
		unsigned char flags;

		/* Map permission to FCB flags (resource fork bit = 0x02) */
		if (perm == kFsRdPerm)
			flags = 0x02; /* resource fork, read only */
		else
			flags = 0x03; /* resource fork + write */

		{
			short refNum = AllocFCB(vcb, cnid, size, flags);
			if (refNum == 0)
			{
				reg_set(g->regBase, 0, handle);
				reg_command(g->regBase, kCmdClose);
				return kTmfoErr;
			}
			{
				Ptr fcb = GetFCB(refNum);
				unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
				*(long *)(fcb + kFCBHostHandle) = handle;
				*(long *)(fcb + kFCBDirID) = (long)reg_get(g->regBase, 3);
				pstr_copy_max(fcb + kFCBCName, (char *)nameAddr, 31);
			}
			*(short *)(pb + pb_ioRefNum) = refNum;
		}
		/* Keep vcbNxtCNID current */
		{
			long nextCNID = *(long *)(vcb + kVcbNxtCNID);
			if ((long)cnid >= nextCNID) *(long *)(vcb + kVcbNxtCNID) = (long)(cnid + 1);
		}
	}
	return kNoErr;
}

static OSErr TrapGetFileInfo(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_GetFileInfo);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapSetFileInfo(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_SetFileInfo);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapGetCatInfo(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_GetCatInfo);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapSetCatInfo(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_SetCatInfo);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapCreate(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_Create);
	if ((unsigned short)reg_result(g->regBase) == kNotOurs) return kPassThrough;
	{
		OSErr err = host_err(g->regBase);
		if (err == kNoErr)
		{
			Ptr vcb = FindVCB(*(short *)(pb + pb_ioVRefNum), g);
			if (vcb != NULL)
			{
				long nextCNID = *(long *)(vcb + kVcbNxtCNID);
				*(long *)(vcb + kVcbNxtCNID) = nextCNID + 1;
			}
		}
		return err;
	}
}

static OSErr TrapDelete(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_Delete);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapRename(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_Rename);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapGetVolInfo(char *pb, Globals *g, short isHFS)
{
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	Ptr v;
	unsigned long fileCount, dirCount, totalBytes;
	short vidx = *(short *)(pb + pb_ioVolIndex);
	short driveNum;

	/* Indexed VCB walk: only handle if Nth VCB is ours */
	if (vidx > 0)
	{
		Ptr vcb = *(Ptr *)(kVCBQHdr + 2); /* qHead */
		short i;
		for (i = 1; i < vidx && vcb != NULL; i++)
			vcb = *(Ptr *)vcb;
		if (vcb == NULL || !IsOurVCB(vcb, g)) return 1; /* not ours — pass through */
		v = vcb;
	}
	else if (vidx == 0)
	{
		short vRefNum = *(short *)(pb + pb_ioVRefNum);
		/* Check direct vRefNum/driveNum match */
		v = FindVCB(vRefNum, g);
		if (v == NULL) return 1; /* not ours */
	}
	else
	{
		/* vidx < 0: return info about the default volume */
		v = FindVCB(0, g);
		if (v == NULL) return 1; /* not ours */
	}
	driveNum = *(short *)(v + 72);

	/* Query host for fresh counts */
	reg_command(g->regBase, kCmdGetVol);
	fileCount = reg_get(g->regBase, 0);
	totalBytes = reg_get(g->regBase, 1);
	dirCount = reg_get(g->regBase, 2);

	/* Copy volume name from VCB */
	if (nameAddr != 0)
	{
		unsigned char *src = (unsigned char *)(v + 44);
		unsigned char *dst = (unsigned char *)nameAddr;
		short len = src[0];
		short j;
		if (len > 27) len = 27;
		dst[0] = len;
		for (j = 1; j <= len; j++)
			dst[j] = src[j];
	}

	/* Return the volume's raw vRefNum from its VCB.  Real HFS preserves
	   the caller's WD refnum in ioVRefNum but for indexed calls (vidx>0)
	   returns the raw volume ref.  Using the VCB value is correct and
	   doesn't depend on guest-side WD tracking. */
	*(short *)(pb + pb_ioVRefNum) = *(short *)(v + 78);
	*(long *)(pb + 30) = *(long *)(v + 10); /* ioVCrDate */
	*(long *)(pb + 34) = *(long *)(v + 14); /* ioVLsMod */
	*(short *)(pb + 38) = 0;				/* ioVAtrb */
	*(short *)(pb + 40) = (short)fileCount; /* ioVNmFls */
	*(short *)(pb + 42) = 0;				/* ioVBitMap */
	*(short *)(pb + 44) = 0;				/* ioVAllocPtr */
	*(short *)(pb + pb_ioVNmAlBlks) = kTotalAllocBlks;
	*(long *)(pb + pb_ioVAlBlkSiz) = kAllocBlkSize;
	*(long *)(pb + pb_ioVClpSiz) = kAllocBlkSize;
	*(short *)(pb + 56) = 0;			 /* ioAlBlSt */
	*(long *)(pb + 58) = fileCount + 16; /* ioVNxtCNID */
	{
		long usedBlks = (long)((totalBytes + kAllocBlkSize - 1) / kAllocBlkSize);
		long freeBlks = kTotalAllocBlks - usedBlks;
		if (freeBlks < 0) freeBlks = 0;
		*(short *)(pb + pb_ioVFrBlk) = (short)freeBlks;
	}

	if (isHFS)
	{
		*(short *)(pb + 64) = 0x4244;		  /* ioVSigWord */
		*(short *)(pb + 66) = driveNum;		  /* ioVDrvInfo */
		*(short *)(pb + 68) = kOurDrvrRefNum; /* ioVDRefNum */
		*(short *)(pb + 70) = 0x5344;		  /* ioVFSID */
		*(long *)(pb + 72) = 0;				  /* ioVBkUp */
		*(short *)(pb + 76) = 0;			  /* ioVSeqNum */
		*(long *)(pb + 78) = 0;				  /* ioVWrCnt */
		*(long *)(pb + 82) = fileCount;		  /* ioVFilCnt */
		*(long *)(pb + 86) = dirCount;		  /* ioVDirCnt */
		mem_zero(pb + 90, 32);				  /* ioVFndrInfo */
	}
	return kNoErr;
}

static OSErr TrapGetVol(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_GetVol);

	if ((unsigned short)reg_result(g->regBase) == kNotOurs) return 1;
	return host_err(g->regBase);
}

static OSErr TrapSetVol(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_SetVol);

	if ((unsigned short)reg_result(g->regBase) == kNotOurs) return 1;
	if (reg_result(g->regBase) != 0) return (short)reg_result(g->regBase);

	{
		short slot = (short)reg_get(g->regBase, 0);
		if (slot >= 0 && slot < kMaxDrives && g->vcb[slot] != NULL)
			*(Ptr *)kDefVCBPtr = g->vcb[slot];
	}
	return kNoErr;
}

static OSErr TrapUnmountVol(char *pb, Globals *g, short isHFS)
{
	short i;
	for (i = 0; i < g->driveCount; i++)
	{
		if (g->dqe[i] != NULL)
		{
			Dequeue((QElemPtr)(g->dqe[i] + 4), (QHdrPtr)kDrvQHdr);
			g->dqe[i] = NULL;
		}
		if (g->vcb[i] != NULL) Dequeue((QElemPtr)g->vcb[i], (QHdrPtr)kVCBQHdr);
	}
	g->ejected = 1;
	return kNoErr;
}

static OSErr TrapEject(char *pb, Globals *g, short isHFS)
{
	short i;
	for (i = 0; i < g->driveCount; i++)
	{
		if (g->dqe[i] != NULL)
		{
			Dequeue((QElemPtr)(g->dqe[i] + 4), (QHdrPtr)kDrvQHdr);
			g->dqe[i] = NULL;
		}
		if (g->vcb[i] != NULL) Dequeue((QElemPtr)g->vcb[i], (QHdrPtr)kVCBQHdr);
	}
	g->ejected = 1;
	return kNoErr;
}

static OSErr TrapFlushVol(char *pb, Globals *g, short isHFS)
{
	return kNoErr;
}

static OSErr TrapOpenWD(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_command(g->regBase, kPB_OpenWD);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapCloseWD(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_command(g->regBase, kPB_CloseWD);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapGetWDInfo(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_command(g->regBase, kPB_GetWDInfo);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapGetVolParms(char *pb, Globals *g, short isHFS)
{
	unsigned long bufAddr = *(unsigned long *)(pb + pb_ioBuffer);
	long reqCount = *(long *)(pb + pb_ioReqCount);
	long actual;

	if (bufAddr == 0) return kParamErr;

	actual = 14;
	if (reqCount < actual) actual = reqCount;

	mem_zero((char *)bufAddr, (short)actual);

	/* vMVersion = 1 */
	if (actual >= 2) *(short *)(bufAddr + 0) = 1;

	/* vMAttrib = 0 (no special capabilities) */
	if (actual >= 6) *(long *)(bufAddr + 2) = 0;

	*(long *)(pb + pb_ioActCount) = actual;
	return kNoErr;
}

static OSErr TrapGetFCBInfo(char *pb, Globals *g, short isHFS)
{
	short refNum = *(short *)(pb + pb_ioRefNum);
	short fcbIdx = *(short *)(pb + pb_ioFCBIndx);
	unsigned long nmAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	Ptr fcb;

	if (fcbIdx != 0) return kParamErr;

	fcb = GetFCB(refNum);
	if (fcb == NULL || *(long *)(fcb + kFCBFlNum) == 0) return -51; /* rfNumErr */

	if (nmAddr != 0) pstr_copy_max((char *)nmAddr, fcb + kFCBCName, 31);

	*(short *)(pb + pb_ioRefNum) = refNum;
	*(long *)(pb + pb_ioFCBFlNm) = *(long *)(fcb + kFCBFlNum);
	*(short *)(pb + pb_ioFCBFlags) =
		(short)((unsigned short)(*(unsigned char *)(fcb + kFCBFlags)) << 8 |
				(unsigned char)(*(unsigned char *)(fcb + kFCBTypByt)));
	*(short *)(pb + pb_ioFCBStBlk) = 0;
	*(long *)(pb + pb_ioFCBEOF) = *(long *)(fcb + kFCBEOF);
	*(long *)(pb + pb_ioFCBPLen) = *(long *)(fcb + kFCBPLen);
	*(long *)(pb + pb_ioFCBCrPs) = *(long *)(fcb + kFCBCrPs);
	/* Derive vRefNum from the FCB's VCB pointer */
	{
		Ptr fcbVCB = *(Ptr *)(fcb + kFCBVPtr);
		short vr = kOurVRefNum; /* default */
		short i;
		for (i = 0; i < g->driveCount; i++)
		{
			if (g->vcb[i] == fcbVCB)
			{
				vr = -(kBaseVRefNum + i);
				break;
			}
		}
		*(short *)(pb + pb_ioFCBVRefNum) = vr;
	}
	*(long *)(pb + pb_ioFCBClpSiz) = 0;
	*(long *)(pb + pb_ioFCBParID) = *(long *)(fcb + kFCBDirID);
	return kNoErr;
}

static OSErr TrapDirCreate(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_DirCreate);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapCatMove(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_set(g->regBase, 1, (unsigned long)isHFS);
	reg_command(g->regBase, kPB_CatMove);
	return host_err_passthrough(g->regBase);
}

static OSErr TrapSetVInfo(char *pb, Globals *g, short isHFS)
{
	return kNoErr;
}

/* ================================================================ */
/*              Central dispatchers (called from stubs)             */
/* ================================================================ */

typedef OSErr (*TrapHandler)(char *pb, Globals *g, short isHFS);

typedef struct
{
	short trapNum;
	TrapHandler handler;
	short refBased; /* 1=check IsOurFCB, 0=host-first dispatch */
} TrapEntry;

#define kMaxFlatTraps 23
#define kMaxHFSTraps 11

static TrapEntry sFlatTraps[kMaxFlatTraps];
static TrapEntry sHFSTraps[kMaxHFSTraps];

static void AddEntry(TrapEntry *table, short *idx, short maxN, short trapNum, TrapHandler handler,
					 short refBased)
{
	if (*idx < maxN - 1)
	{
		table[*idx].trapNum = trapNum;
		table[*idx].handler = handler;
		table[*idx].refBased = refBased;
		(*idx)++;
		/* Keep sentinel zeroed (arrays are already cleared) */
	}
}

void InitTrapTables(void)
{
	short fi = 0, hi = 0;

	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x00, TrapOpen, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x01, TrapClose, 1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x02, TrapRead, 1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x03, TrapWrite, 1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x07, TrapGetVolInfo, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x08, TrapCreate, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x09, TrapDelete, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0A, TrapOpenRF, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0B, TrapRename, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0C, TrapGetFileInfo, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0D, TrapSetFileInfo, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0E, TrapUnmountVol, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x10, TrapAllocate, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x11, TrapGetEOF, 1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x12, TrapSetEOF, 1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x13, TrapFlushVol, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x14, TrapGetVol, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x15, TrapSetVol, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x17, TrapEject, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x18, TrapGetFPos, 1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x44, TrapSetFPos, 1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x45, TrapFlushFile, 1);

	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0001, TrapOpenWD, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0002, TrapCloseWD, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0005, TrapCatMove, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0006, TrapDirCreate, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0007, TrapGetWDInfo, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0008, TrapGetFCBInfo, 1);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0009, TrapGetCatInfo, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x000A, TrapSetCatInfo, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x000B, TrapSetVInfo, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0030, TrapGetVolParms, 0);
}

static short DispatchFromTable(TrapEntry *table, short key, char *pb, Globals *g, short isHFS,
							   short trapWord)
{
	TrapEntry *e;
	OSErr err;

	for (e = table; e->handler != NULL; e++)
	{
		if (e->trapNum != key) continue;

		/* Ownership check */
		if (e->refBased)
		{
			if (!IsOurFCB(*(short *)(pb + pb_ioRefNum)))
			{
				log_trap(g->regBase, trapWord, pb, LOG_PASSTHRU, 0, isHFS ? LOG_F_HFS : 0);
				return 1;
			}
		}
		/* Non-refBased traps handle ownership internally:
		   host-first handlers return kNotOurs -> kPassThrough,
		   VCB-based handlers (GetVolInfo) check VCB pointers. */

		err = e->handler(pb, g, isHFS);
		if (err == kPassThrough)
		{
			log_trap(g->regBase, trapWord, pb, LOG_PASSTHRU, 0, isHFS ? LOG_F_HFS : 0);
			return 1;
		}
		*(short *)(pb + pb_ioResult) = err;
		log_trap(g->regBase, trapWord, pb, err ? LOG_ERROR : LOG_HANDLED, err,
				 (isHFS ? LOG_F_HFS : 0) | LOG_F_PBMOD);
		return 0;
	}
	return 1; /* not in table — pass through */
}

short DispatchFlat(char *pb, short trapWord)
{
	short trapNum = trapWord & 0xFF;
	short isHFS = (trapWord & 0x0200) != 0;
	Globals *g;
	short result;

	SetUpA4();
	g = get_globals();
	if (g == NULL || g->ejected)
	{
		RestoreA4();
		return 1;
	}

	result = DispatchFromTable(sFlatTraps, trapNum, pb, g, isHFS, trapWord);
	RestoreA4();
	return result;
}

short DispatchHFS(char *pb, short selector)
{
	Globals *g;
	short result;

	SetUpA4();
	g = get_globals();
	if (g == NULL || g->ejected)
	{
		RestoreA4();
		return 1;
	}

	result = DispatchFromTable(sHFSTraps, selector, pb, g, 1, selector);
	RestoreA4();
	return result;
}

/* ================================================================ */
/*                Runtime mount helper                              */
/* ================================================================ */

/*
	Allocate a VCB and DQE for a newly-mounted drive discovered
	via kExtFSPollMount.  Called both from the jGNEFilter and from
	the INIT boot loop.
*/
void MountNewDrive(Globals *g, short slot, short vRefNum, short driveNum)
{
	Ptr v;
	unsigned long now;
	unsigned char nameBuf[32];

	if (slot < 0 || slot >= kMaxDrives) return;

	/* Guard against double-mount: if this slot already has a VCB, skip */
	if (g->vcb[slot] != NULL) return;

	/* Get volume name from host */
	reg_set(g->regBase, 0, (unsigned long)slot);
	reg_set(g->regBase, 1, (unsigned long)nameBuf);
	reg_command(g->regBase, kExtFSGetVolName);

	v = NewPtrSysClear(178);
	if (v == NULL) return;
	g->vcb[slot] = v;

	GetDateTime(&now);
	*(short *)(v + 4) = 1;		/* qType = fsQType */
	*(short *)(v + 8) = 0x4244; /* vcbSigWord = HFS */
	*(long *)(v + 10) = now;	/* vcbCrDate */
	*(long *)(v + 14) = now;	/* vcbLsMod */
	*(short *)(v + 18) = 0;		/* vcbAtrb: writable */
	*(short *)(v + 26) = kTotalAllocBlks;
	*(long *)(v + 28) = kAllocBlkSize;
	*(long *)(v + 32) = kAllocBlkSize;
	*(long *)(v + 38) = 16;				  /* vcbNxtCNID */
	*(short *)(v + 42) = kTotalAllocBlks; /* vcbFreeBks: all free for new */
	/* Copy volume name from host (Pascal string in nameBuf) */
	{
		short len = (unsigned char)nameBuf[0];
		short j;
		if (len > 27) len = 27;
		v[44] = len;
		for (j = 1; j <= len; j++)
			v[44 + j] = nameBuf[j];
	}
	*(short *)(v + 72) = driveNum;
	*(short *)(v + 74) = kOurDrvrRefNum;
	*(short *)(v + 76) = 0x5344; /* vcbFSID = 'SD' */
	*(short *)(v + 78) = vRefNum;

	Enqueue((QElemPtr)v, (QHdrPtr)kVCBQHdr);

	/* Allocate DQE */
	{
		Ptr dqe = NewPtrSysClear(20);
		if (dqe != NULL)
		{
			*(long *)dqe = 0x00080000L; /* flags: non-ejectable */
			*(short *)(dqe + 8) = 1;	/* qType */
			*(short *)(dqe + 14) = 0;	/* dQFSID */
			{
				long sectors = (long)kTotalAllocBlks * (kAllocBlkSize / 512);
				*(short *)(dqe + 16) = (short)(sectors & 0xFFFF);
				*(short *)(dqe + 18) = (short)((sectors >> 16) & 0xFFFF);
			}
			AddDrive(kOurDrvrRefNum, driveNum, (DrvQElPtr)(dqe + 4));
			g->dqe[slot] = dqe;
		}
	}

	if (slot >= g->driveCount) g->driveCount = slot + 1;

	dbg_log2(g->regBase, "SharedDrive: mounted slot %ld drv=%ld", (long)slot, (long)driveNum);
}
