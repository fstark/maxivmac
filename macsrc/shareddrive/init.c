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

/* FCB offsets (inside the FCB buffer) */
#define kFCBLen         94      /* size of one FCB entry */
#define kFCBFlNum       0       /* long: CNID */
#define kFCBFlags       4       /* byte: flags */
#define kFCBTypByt      5       /* byte: fork type (0=data) */
#define kFCBEOF         6       /* long: logical EOF */
#define kFCBPLen        10      /* long: physical EOF */
#define kFCBCrPs        14      /* long: mark (current position) */
#define kFCBVPtr        18      /* long: ptr to VCB */
/* FCB flags */
#define kFCBWriteBit    0x01    /* file open for writing */
#define kFCBResourceBit 0x02    /* resource fork */

/* HFSDispatch selectors */
#define kGetCatInfo     0x0009
#define kSetCatInfo     0x000A
#define kOpenWD         0x0001
#define kCloseWD        0x0002
#define kGetWDInfo      0x0007
#define kCatMove        0x0005
#define kDirCreate      0x0006
#define kMakeFSSpec     0x0020
#define kSetVInfo       0x000B
#define kGetFCBInfo     0x0008
#define kCreateFileIDRef 0x0010
#define kDeleteFileIDRef 0x0011
#define kResolveFileIDRef 0x0012

/* ioFlAttrib directory flag */
#define kDirMask        0x10

/* ParamBlockRec field offsets (relative to A0) */
#define ioResult        16      /* word */
#define ioNamePtr       18      /* long */
#define ioVRefNum       22      /* word */
#define ioRefNum        24      /* word */
#define ioFDirIndex     28      /* word */
#define ioFlAttrib      30      /* byte */
#define ioFlFndrInfo    32      /* 16 bytes: FInfo */
#define ioFlNum         36      /* long: file number / dirID for dir */
#define ioFlStBlk       36      /* word (reused offset) */
#define ioFlLgLen       40      /* long: data fork logical len */
#define ioFlPyLen       44      /* long: data fork physical len */
#define ioFlRStBlk      48      /* word */
#define ioFlRLgLen      50      /* long: resource fork logical len */
#define ioFlRPyLen      54      /* long: resource fork physical len */
#define ioFlCrDat       58      /* long: creation date */
#define ioFlMdDat       62      /* long: modification date */
#define ioBuffer        32      /* long: buffer pointer (for Read) */
#define ioReqCount      36      /* long: requested byte count */
#define ioActCount      40      /* long: actual byte count */
#define ioPosMode       44      /* word: positioning mode */
#define ioPosOffset     46      /* long: positioning offset */

/* CInfoPBRec extra offsets */
#define ioDirID         48      /* long: directory ID */
#define ioDrNmFls       40      /* long: number of files in dir */
#define ioDrParID       100     /* long: parent directory ID */
#define ioDrCrDat       58      /* long */
#define ioDrMdDat       62      /* long */

/* VCB offsets */
#define vcbFlags        0       /* word: qType */
#define vcbQLink        2       /* long */
#define vcbVSize        6       /* word: qType for enqueue */
#define vcbVN           7       /* 28 bytes: Pascal string volume name */
#define vcbVRefNum      70      /* word */
#define vcbDrvNum       72      /* word */
#define vcbDRefNum      74      /* word */
#define vcbAtrb         76      /* word */
#define vcbNmFls        78      /* word */
#define vcbCrDate       82      /* long */
#define vcbLsMod        86      /* long */
#define vcbFreeBks      100     /* word */
#define vcbAlBlkSiz     96      /* long */
#define vcbNmAlBlks     94      /* word */
#define vcbClpSiz       98      /* long */

/* ---- extension discovery ---- */

typedef struct {
	unsigned long zeroes[4];
	unsigned long checkval;
	unsigned long pokeaddr;
} MyDriverDat_R;

