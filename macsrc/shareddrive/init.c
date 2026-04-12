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
#define kDefVCBPtr      0x0352

/* FCB offsets (from Inside Macintosh IV-179) */
#define kFCBLen         94
#define kFCBFlNum       0    /* LONGINT — file number (0=free) */
#define kFCBFlags       4    /* byte — flags */
#define kFCBTypByt      5    /* byte — version number */
/* fcbSBlk at 6, 2 bytes — first alloc block (not used for HFS) */
#define kFCBEOF         8    /* LONGINT — logical EOF */
#define kFCBPLen        12   /* LONGINT — physical EOF */
#define kFCBCrPs        16   /* LONGINT — mark (current position) */
#define kFCBVPtr        20   /* LONGINT — pointer to VCB */

/*
	ParamBlockRec / CInfoPBRec field offsets from A0.
	Computed from Inside Macintosh IV-155..IV-177.
	FInfo = 16 bytes (fdType 4 + fdCreator 4 + fdFlags 2 + fdLocation 4 + fdFldr 2)
*/

/* Shared header (all variants) */
#define pb_ioResult     16
#define pb_ioNamePtr    18
#define pb_ioVRefNum    22
#define pb_ioRefNum     24   /* also ioFRefNum */

/* ioParam variant (for _Open, _Read, _Write, _Close, _GetEOF, _SetFPos...) */
#define pb_ioMisc       28   /* Ptr — _GetEOF result goes here */
#define pb_ioBuffer     32   /* Ptr */
#define pb_ioReqCount   36   /* LONGINT */
#define pb_ioActCount   40   /* LONGINT */
#define pb_ioPosMode    44   /* INTEGER */
#define pb_ioPosOffset  46   /* LONGINT */

/* fileParam variant (for _GetFileInfo, _SetFileInfo, _Create, _Delete) */
#define pb_ioFDirIndex  28   /* INTEGER */
#define pb_ioFlAttrib   30   /* SignedByte */
#define pb_ioFlFndrInfo 32   /* FInfo, 16 bytes */
#define pb_ioFlNum      48   /* LONGINT — file number */
#define pb_ioFlStBlk    52   /* INTEGER */
#define pb_ioFlLgLen    54   /* LONGINT — data fork logical EOF */
#define pb_ioFlPyLen    58   /* LONGINT — data fork physical EOF */
#define pb_ioFlRStBlk   62   /* INTEGER */
#define pb_ioFlRLgLen   64   /* LONGINT — rsrc fork logical EOF */
#define pb_ioFlRPyLen   68   /* LONGINT — rsrc fork physical EOF */
#define pb_ioFlCrDat    72   /* LONGINT — creation date */
#define pb_ioFlMdDat    76   /* LONGINT — modification date */

/* CInfoPBRec hFileInfo variant (for PBGetCatInfo, files) */
/* Same as fileParam up through ioFlMdDat, then: */
#define pb_ioFlParID    100  /* LONGINT — parent dir ID */

/* CInfoPBRec dirInfo variant (for PBGetCatInfo, directories) */
/* Shares header + ioFDirIndex/ioFlAttrib with hFileInfo */
#define pb_ioDrUsrWds   32   /* DInfo, 16 bytes */
#define pb_ioDrDirID    48   /* LONGINT — directory ID */
#define pb_ioDrNmFls    52   /* INTEGER — number of files in dir */
#define pb_ioDrCrDat    72   /* LONGINT */
#define pb_ioDrMdDat    76   /* LONGINT */
#define pb_ioDrParID    100  /* LONGINT */

/* volumeParam variant (for _GetVolInfo / PBHGetVInfo) */
#define pb_ioVolIndex   28   /* INTEGER */
#define pb_ioVCrDate    30   /* LONGINT */
#define pb_ioVLsMod     34   /* LONGINT (HFS) */
#define pb_ioVAtrb      38   /* INTEGER */
#define pb_ioVNmFls     40   /* INTEGER */
#define pb_ioVNmAlBlks  46   /* INTEGER */
#define pb_ioVAlBlkSiz  48   /* LONGINT */
#define pb_ioVClpSiz    52   /* LONGINT */
#define pb_ioVFrBlk     62   /* INTEGER */

