/*
	SharedDrive INIT — init.c
	THINK C code resource project. Builds as INIT resource.

	Mounts the host's shared/ directory as a read-only HFS volume
	by patching File Manager traps and dispatching to the emulator
	via the register-block extension interface.

	Register interface at extnBlockBase + $20:
	  $200 ExtFSVersion   -> p0 = version (0 if no shared/ dir)
	  $201 ExtFSGetVol    -> p0 = file count, p1 = total bytes
	  $202 ExtFSGetCatInfo p0=dirID, p1=index, p2=nameBuf
	  $203 ExtFSGetCatInfoByName p0=parentDirID, p1=namePtr, p2=nameBuf
	  $204 ExtFSOpen      p0=CNID, p1=fork -> p0=handle
	  $205 ExtFSRead      p0=handle, p1=offset, p2=count, p3=bufAddr
	  $206 ExtFSClose     p0=handle
	  $207 ExtFSGetFileInfo p0=CNID -> p0=type, p1=creator, p2=crDate, p3=modDate
	  $208 ExtFSReadDir   p0=dirID -> p0=count
	  $209 ExtFSObjByName p0=parentDirID, p1=namePtr -> p0=CNID
	  $20A ExtFSGetWDInfo p0=wdRefNum -> p0=vRefNum, p1=dirID
	  $20B ExtFSOpenWD    p0=vRefNum, p1=dirID -> p0=wdRefNum
	  $20C ExtFSCloseWD   p0=wdRefNum
	  $20D ExtFSDbgLog    p0=fmt string addr, p1-p6=args

	Trap patching approach:
	  Each patched trap gets a small dynamically-generated 68k code
	  stub in the system heap. The stub saves registers, pushes A0
	  (param block) onto the stack, calls our C dispatcher via an
	  absolute JSR, and either returns (if handled) or jumps to the
	  original trap handler.

	  We patch _HFSDispatch and individual flat-file traps.
*/

#include <SetUpA4.h>
#include <Memory.h>
#include <OSUtils.h>
#include <Files.h>
#include <Devices.h>
#include <Events.h>

/* ---- constants ---- */

#define kOurVRefNum     (-32000)
#define kOurDriveNum    8
#define kOurDrvrRefNum  (-64)
#define kRootDirID      2

/* Low-memory globals */
#define kSonyVarsPtr    0x0134
#define kCheckVal       0x841339E2UL
#define kVCBQHdr        0x0356
#define kFCBSPtr        0x034E

/* FCB offsets */
#define kFCBLen         94
#define kFCBFlNum       0
#define kFCBFlags       4
#define kFCBTypByt      5
#define kFCBEOF         6
#define kFCBPLen        10
#define kFCBCrPs        14
#define kFCBVPtr        18

/* CInfoPBRec / HParamBlockRec field offsets from A0 */
#define pb_ioResult     16
#define pb_ioNamePtr    18
#define pb_ioVRefNum    22
#define pb_ioRefNum     24
#define pb_ioFDirIndex  28
#define pb_ioFlAttrib   30
#define pb_ioFlFndrInfo 32
#define pb_ioFlNum      36
#define pb_ioFlLgLen    40
#define pb_ioFlPyLen    44
#define pb_ioFlRLgLen   50
#define pb_ioFlRPyLen   54
#define pb_ioFlCrDat    58
#define pb_ioFlMdDat    62
#define pb_ioBuffer     32
#define pb_ioReqCount   36
#define pb_ioActCount   40
#define pb_ioPosMode    44
#define pb_ioPosOffset  46
#define pb_ioDirID      48
#define pb_ioDrNmFls    40
#define pb_ioDrParID    100
#define pb_ioDrCrDat    58
#define pb_ioDrMdDat    62

/* HFS selectors */
#define kGetCatInfo       0x0009
#define kSetCatInfo       0x000A
#define kOpenWD           0x0001
#define kCloseWD          0x0002
#define kGetWDInfo        0x0007
#define kCatMove          0x0005
#define kDirCreate        0x0006
#define kSetVInfo         0x000B
#define kCreateFileIDRef  0x0010
#define kDeleteFileIDRef  0x0011
#define kResolveFileIDRef 0x0012
#define kGetFCBInfo       0x0008

/* ---- Globals ---- */