static char *find_reg_base(void)
{
	MyDriverDat_R *sv;

	sv = *(MyDriverDat_R **)kSonyVarsPtr;
	if (sv == NULL)
		return NULL;
	if (sv->zeroes[0] != 0 || sv->zeroes[1] != 0
		|| sv->zeroes[2] != 0)
		return NULL;
	if (sv->checkval != kCheckVal)
		return NULL;
	if (sv->pokeaddr == 0)
		return NULL;
	return (char *)(sv->pokeaddr + 0x20);
}

/* ---- register access helpers ---- */

#define REG_COMMAND  0x00
#define REG_RESULT   0x02
#define REG_P(n)     (0x04 + (n) * 4)

static void reg_set(char *base, int n, unsigned long v)
{
	*(unsigned long *)(base + REG_P(n)) = v;
}

static unsigned long reg_get(char *base, int n)
{
	return *(unsigned long *)(base + REG_P(n));
}

static void reg_command(char *base, unsigned short cmd)
{
	*(unsigned short *)(base + REG_COMMAND) = cmd;
}

static unsigned short reg_result(char *base)
{
	return *(unsigned short *)(base + REG_RESULT);
}

/* ---- debug log ---- */

static void dbg_log6(char *base, char *fmt,
	unsigned long v0, unsigned long v1, unsigned long v2,
	unsigned long v3, unsigned long v4, unsigned long v5)
{
	reg_set(base, 0, (unsigned long)fmt);
	reg_set(base, 1, v0);
	reg_set(base, 2, v1);
	reg_set(base, 3, v2);
	reg_set(base, 4, v3);
	reg_set(base, 5, v4);
	reg_set(base, 6, v5);
	reg_command(base, 0x020D);
}

#define dbg_log(b,s)              dbg_log6(b,s,0,0,0,0,0,0)
#define dbg_log1(b,s,a)           dbg_log6(b,s,(long)(a),0,0,0,0,0)
#define dbg_log2(b,s,a,c)         dbg_log6(b,s,(long)(a),(long)(c),0,0,0,0)
#define dbg_log3(b,s,a,c,d)       dbg_log6(b,s,(long)(a),(long)(c),(long)(d),0,0,0)

/* ---- globals (allocated in system heap) ---- */

typedef struct {
	char        *regBase;
	long         oldHFSDispatch;
	long         oldOpen;
	long         oldClose;
	long         oldRead;
	long         oldWrite;
	long         oldGetVolInfo;
	long         oldCreate;
	long         oldDelete;
	long         oldOpenRF;
	long         oldRename;
	long         oldGetFileInfo;
	long         oldSetFileInfo;
	long         oldUnmountVol;
	long         oldAllocate;
	long         oldGetEOF;
	long         oldSetEOF;
	long         oldFlushVol;
	long         oldGetVol;
	long         oldSetVol;
	long         oldEject;
	long         oldGetFPos;
	long         oldSetFPos;
	Ptr          vcb;
	unsigned long volFileCount;
	unsigned long volTotalBytes;
} Globals;

#define kGlobalsPtr     0x0B04  /* scratch area, distinct from ClipSync */

/* ---- FCB management ---- */

/*
	Allocate a free FCB slot. Returns the ioRefNum (byte offset
	from start of FCB buffer + 2), or 0 on failure.
*/
static short AllocFCB(Ptr vcb, unsigned long cnid,
	unsigned long eof, unsigned char forkType)
{
	Ptr fcbBuf;
	short fcbLen;
	short i;
	Ptr fcb;

	fcbBuf = *(Ptr *)kFCBSPtr;
	if (fcbBuf == NULL)
		return 0;

	/* First word of FCB buffer is the total buffer length */
	fcbLen = *(short *)fcbBuf;

	for (i = 2; i < fcbLen; i += kFCBLen) {
		fcb = fcbBuf + i;
		if (*(long *)(fcb + kFCBFlNum) == 0) {
			/* Free slot */
			*(long *)(fcb + kFCBFlNum) = cnid;
			*(char *)(fcb + kFCBFlags) = 0; /* read-only */
			*(char *)(fcb + kFCBTypByt) = forkType;
			*(long *)(fcb + kFCBEOF) = eof;
			*(long *)(fcb + kFCBPLen) = eof;
			*(long *)(fcb + kFCBCrPs) = 0;
			*(long *)(fcb + kFCBVPtr) = (long)vcb;
			return i; /* this IS the ioRefNum */
		}
	}
	return 0; /* no free slot */
}