/* HFS-specific PBGetCatInfo: ioDirID is at same offset as ioFlNum */
#define pb_ioDirID      48

/* WDParam variant (for _OpenWD, _CloseWD, _GetWDInfo) */
#define pb_ioWDIndex    26   /* INTEGER */
#define pb_ioWDProcID   28   /* LONGINT */
#define pb_ioWDVRefNum  32   /* INTEGER */
#define pb_ioWDDirID    48   /* LONGINT */

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
#define kGetVolParms      0x0030

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
	if (vRefNum == kOurVRefNum) return true;
	/* WD refnums we issue are encoded as -(wdRef+32000).
	   The host assigns wdRef starting from 1, so valid
	   encoded values are -32001, -32002, ...
	   Real File Manager WDCBs use small negative numbers (>= -32767)
	   that won't overlap with our range below -32000. */
	if (vRefNum < kOurVRefNum && vRefNum > -32100) return true;
	/* vRefNum 0 means "default volume". Check if the default VCB
	   is ours by reading the DefVCBPtr low-memory global. */
	if (vRefNum == 0) {
		Globals *g = *(Globals **)kGlobalsPtr;
		if (g != NULL && *(Ptr *)kDefVCBPtr == g->vcb)
			return true;
	}
	return false;
}

/* ================================================================ */
/*                    File Manager handlers                         */
/* ================================================================ */

/* Resolve a vRefNum that might be a WD refnum to a dirID.
   If it's our real vRefNum, return kRootDirID.
   If it's one of our WD refnums, query the host for the dirID. */
static long ResolveDir(short vRefNum, long dirID, char *regBase)
{
	if (dirID != 0) return dirID;  /* explicit dirID overrides */
	if (vRefNum == kOurVRefNum) return kRootDirID;
	/* Must be a WD refnum */
	{
		unsigned long wdRef = (unsigned long)(-(long)vRefNum - 32000);
		reg_set(regBase, 0, wdRef);
		reg_command(regBase, 0x020A);
		if (reg_result(regBase) == 0)
			return (long)reg_get(regBase, 1);
	}
	return kRootDirID;
}

static OSErr DoGetCatInfo(char *pb, char *regBase)
{
	short vRefNum = *(short *)(pb + pb_ioVRefNum);
	long dirID = *(long *)(pb + pb_ioDirID);
	short index = *(short *)(pb + pb_ioFDirIndex);
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	unsigned long cnid, flags, sizeOrCount, parentID;
	unsigned long type, creator, crDate, modDate;

	/* Resolve WD refnum → dirID when caller doesn't set ioDirID */
	dirID = ResolveDir(vRefNum, dirID, regBase);

	dbg_log2(regBase, "SD: GCI dir=%ld idx=%ld", dirID, (long)index);

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
		dbg_log1(regBase, "SD: GCI byName=%s", nameAddr);
		reg_set(regBase,0,dirID); reg_set(regBase,1,nameAddr);
		reg_set(regBase,2,(unsigned long)s_nameBuf);
		reg_command(regBase, 0x0203);
		if (reg_result(regBase) != 0) {
			dbg_log(regBase, "SD: GCI byName -> fnfErr");
			return -43;
		}
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
		*(short *)(pb + pb_ioDrNmFls) = (short)sizeOrCount;
		*(long *)(pb + pb_ioDrDirID) = cnid;
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
		*(long *)(pb + pb_ioFlParID) = parentID;
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
	/* _GetEOF returns result in ioMisc (offset 28) */
	*(long *)(pb + pb_ioMisc) = *(long *)(fcb + kFCBEOF);
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
	Ptr v = g->vcb;

	dbg_log(g->regBase, "SD: DoGetVolInfo");

	if (nameAddr != 0) {
		unsigned char *p = (unsigned char *)nameAddr;
		p[0]=6; p[1]='S'; p[2]='h'; p[3]='a';
		p[4]='r'; p[5]='e'; p[6]='d';
	}

	*(short *)(pb + pb_ioVRefNum) = kOurVRefNum;
	*(long  *)(pb + 30) = *(long *)(v + 10);     /* ioVCrDate from vcbCrDate */
	*(long  *)(pb + 34) = *(long *)(v + 14);     /* ioVLsMod from vcbLsMod */
	*(short *)(pb + 38) = (short)0x8000;         /* ioVAtrb: sw locked */
	*(short *)(pb + 40) = (short)g->volFileCount;/* ioVNmFls */
	*(short *)(pb + 42) = 0;                     /* ioVBitMap */
	*(short *)(pb + 44) = 0;                     /* ioVAllocPtr */
	*(short *)(pb + 46) = 1024;                  /* ioVNmAlBlks */
	*(long  *)(pb + 48) = 512;                   /* ioVAlBlkSiz */
	*(long  *)(pb + 52) = 512;                   /* ioVClpSiz */
	*(short *)(pb + 56) = 0;                     /* ioAlBlSt */
	*(long  *)(pb + 58) = 16;                    /* ioVNxtCNID */
	*(short *)(pb + 62) = 0;                     /* ioVFrBlk */
	*(short *)(pb + 64) = 0x4244;                /* ioVSigWord = HFS */
	*(short *)(pb + 66) = kOurDriveNum;          /* ioVDrvInfo */
	*(short *)(pb + 68) = kOurDrvrRefNum;        /* ioVDRefNum */
	*(short *)(pb + 70) = 0x5344;                /* ioVFSID = 'SD' */
	*(long  *)(pb + 72) = 0;                     /* ioVBkUp */
	*(short *)(pb + 76) = 0;                     /* ioVSeqNum */
	*(long  *)(pb + 78) = 0;                     /* ioVWrCnt */
	*(long  *)(pb + 82) = g->volFileCount;       /* ioVFilCnt */
	*(long  *)(pb + 86) = 1;                     /* ioVDirCnt */
	/* ioVFndrInfo at 90, 32 bytes — leave as caller had them */
	return 0;
}