typedef struct {
	char    *regBase;
	Ptr      vcb;
	long     volFileCount;
	long     volTotalBytes;
	long     savedA4;        /* THINK C code resource A4 */
} Globals;

#define kGlobalsPtr     0x0B04

/* ---- Extension discovery ---- */

typedef struct {
	unsigned long zeroes[4];
	unsigned long checkval;
	unsigned long pokeaddr;
} MyDriverDat_R;

static char *find_reg_base(void)
{
	MyDriverDat_R *sv;
	sv = *(MyDriverDat_R **)kSonyVarsPtr;
	if (sv == NULL) return NULL;
	if (sv->zeroes[0] != 0 || sv->zeroes[1] != 0
		|| sv->zeroes[2] != 0) return NULL;
	if (sv->checkval != kCheckVal) return NULL;
	if (sv->pokeaddr == 0) return NULL;
	return (char *)(sv->pokeaddr + 0x20);
}

/* ---- Register access ---- */

#define REG_COMMAND  0x00
#define REG_RESULT   0x02
#define REG_P(n)     (0x04 + (n) * 4)

static void reg_set(char *base, int n, unsigned long v)
{	*(unsigned long *)(base + REG_P(n)) = v; }

static unsigned long reg_get(char *base, int n)
{	return *(unsigned long *)(base + REG_P(n)); }

static void reg_command(char *base, unsigned short cmd)
{	*(unsigned short *)(base + REG_COMMAND) = cmd; }

static unsigned short reg_result(char *base)
{	return *(unsigned short *)(base + REG_RESULT); }

/* ---- Debug log ---- */

static void dbg_log6(char *base, char *fmt,
	unsigned long a,unsigned long b,unsigned long c,
	unsigned long d,unsigned long e,unsigned long f)
{
	reg_set(base,0,(unsigned long)fmt);
	reg_set(base,1,a); reg_set(base,2,b);
	reg_set(base,3,c); reg_set(base,4,d);
	reg_set(base,5,e); reg_set(base,6,f);
	reg_command(base, 0x020D);
}
#define dbg_log(b,s)            dbg_log6(b,s,0,0,0,0,0,0)
#define dbg_log1(b,s,a)         dbg_log6(b,s,(long)(a),0,0,0,0,0)
#define dbg_log2(b,s,a,c)       dbg_log6(b,s,(long)(a),(long)(c),0,0,0,0)

/* ---- Name buffer ---- */

static char s_nameBuf[64];

/* ---- FCB management ---- */