static void FreeFCB(short refNum)
{
	Ptr fcbBuf;
	Ptr fcb;

	fcbBuf = *(Ptr *)kFCBSPtr;
	if (fcbBuf == NULL)
		return;
	fcb = fcbBuf + refNum;
	*(long *)(fcb + kFCBFlNum) = 0; /* mark free */
}

static Ptr GetFCB(short refNum)
{
	Ptr fcbBuf;
	fcbBuf = *(Ptr *)kFCBSPtr;
	if (fcbBuf == NULL)
		return NULL;
	return fcbBuf + refNum;
}

/* Check if an FCB belongs to our volume */
static Boolean IsOurFCB(short refNum)
{
	Ptr fcb;
	Globals *g;

	g = *(Globals **)kGlobalsPtr;
	if (g == NULL)
		return false;

	fcb = GetFCB(refNum);
	if (fcb == NULL)
		return false;

	return (*(long *)(fcb + kFCBVPtr) == (long)(g->vcb));
}

/* ---- Volume ownership check ---- */

static Boolean IsOurVolume(short vRefNum)
{
	Globals *g = *(Globals **)kGlobalsPtr;
	if (g == NULL)
		return false;
	if (vRefNum == kOurVRefNum)
		return true;
	/* TODO: check WD references */
	return false;
}

/* ---- Name buffer in system heap for host communication ---- */

static char s_nameBuf[64]; /* Pascal string buffer, 32 bytes enough */

/* ---- Trap patch implementations ---- */

/*
	HFSDispatch patch.
	Called with A0 = ParamBlockRec, D0 = selector.
	We need to check the selector, and for relevant ones
	check if the vRefNum is ours.
*/

static pascal OSErr MyGetCatInfo(void *pb, char *regBase, Ptr vcb)
{
	short vRefNum = *(short *)((char *)pb + ioVRefNum);
	long dirID = *(long *)((char *)pb + ioDirID);
	short index = *(short *)((char *)pb + ioFDirIndex);
	unsigned long nameAddr = *(unsigned long *)((char *)pb + ioNamePtr);
	unsigned long cnid, flags, sizeOrCount, parentID;
	unsigned long type, creator, crDate, modDate;

	if (index > 0) {
		/* Indexed enumeration */
		reg_set(regBase, 0, dirID);
		reg_set(regBase, 1, (unsigned long)index);
		reg_set(regBase, 2, (unsigned long)s_nameBuf);
		reg_command(regBase, 0x0202);
		if (reg_result(regBase) != 0)
			return -43; /* fnfErr */
	} else if (index == 0) {
		/* Get info about the directory itself */
		reg_set(regBase, 0, dirID);
		reg_set(regBase, 1, 0);
		reg_set(regBase, 2, (unsigned long)s_nameBuf);
		reg_command(regBase, 0x0202);
		if (reg_result(regBase) != 0)
			return -43;
	} else {
		/* index < 0: get by name */
		if (nameAddr == 0)
			return -50; /* paramErr */
		reg_set(regBase, 0, dirID);
		reg_set(regBase, 1, nameAddr);
		reg_set(regBase, 2, (unsigned long)s_nameBuf);
		reg_command(regBase, 0x0203);
		if (reg_result(regBase) != 0)
			return -43;
	}

	cnid = reg_get(regBase, 0);
	flags = reg_get(regBase, 1);
	sizeOrCount = reg_get(regBase, 2);
	parentID = reg_get(regBase, 3);

	/* Get dates and type/creator via GetFileInfo */
	reg_set(regBase, 0, cnid);
	reg_command(regBase, 0x0207);
	type = reg_get(regBase, 0);
	creator = reg_get(regBase, 1);
	crDate = reg_get(regBase, 2);
	modDate = reg_get(regBase, 3);

	/* Copy name back to caller's name buffer */
	if (nameAddr != 0) {
		unsigned char len = (unsigned char)s_nameBuf[0];
		short i;
		*(unsigned char *)nameAddr = len;
		for (i = 0; i < len; i++)
			((char *)nameAddr)[1 + i] = s_nameBuf[1 + i];
	}

	if (flags & kDirMask) {
		/* Directory */
		*(unsigned char *)((char *)pb + ioFlAttrib) = kDirMask;
		*(long *)((char *)pb + ioDrNmFls) = sizeOrCount;
		*(long *)((char *)pb + ioDirID) = cnid;
		*(long *)((char *)pb + ioDrParID) = parentID;
		*(long *)((char *)pb + ioDrCrDat) = crDate;
		*(long *)((char *)pb + ioDrMdDat) = modDate;
	} else {
		/* File */
		*(unsigned char *)((char *)pb + ioFlAttrib) = 0x01; /* locked */
		/* FInfo: type + creator at ioFlFndrInfo */
		*(unsigned long *)((char *)pb + ioFlFndrInfo) = type;
		*(unsigned long *)((char *)pb + ioFlFndrInfo + 4) = creator;
		*(long *)((char *)pb + ioFlNum) = cnid;
		*(long *)((char *)pb + ioFlLgLen) = sizeOrCount;
		*(long *)((char *)pb + ioFlPyLen) = sizeOrCount;
		*(long *)((char *)pb + ioFlRLgLen) = 0;
		*(long *)((char *)pb + ioFlRPyLen) = 0;
		*(long *)((char *)pb + ioFlCrDat) = crDate;
		*(long *)((char *)pb + ioFlMdDat) = modDate;
		*(long *)((char *)pb + ioDirID) = parentID;
	}

	*(short *)((char *)pb + ioResult) = 0;
	return 0; /* noErr */
}