/*
	DoGetVolParms: return volume capabilities.
	The caller provides a buffer (ioBuffer, ioReqCount).
	We fill a GetVolParmsInfoBuffer version 1 (14 bytes).
*/
static OSErr DoGetVolParms(char *pb, char *regBase)
{
	unsigned long bufAddr = *(unsigned long *)(pb + pb_ioBuffer);
	long reqCount = *(long *)(pb + pb_ioReqCount);
	long actual;

	dbg_log1(regBase, "SD: GetVolParms buf=%lx", bufAddr);

	if (bufAddr == 0) return -50;

	/*
		GetVolParmsInfoBuffer v1 layout:
		  0: vMVersion   INTEGER (2)
		  2: vMAttrib    LONGINT (4)
		  6: vMLocalHand Handle  (4)
		 10: vMServerAdr LONGINT (4)
		 Total: 14 bytes

		vMAttrib bits:
		  16 bExtFSVol, 17 bNoSysDir, 19 bNoBootBlks,
		  20 bNoDeskItems, 27 bNoLclSync, 28 bNoVNEdit
	*/

	actual = 14;
	if (reqCount < actual) actual = reqCount;

	/* Zero the buffer first */
	{
		long j;
		for (j = 0; j < actual; j++)
			*(char *)(bufAddr + j) = 0;
	}

	/* vMVersion = 1 */
	if (actual >= 2)
		*(short *)(bufAddr + 0) = 1;

	/* vMAttrib = bExtFSVol | bNoSysDir | bNoBootBlks | bNoDeskItems | bNoLclSync | bNoVNEdit */
	if (actual >= 6)
		*(long *)(bufAddr + 2) = (long)0x181B0000UL;

	*(long *)(pb + pb_ioActCount) = actual;
	return 0;
}

static OSErr DoOpenWD(char *pb, char *regBase)
{
	long dirID = *(long *)(pb + pb_ioWDDirID);
	long procID = *(long *)(pb + pb_ioWDProcID);
	unsigned long wdRef;

	dbg_log2(regBase, "SD: OpenWD dir=%ld proc=%lx", dirID, procID);

	reg_set(regBase, 0, (unsigned long)kOurVRefNum);
	reg_set(regBase, 1, (unsigned long)dirID);
	reg_command(regBase, 0x020B);
	if (reg_result(regBase) != 0) return -43;
	wdRef = reg_get(regBase, 0);

	/* Return WD refnum in ioVRefNum.  We encode it as a negative
	   number in the range used by WDCBs (the File Manager uses
	   negative values starting from -32767). We use -(wdRef+32000)
	   so there's no collision with our vRefNum (-32000).  Since the
	   host allocates small sequential integers, this is safe. */
	*(short *)(pb + pb_ioVRefNum) = (short)(-(long)wdRef - 32000);
	return 0;
}