static short AllocFCB(Ptr vcb, unsigned long cnid,
	unsigned long eof, unsigned char forkType)
{
	Ptr fcbBuf = *(Ptr *)kFCBSPtr;
	short fcbLen, i;
	Ptr fcb;
	if (fcbBuf == NULL) return 0;
	fcbLen = *(short *)fcbBuf;
	for (i = 2; i < fcbLen; i += kFCBLen) {
		fcb = fcbBuf + i;
		if (*(long *)(fcb + kFCBFlNum) == 0) {
			*(long *)(fcb + kFCBFlNum) = cnid;
			*(char *)(fcb + kFCBFlags) = 0;
			*(char *)(fcb + kFCBTypByt) = forkType;
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
	if (fcbBuf != NULL)
		*(long *)(fcbBuf + refNum + kFCBFlNum) = 0;
}

static Ptr GetFCB(short refNum)
{
	Ptr fcbBuf = *(Ptr *)kFCBSPtr;
	if (fcbBuf == NULL) return NULL;
	return fcbBuf + refNum;
}

static Boolean IsOurFCB(short refNum)
{
	Globals *g = *(Globals **)kGlobalsPtr;
	Ptr fcb;
	if (g == NULL) return false;
	fcb = GetFCB(refNum);
	if (fcb == NULL) return false;
	return (*(long *)(fcb + kFCBVPtr) == (long)(g->vcb));
}

static Boolean IsOurVolume(short vRefNum)
{
	return (vRefNum == kOurVRefNum);
}

/* ================================================================ */
/*                    File Manager handlers                         */
/* ================================================================ */

static OSErr DoGetCatInfo(char *pb, char *regBase)
{
	long dirID = *(long *)(pb + pb_ioDirID);
	short index = *(short *)(pb + pb_ioFDirIndex);
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	unsigned long cnid, flags, sizeOrCount, parentID;
	unsigned long type, creator, crDate, modDate;

	if (index > 0) {
		reg_set(regBase,0,dirID); reg_set(regBase,1,(unsigned long)index);
		reg_set(regBase,2,(unsigned long)s_nameBuf);
		reg_command(regBase, 0x0202);
		if (reg_result(regBase) != 0) return -43;
	} else if (index == 0) {
		reg_set(regBase,0,dirID); reg_set(regBase,1,0);
		reg_set(regBase,2,(unsigned long)s_nameBuf);
		reg_command(regBase, 0x0202);
		if (reg_result(regBase) != 0) return -43;
	} else {
		if (nameAddr == 0) return -50;
		reg_set(regBase,0,dirID); reg_set(regBase,1,nameAddr);
		reg_set(regBase,2,(unsigned long)s_nameBuf);
		reg_command(regBase, 0x0203);
		if (reg_result(regBase) != 0) return -43;
	}

	cnid        = reg_get(regBase, 0);
	flags       = reg_get(regBase, 1);
	sizeOrCount = reg_get(regBase, 2);
	parentID    = reg_get(regBase, 3);

	/* Get type/creator/dates */
	reg_set(regBase,0,cnid);
	reg_command(regBase, 0x0207);
	type    = reg_get(regBase, 0);
	creator = reg_get(regBase, 1);
	crDate  = reg_get(regBase, 2);
	modDate = reg_get(regBase, 3);

	/* Copy name to caller's buffer */
	if (nameAddr != 0) {
		unsigned char len = (unsigned char)s_nameBuf[0];
		short i;
		*(unsigned char *)nameAddr = len;
		for (i = 0; i < len; i++)
			((char *)nameAddr)[1+i] = s_nameBuf[1+i];
	}

	if (flags & 0x10) {
		/* Directory */
		*(unsigned char *)(pb + pb_ioFlAttrib) = 0x10;
		*(long *)(pb + pb_ioDrNmFls) = sizeOrCount;
		*(long *)(pb + pb_ioDirID) = cnid;
		*(long *)(pb + pb_ioDrParID) = parentID;
		*(long *)(pb + pb_ioDrCrDat) = crDate;
		*(long *)(pb + pb_ioDrMdDat) = modDate;
	} else {
		/* File */
		*(unsigned char *)(pb + pb_ioFlAttrib) = 0x01;
		*(unsigned long *)(pb + pb_ioFlFndrInfo)     = type;
		*(unsigned long *)(pb + pb_ioFlFndrInfo + 4) = creator;
		*(long *)(pb + pb_ioFlNum) = cnid;
		*(long *)(pb + pb_ioFlLgLen) = sizeOrCount;
		*(long *)(pb + pb_ioFlPyLen) = sizeOrCount;
		*(long *)(pb + pb_ioFlRLgLen) = 0;
		*(long *)(pb + pb_ioFlRPyLen) = 0;
		*(long *)(pb + pb_ioFlCrDat) = crDate;
		*(long *)(pb + pb_ioFlMdDat) = modDate;
		*(long *)(pb + pb_ioDirID) = parentID;
	}
	return 0;
}

static OSErr DoOpen(char *pb, char *regBase, Ptr vcb)
{
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	unsigned long cnid, size, handle;
	short refNum;

	if (nameAddr == 0) return -50;

	/* Look up file by name in root (simplified — no WD yet) */
	reg_set(regBase,0,kRootDirID); reg_set(regBase,1,nameAddr);
	reg_command(regBase, 0x0209);
	cnid = reg_get(regBase, 0);
	if (cnid == 0) return -43;

	/* Get size */
	reg_set(regBase,0,kRootDirID); reg_set(regBase,1,nameAddr);
	reg_set(regBase,2,0);
	reg_command(regBase, 0x0203);
	if (reg_result(regBase) != 0) return -43;
	size = reg_get(regBase, 2);

	/* Open on host */
	reg_set(regBase,0,cnid); reg_set(regBase,1,0);
	reg_command(regBase, 0x0204);
	if (reg_result(regBase) != 0) return -43;
	handle = reg_get(regBase, 0);

	/* Allocate FCB */
	refNum = AllocFCB(vcb, cnid, size, 0);
	if (refNum == 0) {
		reg_set(regBase,0,handle);
		reg_command(regBase, 0x0206);
		return -42;
	}

	/* Store host handle in FCB's PLen field */
	{
		Ptr fcb = GetFCB(refNum);
		*(long *)(fcb + kFCBPLen) = handle;
	}

	*(short *)(pb + pb_ioRefNum) = refNum;
	return 0;
}

static OSErr DoRead(char *pb, char *regBase)
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
	if (fcb == NULL) return -43;

	mark = *(long *)(fcb + kFCBCrPs);
	eof  = *(long *)(fcb + kFCBEOF);
	handle = *(long *)(fcb + kFCBPLen);

	switch (posMode & 0x03) {
		case 1: mark = posOffset; break;
		case 2: mark = eof + posOffset; break;
		case 3: mark += posOffset; break;
	}
	if (mark < 0) mark = 0;
	if (mark > eof) mark = eof;

	if (reqCount <= 0) {
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		*(long *)(fcb + kFCBCrPs) = mark;
		return 0;
	}
	if (mark + reqCount > eof)
		reqCount = eof - mark;

	reg_set(regBase,0,handle);
	reg_set(regBase,1,(unsigned long)mark);
	reg_set(regBase,2,(unsigned long)reqCount);
	reg_set(regBase,3,buffer);
	reg_command(regBase, 0x0205);
	actual = reg_get(regBase, 0);

	mark += actual;
	*(long *)(fcb + kFCBCrPs) = mark;
	*(long *)(pb + pb_ioActCount) = actual;
	*(long *)(pb + pb_ioPosOffset) = mark;
	return (actual < (unsigned long)reqCount) ? -39 : 0;
}

static OSErr DoClose(char *pb, char *regBase)
{
	short refNum = *(short *)(pb + pb_ioRefNum);
	Ptr fcb = GetFCB(refNum);
	if (fcb == NULL) return -43;
	reg_set(regBase,0,*(unsigned long *)(fcb + kFCBPLen));
	reg_command(regBase, 0x0206);
	FreeFCB(refNum);
	return 0;
}

static OSErr DoGetFileInfo(char *pb, char *regBase)
{
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	unsigned long cnid, size, type, creator, crDate, modDate;

	if (nameAddr == 0) return -50;
	reg_set(regBase,0,kRootDirID); reg_set(regBase,1,nameAddr);
	reg_set(regBase,2,(unsigned long)s_nameBuf);
	reg_command(regBase, 0x0203);
	if (reg_result(regBase) != 0) return -43;

	cnid = reg_get(regBase, 0);
	size = reg_get(regBase, 2);

	reg_set(regBase,0,cnid);
	reg_command(regBase, 0x0207);
	type    = reg_get(regBase, 0);
	creator = reg_get(regBase, 1);
	crDate  = reg_get(regBase, 2);
	modDate = reg_get(regBase, 3);

	*(unsigned char *)(pb + pb_ioFlAttrib) = 0x01;
	*(unsigned long *)(pb + pb_ioFlFndrInfo)     = type;
	*(unsigned long *)(pb + pb_ioFlFndrInfo + 4) = creator;
	*(long *)(pb + pb_ioFlNum) = cnid;
	*(long *)(pb + pb_ioFlLgLen) = size;
	*(long *)(pb + pb_ioFlPyLen) = size;
	*(long *)(pb + pb_ioFlRLgLen) = 0;
	*(long *)(pb + pb_ioFlRPyLen) = 0;
	*(long *)(pb + pb_ioFlCrDat) = crDate;
	*(long *)(pb + pb_ioFlMdDat) = modDate;
	return 0;
}

static OSErr DoGetEOF(char *pb)
{
	Ptr fcb = GetFCB(*(short *)(pb + pb_ioRefNum));
	if (fcb == NULL) return -43;
	/* ioMisc at offset 28 holds the EOF for _GetEOF result */
	*(long *)(pb + 28) = *(long *)(fcb + kFCBEOF);
	return 0;
}

static OSErr DoGetFPos(char *pb)
{
	Ptr fcb = GetFCB(*(short *)(pb + pb_ioRefNum));
	if (fcb == NULL) return -43;
	*(long *)(pb + pb_ioPosOffset) = *(long *)(fcb + kFCBCrPs);
	*(long *)(pb + pb_ioReqCount)  = 0;
	*(long *)(pb + pb_ioActCount)  = 0;
	return 0;
}

static OSErr DoSetFPos(char *pb)
{
	short posMode = *(short *)(pb + pb_ioPosMode);
	long posOffset = *(long *)(pb + pb_ioPosOffset);
	Ptr fcb = GetFCB(*(short *)(pb + pb_ioRefNum));
	long mark, eof;
	if (fcb == NULL) return -43;

	mark = *(long *)(fcb + kFCBCrPs);
	eof  = *(long *)(fcb + kFCBEOF);
	switch (posMode & 0x03) {
		case 1: mark = posOffset; break;
		case 2: mark = eof + posOffset; break;
		case 3: mark += posOffset; break;
	}
	if (mark < 0) mark = 0;
	if (mark > eof) mark = eof;
	*(long *)(fcb + kFCBCrPs) = mark;
	*(long *)(pb + pb_ioPosOffset) = mark;
	return 0;
}

static OSErr DoGetVolInfo(char *pb, Globals *g)
{
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	if (nameAddr != 0) {
		unsigned char *p = (unsigned char *)nameAddr;
		p[0]=6; p[1]='S'; p[2]='h'; p[3]='a';
		p[4]='r'; p[5]='e'; p[6]='d';
	}
	*(short *)(pb + pb_ioVRefNum) = kOurVRefNum;
	*(short *)(pb + 30) = (short)0x8000; /* ioVAtrb: locked */
	*(short *)(pb + pb_ioDrNmFls) = (short)g->volFileCount;
	*(short *)(pb + 42) = 1024;          /* ioVNmAlBlks */
	*(long *)(pb + 44)  = 512;           /* ioVAlBlkSiz */
	*(short *)(pb + 50) = 0;             /* ioVFreeBks */
	*(long *)(pb + pb_ioFlCrDat) = 0;
	*(long *)(pb + pb_ioFlMdDat) = 0;
	return 0;
}

/* ================================================================ */
/*              Central dispatchers (called from stubs)             */
/* ================================================================ */

/*
	DispatchFlat: called from generated 68k stub code.
	pb = ParamBlockRec pointer, trapNum = low byte of trap word.
	Returns: 0 if handled, non-zero to pass through.
	If handled, ioResult is already set in pb.
*/
short DispatchFlat(char *pb, short trapNum)
{
	Globals *g;
	OSErr err;
	short refNum;

	SetUpA4();
	g = *(Globals **)kGlobalsPtr;
	if (g == NULL) { RestoreA4(); return 1; }

	/* Traps keyed on ioRefNum (open file) */
	switch (trapNum) {
		case 0x01: /* _Close */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoClose(pb, g->regBase);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x02: /* _Read */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoRead(pb, g->regBase);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x03: /* _Write */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x11: /* _GetEOF */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoGetEOF(pb);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x12: /* _SetEOF */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x18: /* _GetFPos */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoGetFPos(pb);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x44: /* _SetFPos */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoSetFPos(pb);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;
	}

	/* Traps keyed on ioVRefNum */
	if (!IsOurVolume(*(short *)(pb + pb_ioVRefNum))) {
		RestoreA4(); return 1;
	}

	switch (trapNum) {
		case 0x00: /* _Open */
			err = DoOpen(pb, g->regBase, g->vcb);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x07: /* _GetVolInfo */
			err = DoGetVolInfo(pb, g);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x08: /* _Create */
		case 0x09: /* _Delete */
		case 0x0B: /* _Rename */
		case 0x0D: /* _SetFileInfo */
		case 0x10: /* _Allocate */
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x0A: /* _OpenRF */
			*(short *)(pb + pb_ioResult) = -43;
			RestoreA4(); return 0;

		case 0x0C: /* _GetFileInfo */
			err = DoGetFileInfo(pb, g->regBase);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x0E: /* _UnmountVol */
		case 0x13: /* _FlushVol */
		case 0x17: /* _Eject */
			*(short *)(pb + pb_ioResult) = 0;
			RestoreA4(); return 0;
	}

	RestoreA4(); return 1;
}

/*
	DispatchHFS: called from the _HFSDispatch stub.
	pb = ParamBlockRec, selector = HFS call selector.
	Returns: 0 if handled, non-zero to pass through.
*/
short DispatchHFS(char *pb, short selector)
{
	Globals *g;
	OSErr err;

	SetUpA4();
	g = *(Globals **)kGlobalsPtr;
	if (g == NULL) { RestoreA4(); return 1; }

	if (!IsOurVolume(*(short *)(pb + pb_ioVRefNum))) {
		RestoreA4(); return 1;
	}

	switch (selector) {
		case kGetCatInfo:
			err = DoGetCatInfo(pb, g->regBase);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case kSetCatInfo:
		case kCatMove:
		case kDirCreate:
		case kSetVInfo:
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case kOpenWD:
		case kCloseWD:
		case kGetWDInfo:
			*(short *)(pb + pb_ioResult) = -50;
			RestoreA4(); return 0;

		case kCreateFileIDRef:
		case kDeleteFileIDRef:
		case kResolveFileIDRef:
			*(short *)(pb + pb_ioResult) = -50;
			RestoreA4(); return 0;
	}

	RestoreA4(); return 1;
}

/* ================================================================ */
/*            Dynamic 68k stub generation                           */
/* ================================================================ */

/*
	Generate a small 68k code stub in the system heap for one
	flat-file OS trap. The stub:

	  MOVEM.L D0-D2/A0-A2, -(SP)  ; save regs (24 bytes)
	  MOVE.W  #trapNum, -(SP)      ; push trapNum arg
	  MOVE.L  A0, -(SP)            ; push pb arg
	  JSR     dispatchAddr          ; call DispatchFlat
	  ADDQ.L  #6, SP               ; pop args
	  TST.W   D0                   ; handled?
	  BNE.S   @pass                ; no — pass through
	  MOVEM.L (SP)+, D0-D2/A0-A2  ; restore regs
	  MOVE.W  16(A0), D0           ; D0 = ioResult
	  RTS                          ; return to caller
	@pass:
	  MOVEM.L (SP)+, D0-D2/A0-A2  ; restore regs
	  MOVE.L  #oldAddr, -(SP)     ; push old trap address
	  RTS                          ; jump to original

	Total: ~40 bytes.
*/
static Ptr MakeFlatStub(short trapNum, long dispatchAddr, long oldAddr)
{
	Ptr p;
	short *w;

	p = NewPtrSys(42);
	if (p == NULL) return NULL;
	w = (short *)p;

	/* MOVEM.L D0-D2/A0-A2, -(SP) */
	*w++ = 0x48E7; *w++ = (short)0xE038;   /* D0-D2/A0-A2 */

	/* MOVE.W #trapNum, -(SP) */
	*w++ = 0x3F3C; *w++ = trapNum;

	/* MOVE.L A0, -(SP) */
	*w++ = 0x2F08;

	/* JSR dispatchAddr (absolute long) */
	*w++ = 0x4EB9;
	*(long *)w = dispatchAddr; w += 2;

	/* ADDQ.L #6, SP */
	*w++ = 0x5C8F;

	/* TST.W D0 */
	*w++ = 0x4A40;

	/* BNE.S @pass (skip 6 bytes: MOVEM.L + MOVE.W + RTS) */
	*w++ = 0x6606;

	/* MOVEM.L (SP)+, D0-D2/A0-A2 */
	*w++ = 0x4CDF; *w++ = 0x1C07;

	/* MOVE.W 16(A0), D0 — ioResult */
	*w++ = 0x3028; *w++ = 0x0010;

	/* RTS */
	*w++ = 0x4E75;

	/* @pass: MOVEM.L (SP)+, D0-D2/A0-A2 */
	*w++ = 0x4CDF; *w++ = 0x1C07;

	/* MOVE.L #oldAddr, -(SP) */
	*w++ = 0x2F3C;
	*(long *)w = oldAddr; w += 2;

	/* RTS (= JMP to old trap) */
	*w++ = 0x4E75;

	return p;
}

/*
	Generate an _HFSDispatch stub. Similar to flat but extracts
	the selector from D0 (the trap dispatcher passes it in D0
	as the low word on 68020, or we pick it from D1 on 68000).

	For _HFSDispatch, the selector is in the low byte of the
	trap word, which is in D1. But the Toolbox trap dispatcher
	works differently — for Toolbox traps, the selector is
	already extracted. We'll pass D0 as selector.

	Actually: _HFSDispatch ($A260) is a Toolbox trap (bit 11 set).
	The selector is passed in the register specified by the trap
	— for $A260, it's the word at 6(SP) on return from the macro
	expansion... This is complex. For now, let's just use D0
	which typically holds the selector for _HFSDispatch.
	
	REVISED: on System 6, _HFSDispatch is called with:
	  - A0 pointing to the parameter block
	  - The selector is on the stack (pushed by the glue code)
	    OR in D0.w — it depends on the File Manager glue.
	  Let's check D0 first. The HFS dispatch glue code
	  typically does: MOVE.W selector,D0 / _HFSDispatch.
*/
static Ptr MakeHFSStub(long dispatchAddr, long oldAddr)
{
	Ptr p;
	short *w;

	p = NewPtrSys(42);
	if (p == NULL) return NULL;
	w = (short *)p;

	/* MOVEM.L D0-D2/A0-A2, -(SP) */
	*w++ = 0x48E7; *w++ = (short)0xE038;

	/* Push selector (D0.W) as second arg */
	*w++ = 0x3F00;  /* MOVE.W D0, -(SP) */

	/* MOVE.L A0, -(SP) — pb */
	*w++ = 0x2F08;

	/* JSR dispatchAddr */
	*w++ = 0x4EB9;
	*(long *)w = dispatchAddr; w += 2;

	/* ADDQ.L #6, SP */
	*w++ = 0x5C8F;

	/* TST.W D0 */
	*w++ = 0x4A40;

	/* BNE.S @pass (skip 6 bytes) */
	*w++ = 0x6606;

	/* MOVEM.L (SP)+, D0-D2/A0-A2 */
	*w++ = 0x4CDF; *w++ = 0x1C07;

	/* MOVE.W 16(A0), D0 — ioResult */
	*w++ = 0x3028; *w++ = 0x0010;

	/* RTS */
	*w++ = 0x4E75;

	/* @pass: MOVEM.L (SP)+, D0-D2/A0-A2 */
	*w++ = 0x4CDF; *w++ = 0x1C07;

	/* MOVE.L #oldAddr, -(SP) */
	*w++ = 0x2F3C;
	*(long *)w = oldAddr; w += 2;

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
	stub = MakeFlatStub(trapWord & 0xFF, dispAddr, oldAddr);
	if (stub == NULL) {
		dbg_log1(regBase, "SharedDrive: stub alloc failed for %lx",
			(long)trapWord);
		return;
	}
	NSetTrapAddress((UniversalProcPtr)stub, trapWord, OSTrap);
}

static void InstallHFSPatch(char *regBase)
{
	long oldAddr;
	long dispAddr;
	Ptr stub;

	oldAddr = (long)NGetTrapAddress(0xA260, ToolTrap);
	dispAddr = (long)DispatchHFS;
	stub = MakeHFSStub(dispAddr, oldAddr);
	if (stub == NULL) {
		dbg_log(regBase, "SharedDrive: HFS stub alloc failed");
		return;
	}
	NSetTrapAddress((UniversalProcPtr)stub, 0xA260, ToolTrap);
	dbg_log1(regBase, "SharedDrive: HFS patch at %lx", (long)stub);
}

/* ================================================================ */
/*                         INIT entry point                         */
/* ================================================================ */

void main(void)
{
	char *regBase;
	unsigned long version;
	Globals *g;
	Handle self;
	Ptr myINITPtr;

	asm { move.l a0, myINITPtr }
	RememberA0();
	SetUpA4();

	regBase = find_reg_base();
	if (regBase == NULL) goto bail;

	dbg_log1(regBase, "SharedDrive INIT: regBase=%lx",
		(unsigned long)regBase);

	/* Check version */
	reg_command(regBase, 0x0200);
	version = reg_get(regBase, 0);
	if (version < 1) {
		dbg_log(regBase, "SharedDrive: no shared/ or version 0");
		goto bail;
	}
	dbg_log1(regBase, "SharedDrive INIT: version=%ld", version);

	/* Get volume stats */
	reg_command(regBase, 0x0201);

	/* Allocate globals */
	g = (Globals *)NewPtrSysClear(sizeof(Globals));
	if (g == NULL) {
		dbg_log(regBase, "SharedDrive: NewPtrSys failed");
		goto bail;
	}
	g->regBase = regBase;
	g->volFileCount = reg_get(regBase, 0);
	g->volTotalBytes = reg_get(regBase, 1);

	/* Save A4 for code resource data access from stubs */
	asm { move.l a4, g->savedA4 }

	*(Globals **)kGlobalsPtr = g;

	dbg_log2(regBase, "SharedDrive: %ld files, %ld bytes",
		g->volFileCount, g->volTotalBytes);

	/* Keep our code resource in memory */
	self = GetResource('INIT', 315);
	if (self != NULL) {
		DetachResource(self);
		HLock(self);
		HNoPurge(self);
	}

	/* Allocate and fill VCB */
	g->vcb = NewPtrSysClear(178);
	if (g->vcb == NULL) {
		dbg_log(regBase, "SharedDrive: VCB alloc failed");
		goto bail;
	}
	{
		Ptr v = g->vcb;
		unsigned long now;
		GetDateTime(&now);

		v[7] = 6; /* vcbVN length byte */
		v[8]='S'; v[9]='h'; v[10]='a';
		v[11]='r'; v[12]='e'; v[13]='d';

		*(short *)(v + 70)  = kOurVRefNum;   /* vcbVRefNum */
		*(short *)(v + 72)  = kOurDriveNum;  /* vcbDrvNum */
		*(short *)(v + 74)  = kOurDrvrRefNum;/* vcbDRefNum */
		*(short *)(v + 76)  = (short)0x8080; /* vcbAtrb: locked */
		*(short *)(v + 78)  = (short)g->volFileCount;
		*(long  *)(v + 82)  = now;           /* vcbCrDate */
		*(long  *)(v + 86)  = now;           /* vcbLsMod */
		*(short *)(v + 94)  = 1024;          /* vcbNmAlBlks */
		*(long  *)(v + 96)  = 512;           /* vcbAlBlkSiz */
		*(long  *)(v + 98)  = 512;           /* vcbClpSiz */
		*(short *)(v + 100) = 0;             /* vcbFreeBks */
	}
	Enqueue((QElemPtr)g->vcb, (QHdrPtr)kVCBQHdr);
	dbg_log1(regBase, "SharedDrive: VCB at %lx", (long)g->vcb);

	/* Install trap patches */
	InstallHFSPatch(regBase);

	/* Flat-file traps */
	InstallFlatPatch(0xA000, regBase);  /* _Open */
	InstallFlatPatch(0xA001, regBase);  /* _Close */
	InstallFlatPatch(0xA002, regBase);  /* _Read */
	InstallFlatPatch(0xA003, regBase);  /* _Write */
	InstallFlatPatch(0xA007, regBase);  /* _GetVolInfo */
	InstallFlatPatch(0xA008, regBase);  /* _Create */
	InstallFlatPatch(0xA009, regBase);  /* _Delete */
	InstallFlatPatch(0xA00A, regBase);  /* _OpenRF */
	InstallFlatPatch(0xA00B, regBase);  /* _Rename */
	InstallFlatPatch(0xA00C, regBase);  /* _GetFileInfo */
	InstallFlatPatch(0xA00D, regBase);  /* _SetFileInfo */
	InstallFlatPatch(0xA00E, regBase);  /* _UnmountVol */
	InstallFlatPatch(0xA010, regBase);  /* _Allocate */
	InstallFlatPatch(0xA011, regBase);  /* _GetEOF */
	InstallFlatPatch(0xA012, regBase);  /* _SetEOF */
	InstallFlatPatch(0xA013, regBase);  /* _FlushVol */
	InstallFlatPatch(0xA017, regBase);  /* _Eject */
	InstallFlatPatch(0xA018, regBase);  /* _GetFPos */
	InstallFlatPatch(0xA044, regBase);  /* _SetFPos */

	dbg_log(regBase, "SharedDrive: traps patched");

	/* Post disk-inserted event */
	PostEvent(7, (long)kOurDriveNum);
	dbg_log(regBase, "SharedDrive INIT: done!");

bail:
	RestoreA4();
}