/* ---- Open (flat trap) ---- */

static pascal OSErr MyOpen(void *pb, char *regBase, Ptr vcb)
{
	unsigned long nameAddr = *(unsigned long *)((char *)pb + ioNamePtr);
	long dirID;
	unsigned long cnid, size;
	unsigned long handle;
	short refNum;

	/* Look up the file by name */
	dirID = kRootDirID; /* TODO: resolve WD for proper dirID */
	if (nameAddr == 0)
		return -50;

	reg_set(regBase, 0, dirID);
	reg_set(regBase, 1, nameAddr);
	reg_command(regBase, 0x0209); /* ObjByName */
	cnid = reg_get(regBase, 0);
	if (cnid == 0)
		return -43; /* fnfErr */

	/* Get file size */
	reg_set(regBase, 0, cnid);
	reg_command(regBase, 0x0207); /* GetFileInfo */
	size = reg_get(regBase, 2); /* crDate field has... wait, need actual size */

	/* Actually we need to ask GetCatInfo for the size */
	/* Use GetCatInfoByName which returns size in p2 */
	reg_set(regBase, 0, dirID);
	reg_set(regBase, 1, nameAddr);
	reg_set(regBase, 2, 0);
	reg_command(regBase, 0x0203);
	if (reg_result(regBase) != 0)
		return -43;
	size = reg_get(regBase, 2); /* sizeOrCount for file */

	/* Open on host */
	reg_set(regBase, 0, cnid);
	reg_set(regBase, 1, 0); /* data fork */
	reg_command(regBase, 0x0204);
	if (reg_result(regBase) != 0)
		return -43;
	handle = reg_get(regBase, 0);

	/* Allocate FCB */
	refNum = AllocFCB(vcb, cnid, size, 0);
	if (refNum == 0) {
		/* Close on host */
		reg_set(regBase, 0, handle);
		reg_command(regBase, 0x0206);
		return -42; /* tmfoErr */
	}

	/* Store handle in a place we can find it — use the CNID
	   field (it's already stored as cnid). We'll use cdFlNum's
	   spot cleverly... actually let's use the FCB's physical EOF
	   as handle storage since we don't need it. */
	/* Better: store handle = FCB.PLen (we set PLen = handle) */
	{
		Ptr fcb = GetFCB(refNum);
		*(long *)(fcb + kFCBPLen) = handle;
	}

	*(short *)((char *)pb + ioRefNum) = refNum;
	*(short *)((char *)pb + ioResult) = 0;
	return 0;
}