static OSErr DoCloseWD(char *pb, char *regBase)
{
	short vRefNum = *(short *)(pb + pb_ioVRefNum);
	unsigned long wdRef;

	/* Decode WD refnum */
	wdRef = (unsigned long)(-(long)vRefNum - 32000);
	dbg_log1(regBase, "SD: CloseWD ref=%ld", (long)wdRef);

	reg_set(regBase, 0, wdRef);
	reg_command(regBase, 0x020C);
	return 0;
}

static OSErr DoGetWDInfo(char *pb, char *regBase)
{
	short vRefNum = *(short *)(pb + pb_ioVRefNum);
	short wdIndex = *(short *)(pb + pb_ioWDIndex);
	unsigned long wdRef;
	unsigned long dirID;

	dbg_log2(regBase, "SD: GetWDInfo vref=%ld idx=%ld",
		(long)vRefNum, (long)wdIndex);

	/* Only handle direct lookup (ioWDIndex == 0) for now */
	if (wdIndex != 0) return -35;

	/* Decode WD refnum */
	wdRef = (unsigned long)(-(long)vRefNum - 32000);

	reg_set(regBase, 0, wdRef);
	reg_command(regBase, 0x020A);
	if (reg_result(regBase) != 0) return -35;

	dirID = reg_get(regBase, 1);

	*(short *)(pb + pb_ioWDVRefNum) = kOurVRefNum;
	*(long *)(pb + pb_ioWDDirID) = dirID;
	*(long *)(pb + pb_ioWDProcID) = 0;
	/* Return volume name if requested */
	{
		unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
		if (nameAddr != 0) {
			unsigned char *p = (unsigned char *)nameAddr;
			p[0]=6; p[1]='S'; p[2]='h'; p[3]='a';
			p[4]='r'; p[5]='e'; p[6]='d';
		}
	}
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
	short vRefNum;
	unsigned long nameAddr;

	SetUpA4();
	g = *(Globals **)kGlobalsPtr;
	if (g == NULL) { RestoreA4(); return 1; }

	vRefNum  = *(short *)(pb + pb_ioVRefNum);
	nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);

	/* Log every entry: trap, vRefNum, name */
	dbg_log2(g->regBase, "SD:F %02lx vr=%ld", (long)trapNum,
		(long)vRefNum);
	if (nameAddr)
		dbg_log1(g->regBase, "SD:F  nm=%S", nameAddr);

	/* Traps keyed on ioRefNum (open file) */
	switch (trapNum) {
		case 0x01: /* _Close */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			dbg_log1(g->regBase, "SD: Close ref=%ld",
				(long)refNum);
			err = DoClose(pb, g->regBase);
			dbg_log1(g->regBase, "SD: Close -> %ld", (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x02: /* _Read */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			dbg_log2(g->regBase, "SD: Read ref=%ld cnt=%ld",
				(long)refNum, *(long *)(pb + pb_ioReqCount));
			err = DoRead(pb, g->regBase);
			dbg_log2(g->regBase, "SD: Read -> %ld act=%ld",
				(long)err, *(long *)(pb + pb_ioActCount));
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x03: /* _Write */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			dbg_log1(g->regBase, "SD: Write ref=%ld -> wPrErr",
				(long)refNum);
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x11: /* _GetEOF */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoGetEOF(pb);
			dbg_log2(g->regBase, "SD: GetEOF ref=%ld -> %ld",
				(long)refNum, (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x12: /* _SetEOF */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			dbg_log1(g->regBase, "SD: SetEOF ref=%ld -> wPrErr",
				(long)refNum);
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x18: /* _GetFPos */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoGetFPos(pb);
			dbg_log2(g->regBase, "SD: GetFPos ref=%ld -> %ld",
				(long)refNum, (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case 0x44: /* _SetFPos */
			refNum = *(short *)(pb + pb_ioRefNum);
			if (!IsOurFCB(refNum)) { RestoreA4(); return 1; }
			err = DoSetFPos(pb);
			dbg_log2(g->regBase, "SD: SetFPos ref=%ld -> %ld",
				(long)refNum, (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;
	}

	/* Traps keyed on ioVRefNum */
	if (!IsOurVolume(vRefNum)) {
		RestoreA4(); return 1;
	}

	switch (trapNum) {
		case 0x00: /* _Open */
		{
			dbg_log1(g->regBase, "SD: Open nm=%S", nameAddr);
			err = DoOpen(pb, g->regBase, g->vcb);
			dbg_log1(g->regBase, "SD: Open -> %ld", (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;
		}

		case 0x07: /* _GetVolInfo */
		{
			short vidx = *(short *)(pb + pb_ioVolIndex);
			dbg_log2(g->regBase,
				"SD: GetVolInfo vidx=%ld nm=%S",
				(long)vidx, nameAddr);
			err = DoGetVolInfo(pb, g);
			dbg_log1(g->regBase, "SD: GetVolInfo -> %ld",
				(long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;
		}

		case 0x08: /* _Create */
			dbg_log1(g->regBase, "SD: Create nm=%S -> wPrErr",
				nameAddr);
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x09: /* _Delete */
			dbg_log1(g->regBase, "SD: Delete nm=%S -> wPrErr",
				nameAddr);
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x0A: /* _OpenRF */
			dbg_log1(g->regBase, "SD: OpenRF nm=%S -> fnfErr",
				nameAddr);
			*(short *)(pb + pb_ioResult) = -43;
			RestoreA4(); return 0;

		case 0x0B: /* _Rename */
			dbg_log1(g->regBase, "SD: Rename nm=%S -> wPrErr",
				nameAddr);
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x0C: /* _GetFileInfo */
		{
			short fidx = *(short *)(pb + pb_ioFDirIndex);
			dbg_log2(g->regBase, "SD: GFI nm=%S idx=%ld",
				nameAddr, (long)fidx);
			err = DoGetFileInfo(pb, g->regBase);
			dbg_log1(g->regBase, "SD: GFI -> %ld", (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;
		}

		case 0x0D: /* _SetFileInfo */
			dbg_log1(g->regBase,
				"SD: SetFileInfo nm=%S -> wPrErr", nameAddr);
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x0E: /* _UnmountVol */
			dbg_log(g->regBase, "SD: UnmountVol -> 0");
			*(short *)(pb + pb_ioResult) = 0;
			RestoreA4(); return 0;

		case 0x10: /* _Allocate */
			dbg_log(g->regBase, "SD: Allocate -> wPrErr");
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case 0x13: /* _FlushVol */
			dbg_log(g->regBase, "SD: FlushVol -> 0");
			*(short *)(pb + pb_ioResult) = 0;
			RestoreA4(); return 0;

		case 0x17: /* _Eject */
			dbg_log(g->regBase, "SD: Eject -> 0");
			*(short *)(pb + pb_ioResult) = 0;
			RestoreA4(); return 0;
	}

	dbg_log1(g->regBase, "SD: UNHANDLED flat %02lx", (long)trapNum);
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
	short vRefNum;
	unsigned long nameAddr;

	SetUpA4();
	g = *(Globals **)kGlobalsPtr;
	if (g == NULL) { RestoreA4(); return 1; }

	vRefNum  = *(short *)(pb + pb_ioVRefNum);
	nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);

	/* Log every HFS dispatch entry */
	dbg_log2(g->regBase, "SD:H %04lx vr=%ld",
		(long)selector, (long)vRefNum);
	if (nameAddr)
		dbg_log1(g->regBase, "SD:H  nm=%S", nameAddr);

	if (!IsOurVolume(vRefNum)) {
		RestoreA4(); return 1;
	}

	switch (selector) {
		case kGetCatInfo:
		{
			short idx = *(short *)(pb + pb_ioFDirIndex);
			long dirID = *(long *)(pb + pb_ioDirID);
			dbg_log2(g->regBase,
				"SD: GCI dir=%ld idx=%ld", dirID, (long)idx);
			if (nameAddr)
				dbg_log1(g->regBase, "SD: GCI nm=%S", nameAddr);
			err = DoGetCatInfo(pb, g->regBase);
			dbg_log1(g->regBase, "SD: GCI -> %ld", (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;
		}

		case kSetCatInfo:
			dbg_log(g->regBase, "SD: SetCatInfo -> wPrErr");
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case kCatMove:
			dbg_log(g->regBase, "SD: CatMove -> wPrErr");
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case kDirCreate:
			dbg_log(g->regBase, "SD: DirCreate -> wPrErr");
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case kSetVInfo:
			dbg_log(g->regBase, "SD: SetVInfo -> wPrErr");
			*(short *)(pb + pb_ioResult) = -46;
			RestoreA4(); return 0;

		case kOpenWD:
			err = DoOpenWD(pb, g->regBase);
			dbg_log1(g->regBase, "SD: OpenWD -> %ld", (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case kCloseWD:
			err = DoCloseWD(pb, g->regBase);
			dbg_log1(g->regBase, "SD: CloseWD -> %ld", (long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case kGetWDInfo:
			err = DoGetWDInfo(pb, g->regBase);
			dbg_log1(g->regBase, "SD: GetWDInfo -> %ld",
				(long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;

		case kGetVolParms:
		{
			long rc = *(long *)(pb + pb_ioReqCount);
			dbg_log1(g->regBase, "SD: GetVolParms reqCnt=%ld",
				rc);
			err = DoGetVolParms(pb, g->regBase);
			dbg_log1(g->regBase, "SD: GetVolParms -> %ld",
				(long)err);
			*(short *)(pb + pb_ioResult) = err;
			RestoreA4(); return 0;
		}

		case kGetFCBInfo:
			dbg_log(g->regBase, "SD: GetFCBInfo -> paramErr");
			*(short *)(pb + pb_ioResult) = -50;
			RestoreA4(); return 0;

		default:
			dbg_log1(g->regBase, "SD: HFS UNHANDLED sel=%04lx",
				(long)selector);
			*(short *)(pb + pb_ioResult) = -50;
			RestoreA4(); return 0;
	}
}

/* ================================================================ */
/*            Dynamic 68k stub generation                           */
/* ================================================================ */

/*
	Generate a small 68k code stub in the system heap for one
	flat-file OS trap. The stub:

	  MOVEM.L D0-D2/A0-A1, -(SP)  ; save regs (20 bytes)
	  MOVE.W  #trapNum, -(SP)      ; push trapNum arg
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
	  4: MOVE.W  #imm,-(SP)         3F3C xxxx       4
	  8: MOVE.L  A0,-(SP)           2F08            2
	 10: JSR     abs.L              4EB9 xxxx xxxx  6
	 16: ADDQ.L  #6,SP              5C8F            2
	 18: TST.W   D0                 4A40            2
	 20: BNE.S   +10                660A            2
	 22: MOVEM.L (SP)+,D0-D2/A0-A1 4CDF 0307       4
	 26: MOVE.W  16(A0),D0         3028 0010       4
	 30: RTS                        4E75            2
	 32: MOVEM.L (SP)+,D0-D2/A0-A1 4CDF 0307       4
	 36: MOVE.L  #imm,-(SP)         2F3C xxxx xxxx  6
	 42: RTS                        4E75            2
	 Total: 44 bytes

	Register mask encoding (68000 PRM):
	  Predecrement: bit15=D0..bit8=D7, bit7=A0..bit0=A7
	    D0-D2 = $E000, A0-A1 = $00C0 → $E0C0
	  Postincrement: bit0=D0..bit7=D7, bit8=A0..bit15=A7
	    D0-D2 = $0007, A0-A1 = $0300 → $0307
*/
static Ptr MakeFlatStub(short trapNum, long dispatchAddr, long oldAddr)
{
	Ptr p;
	short *w;

	p = NewPtrSys(44);
	if (p == NULL) return NULL;
	w = (short *)p;

	/* MOVEM.L D0-D2/A0-A1, -(SP) */
	*w++ = 0x48E7; *w++ = (short)0xE0C0;

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

	/* BNE.S @pass — displacement +10 = $0A (skip 4+4+2 bytes) */
	*w++ = 0x660A;

	/* MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF; *w++ = 0x0307;

	/* MOVE.W 16(A0), D0 — ioResult */
	*w++ = 0x3028; *w++ = 0x0010;

	/* RTS */
	*w++ = 0x4E75;

	/* @pass: MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF; *w++ = 0x0307;

	/* MOVE.L #oldAddr, -(SP) */
	*w++ = 0x2F3C;
	*(long *)w = oldAddr; w += 2;

	/* RTS (= JMP to old trap) */
	*w++ = 0x4E75;

	return p;
}

/*
	Generate an _HFSDispatch stub. Same structure as flat stub
	but reads D0.W as the HFS selector instead of a constant trapNum.

	_HFSDispatch ($A260) is an OS trap (bit 11 = 0). The glue code
	sets D0.W = selector, A0 = parameter block, then traps.

	Byte layout: same 44 bytes as MakeFlatStub except byte 4 uses
	MOVE.W D0,-(SP) instead of MOVE.W #imm,-(SP), saving 2 bytes
	(total 42 bytes).

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
	*w++ = 0x48E7; *w++ = (short)0xE0C0;

	/* MOVE.W D0, -(SP) — push selector */
	*w++ = 0x3F00;

	/* MOVE.L A0, -(SP) — push pb */
	*w++ = 0x2F08;

	/* JSR dispatchAddr */
	*w++ = 0x4EB9;
	*(long *)w = dispatchAddr; w += 2;

	/* ADDQ.L #6, SP */
	*w++ = 0x5C8F;

	/* TST.W D0 */
	*w++ = 0x4A40;

	/* BNE.S @pass — displacement +10 = $0A */
	*w++ = 0x660A;

	/* MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF; *w++ = 0x0307;

	/* MOVE.W 16(A0), D0 — ioResult */
	*w++ = 0x3028; *w++ = 0x0010;

	/* RTS */
	*w++ = 0x4E75;

	/* @pass: MOVEM.L (SP)+, D0-D2/A0-A1 */
	*w++ = 0x4CDF; *w++ = 0x0307;

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
	if (stub == NULL) {
		dbg_log(regBase, "SharedDrive: HFS stub alloc failed");
		return;
	}
	NSetTrapAddress((long)stub, 0xA260, OSTrap);
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
	{
		long *a4dst = &g->savedA4;
		asm {
			move.l a4dst, a0
			move.l a4, (a0)
		}
	}

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

		/* VCB field offsets from Inside Macintosh IV-155 */
		*(short *)(v + 4)   = 1;             /* qType = fsQType */
		*(short *)(v + 8)   = 0x4244;        /* vcbSigWord = HFS */
		*(long  *)(v + 10)  = now;           /* vcbCrDate */
		*(long  *)(v + 14)  = now;           /* vcbLsMod */
		*(short *)(v + 18)  = (short)0x8000; /* vcbAtrb: software locked */
		*(short *)(v + 20)  = (short)g->volFileCount; /* vcbNmFls */
		*(short *)(v + 26)  = 1024;          /* vcbNmAlBlks */
		*(long  *)(v + 28)  = 512;           /* vcbAlBlkSiz */
		*(long  *)(v + 32)  = 512;           /* vcbClpSiz */
		*(long  *)(v + 38)  = 16;            /* vcbNxtCNID */
		*(short *)(v + 42)  = 0;             /* vcbFreeBks */

		/* vcbVN at offset 44: Pascal string, 1 len + 27 chars */
		v[44] = 6;
		v[45]='S'; v[46]='h'; v[47]='a';
		v[48]='r'; v[49]='e'; v[50]='d';

		*(short *)(v + 72)  = kOurDriveNum;  /* vcbDrvNum */
		*(short *)(v + 74)  = kOurDrvrRefNum;/* vcbDRefNum */
		*(short *)(v + 76)  = 0x5344;        /* vcbFSID = 'SD' (ext FS) */
		*(short *)(v + 78)  = kOurVRefNum;   /* vcbVRefNum */
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

	/* Enable host-side trap tracing so we can see what the
	   system does with our volume after the INIT finishes. */
	reg_command(regBase, 0x020E); /* BeginTraceTraps */

	/*
	 * Don't PostEvent — it would trigger _MountVol for drive 8,
	 * which fails (no real disk / no DQE).  The VCB is already in
	 * the queue so the Finder will discover it at startup.
	 */
	dbg_log(regBase, "SharedDrive INIT: done!");

bail:
	RestoreA4();
}