/* ---- Read (flat trap) ---- */

static pascal OSErr MyRead(void *pb, char *regBase)
{
	short refNum = *(short *)((char *)pb + ioRefNum);
	unsigned long buffer = *(unsigned long *)((char *)pb + ioBuffer);
	long reqCount = *(long *)((char *)pb + ioReqCount);
	short posMode = *(short *)((char *)pb + ioPosMode);
	long posOffset = *(long *)((char *)pb + ioPosOffset);
	Ptr fcb;
	long mark, eof, handle;
	unsigned long actual;

	fcb = GetFCB(refNum);
	if (fcb == NULL)
		return -43;

	mark = *(long *)(fcb + kFCBCrPs);
	eof = *(long *)(fcb + kFCBEOF);
	handle = *(long *)(fcb + kFCBPLen); /* our host handle */

	/* Apply positioning */
	switch (posMode & 0x03) {
		case 0: /* fsAtMark */
			break;
		case 1: /* fsFromStart */
			mark = posOffset;
			break;
		case 2: /* fsFromLEOF */
			mark = eof + posOffset;
			break;
		case 3: /* fsFromMark */
			mark += posOffset;
			break;
	}

	if (mark < 0)
		mark = 0;
	if (mark > eof)
		mark = eof;

	if (reqCount <= 0) {
		*(long *)((char *)pb + ioActCount) = 0;
		*(long *)((char *)pb + ioPosOffset) = mark;
		*(long *)(fcb + kFCBCrPs) = mark;
		return 0;
	}

	/* Clamp to EOF */
	if (mark + reqCount > eof)
		reqCount = eof - mark;

	/* Read from host */
	reg_set(regBase, 0, handle);
	reg_set(regBase, 1, (unsigned long)mark);
	reg_set(regBase, 2, (unsigned long)reqCount);
	reg_set(regBase, 3, buffer);
	reg_command(regBase, 0x0205);
	actual = reg_get(regBase, 0);

	mark += actual;
	*(long *)(fcb + kFCBCrPs) = mark;

	*(long *)((char *)pb + ioActCount) = actual;
	*(long *)((char *)pb + ioPosOffset) = mark;
	*(short *)((char *)pb + ioResult) = 0;

	if (actual < (unsigned long)reqCount)
		return -39; /* eofErr */
	return 0;
}

/* ---- Close (flat trap) ---- */

static pascal OSErr MyClose(void *pb, char *regBase)
{
	short refNum = *(short *)((char *)pb + ioRefNum);
	Ptr fcb;
	long handle;

	fcb = GetFCB(refNum);
	if (fcb == NULL)
		return -43;

	handle = *(long *)(fcb + kFCBPLen);

	/* Close on host */
	reg_set(regBase, 0, (unsigned long)handle);
	reg_command(regBase, 0x0206);

	FreeFCB(refNum);
	return 0;
}

/* ---- GetFileInfo (flat trap) ---- */

static pascal OSErr MyGetFileInfo(void *pb, char *regBase)
{
	unsigned long nameAddr = *(unsigned long *)((char *)pb + ioNamePtr);
	long dirID = kRootDirID;
	unsigned long cnid, flags, size, parentID;
	unsigned long type, creator, crDate, modDate;

	if (nameAddr == 0)
		return -50;

	/* Look up by name in root (simplified) */
	reg_set(regBase, 0, dirID);
	reg_set(regBase, 1, nameAddr);
	reg_set(regBase, 2, (unsigned long)s_nameBuf);
	reg_command(regBase, 0x0203);
	if (reg_result(regBase) != 0)
		return -43;

	cnid = reg_get(regBase, 0);
	flags = reg_get(regBase, 1);
	size = reg_get(regBase, 2);
	parentID = reg_get(regBase, 3);

	reg_set(regBase, 0, cnid);
	reg_command(regBase, 0x0207);
	type = reg_get(regBase, 0);
	creator = reg_get(regBase, 1);
	crDate = reg_get(regBase, 2);
	modDate = reg_get(regBase, 3);

	*(unsigned char *)((char *)pb + ioFlAttrib) = 0x01; /* locked */
	*(unsigned long *)((char *)pb + ioFlFndrInfo) = type;
	*(unsigned long *)((char *)pb + ioFlFndrInfo + 4) = creator;
	*(long *)((char *)pb + ioFlNum) = cnid;
	*(long *)((char *)pb + ioFlLgLen) = size;
	*(long *)((char *)pb + ioFlPyLen) = size;
	*(long *)((char *)pb + ioFlRLgLen) = 0;
	*(long *)((char *)pb + ioFlRPyLen) = 0;
	*(long *)((char *)pb + ioFlCrDat) = crDate;
	*(long *)((char *)pb + ioFlMdDat) = modDate;

	return 0;
}

/* ---- GetEOF / GetFPos / SetFPos ---- */

static pascal OSErr MyGetEOF(void *pb)
{
	short refNum = *(short *)((char *)pb + ioRefNum);
	Ptr fcb = GetFCB(refNum);
	if (fcb == NULL)
		return -43;
	*(long *)((char *)pb + 28) = *(long *)(fcb + kFCBEOF);
	return 0;
}

static pascal OSErr MyGetFPos(void *pb)
{
	short refNum = *(short *)((char *)pb + ioRefNum);
	Ptr fcb = GetFCB(refNum);
	if (fcb == NULL)
		return -43;
	*(long *)((char *)pb + ioPosOffset) = *(long *)(fcb + kFCBCrPs);
	*(long *)((char *)pb + ioReqCount) = 0;
	*(long *)((char *)pb + ioActCount) = 0;
	return 0;
}

static pascal OSErr MySetFPos(void *pb)
{
	short refNum = *(short *)((char *)pb + ioRefNum);
	short posMode = *(short *)((char *)pb + ioPosMode);
	long posOffset = *(long *)((char *)pb + ioPosOffset);
	Ptr fcb;
	long mark, eof;

	fcb = GetFCB(refNum);
	if (fcb == NULL)
		return -43;

	mark = *(long *)(fcb + kFCBCrPs);
	eof = *(long *)(fcb + kFCBEOF);

	switch (posMode & 0x03) {
		case 1: mark = posOffset; break;
		case 2: mark = eof + posOffset; break;
		case 3: mark += posOffset; break;
	}

	if (mark < 0) mark = 0;
	if (mark > eof) mark = eof;

	*(long *)(fcb + kFCBCrPs) = mark;
	*(long *)((char *)pb + ioPosOffset) = mark;
	return 0;
}

/* ---- GetVolInfo ---- */

static pascal OSErr MyGetVolInfo(void *pb, Globals *g)
{
	unsigned long nameAddr;

	nameAddr = *(unsigned long *)((char *)pb + ioNamePtr);
	if (nameAddr != 0) {
		unsigned char *p = (unsigned char *)nameAddr;
		p[0] = 6;
		p[1] = 'S'; p[2] = 'h'; p[3] = 'a';
		p[4] = 'r'; p[5] = 'e'; p[6] = 'd';
	}

	*(short *)((char *)pb + ioVRefNum) = kOurVRefNum;
	*(long *)((char *)pb + ioFlCrDat) = 0;
	*(long *)((char *)pb + ioFlMdDat) = 0;
	/* ioVAtrb — volume attributes (locked) */
	*(short *)((char *)pb + 30) = 0x8000;
	/* ioVNmFls — file count */
	*(short *)((char *)pb + 40) = (short)g->volFileCount;
	/* ioVAlBlkSiz */
	*(long *)((char *)pb + 44) = 512;
	/* ioVNmAlBlks */
	*(short *)((char *)pb + 42) = 1024;
	/* ioVFreeBks */
	*(short *)((char *)pb + 50) = 0;

	return 0;
}

/* ──────────────────────────────────────────────── */
/*                 INIT entry point                 */
/* ──────────────────────────────────────────────── */

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

	/* Find extension register base */
	regBase = find_reg_base();
	if (regBase == NULL)
		goto bail;

	dbg_log1(regBase, "SharedDrive INIT: regBase=%lx",
		(unsigned long)regBase);

	/* Check version */
	reg_command(regBase, 0x0200);
	version = reg_get(regBase, 0);
	if (version < 1) {
		dbg_log(regBase, "SharedDrive INIT: no shared/ dir or version 0");
		goto bail;
	}

	dbg_log1(regBase, "SharedDrive INIT: version=%ld", version);

	/* Get volume stats */
	reg_command(regBase, 0x0201);

	/* Allocate globals in system heap */
	g = (Globals *)NewPtrSysClear(sizeof(Globals));
	if (g == NULL) {
		dbg_log(regBase, "SharedDrive INIT: NewPtrSys failed!");
		goto bail;
	}

	g->regBase = regBase;
	g->volFileCount = reg_get(regBase, 0);
	g->volTotalBytes = reg_get(regBase, 1);
	*(Globals **)kGlobalsPtr = g;

	dbg_log2(regBase, "SharedDrive INIT: files=%ld bytes=%ld",
		g->volFileCount, g->volTotalBytes);

	/* Keep our code resource in memory */
	self = GetResource('INIT', 315);
	if (self != NULL) {
		DetachResource(self);
		HLock(self);
		HNoPurge(self);
	} else {
		dbg_log(regBase, "SharedDrive INIT: WARNING no handle!");
	}

	/*
		We now have the host ready. The proper approach is to
		patch all the traps. However, this is THINK C code running
		as an INIT — we need assembly stubs for each trap patch
		because the calling convention is register-based (A0=pb).

		For now, we simply register the drive and VCB so the volume
		appears on the desktop, and log success. The trap patches
		will be added incrementally — they require inline assembly
		wrappers since each trap receives the param block in A0.
	*/

	/* TODO: The actual trap patching requires assembly stubs
	   that save/restore registers and dispatch to our C handlers.
	   This will be implemented next. For now, just mount the volume
	   icon. */

	/* Allocate and fill VCB */
	g->vcb = NewPtrSysClear(178); /* VCB size on System 6 */
	if (g->vcb == NULL) {
		dbg_log(regBase, "SharedDrive INIT: VCB alloc failed!");
		goto bail;
	}

	{
		Ptr v = g->vcb;
		unsigned long now;

		GetDateTime(&now);

		/* Volume name */
		v[vcbVN] = 6; /* length byte */
		v[vcbVN+1] = 'S';
		v[vcbVN+2] = 'h';
		v[vcbVN+3] = 'a';
		v[vcbVN+4] = 'r';
		v[vcbVN+5] = 'e';
		v[vcbVN+6] = 'd';

		*(short *)(v + vcbVRefNum) = kOurVRefNum;
		*(short *)(v + vcbDrvNum) = kOurDriveNum;
		*(short *)(v + vcbDRefNum) = kOurDrvrRefNum;
		*(short *)(v + vcbAtrb) = (short)0x8080; /* locked, read-only */
		*(short *)(v + vcbNmFls) = (short)g->volFileCount;
		*(long *)(v + vcbCrDate) = now;
		*(long *)(v + vcbLsMod) = now;
		*(short *)(v + vcbNmAlBlks) = 1024;
		*(long *)(v + vcbAlBlkSiz) = 512;
		*(long *)(v + vcbClpSiz) = 512;
		*(short *)(v + vcbFreeBks) = 0;
	}

	/* Link VCB into the system VCB queue */
	Enqueue((QElemPtr)g->vcb, (QHdrPtr)kVCBQHdr);

	dbg_log1(regBase, "SharedDrive INIT: VCB at %lx",
		(unsigned long)g->vcb);

	/* Add drive queue entry */
	/* AddDrive is problematic from C — we just post the event
	   and let the system discover our VCB. The drive queue entry
	   is needed for proper integration but we can start without it
	   and see what happens. */

	/* Post disk-inserted event so Finder discovers the volume */
	PostEvent(7, (long)kOurDriveNum);

	dbg_log(regBase, "SharedDrive INIT: volume posted, done!");

bail:
	RestoreA4();
}
