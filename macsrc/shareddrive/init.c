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
	  $204 ExtFSOpen      p0=CNID, p1=fork -> p0=handle, p1=fileSize
	  $205 ExtFSRead      p0=handle, p1=offset, p2=count, p3=bufAddr
	  $206 ExtFSClose     p0=handle
	  $207 ExtFSGetFileInfo p0=CNID -> p0=type, p1=creator, p2=crDate, p3=modDate
	  $208 ExtFSReadDir   p0=dirID -> p0=count
	  $209 ExtFSObjByName p0=parentDirID, p1=namePtr -> p0=CNID
	  $20A ExtFSGetWDInfo p0=wdRefNum -> p0=vRefNum, p1=dirID
	  $20B ExtFSOpenWD    p0=vRefNum, p1=dirID -> p0=wdRefNum
	  $20C ExtFSCloseWD   p0=wdRefNum
	  $20D ExtFSDbgLog    p0=fmt string addr, p1-p6=args
	  $210 ExtFSCreateFile p0=parentDirID, p1=namePtr -> p0=CNID
	  $211 ExtFSWrite     p0=handle, p1=offset, p2=count, p3=bufAddr
	  $212 ExtFSDeleteFile p0=parentDirID, p1=namePtr
	  $213 ExtFSSetFileInfo p0=CNID, p1=type, p2=creator, p3=flags
	  $214 ExtFSFatal     p0=fmt string addr, p1-p6=args  (shuts down emulator)
	  $219 ExtFSGetDirInfo p0=CNID, p1=bufAddr -> 32 bytes DInfo+DXInfo
	  $21A ExtFSSetDirInfo p0=CNID, p1=bufAddr (32 bytes DInfo+DXInfo)

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

/* Virtual volume geometry — report ~1 GB capacity so copy operations
   don't fail with "disk full".  The host filesystem provides the
   actual storage; these numbers are only what we tell the Finder. */
#define kAllocBlkSize   32768L   /* 32 KB allocation blocks */
#define kTotalAllocBlks 32000    /* 32000 * 32KB ≈ 1 GB */

/* VCB field offsets (from Inside Macintosh IV) */
#define kVcbNxtCNID     38       /* LONGINT — next unused CNID */

/* Low-memory globals */
#define kSonyVarsPtr    0x0134
#define kCheckVal       0x841339E2UL
#define kVCBQHdr        0x0356
#define kDrvQHdr        0x0308
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
/* HFS-specific fields (from FSEqu.a in SuperMario sources) */
#define kFCBClmpSize    30   /* LONGINT — clump size */
#define kFCBBTCBPtr     34   /* LONGINT — B*-Tree control block ptr */
/* We repurpose kFCBBTCBPtr to store the host file handle.
   kFCBPLen must hold the real physical EOF so the ROM
   Resource Manager doesn't think the file is truncated. */
#define kFCBHostHandle  kFCBBTCBPtr
#define kFCBExtRec      38   /* 12 bytes — first 3 file extents */
#define kFCBFType       50   /* LONGINT — file's 4 Finder type bytes */
#define kFCBCatPos      54   /* LONGINT — catalog hint for Close */
#define kFCBDirID       58   /* LONGINT — parent directory ID */
#define kFCBCName       62   /* 32 bytes — file name (Pascal string) */

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

/* HFS-extended fileParam / CInfoPBRec offsets (Inside Mac IV-109) */
#define pb_ioFlBkDat     80   /* LONGINT — backup date */
#define pb_ioFlXFndrInfo 84   /* FXInfo, 16 bytes */
#define pb_ioFlClpSiz    104  /* LONGINT — clump size */

/* HFS-extended dirInfo offsets */
#define pb_ioDrBkDat     80   /* LONGINT — backup date */
#define pb_ioDrFndrInfo  84   /* DXInfo, 16 bytes */

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

/* FCBPBRec variant (for PBGetFCBInfo) — TN087 corrected offsets */
#define pb_ioFCBIndx    28   /* INTEGER (TN087: not LONGINT) */
#define pb_ioFCBFlNm    32   /* LONGINT — file number */
#define pb_ioFCBFlags   36   /* INTEGER — FCB flags */
#define pb_ioFCBStBlk   38   /* INTEGER — first alloc block */
#define pb_ioFCBEOF     40   /* LONGINT — logical EOF */
#define pb_ioFCBPLen    44   /* LONGINT — physical EOF */
#define pb_ioFCBCrPs    48   /* LONGINT — mark */
#define pb_ioFCBVRefNum 52   /* INTEGER — volume refnum */
#define pb_ioFCBClpSiz  54   /* LONGINT — clump size */
#define pb_ioFCBParID   58   /* LONGINT — parent dir ID */

/* WDParam variant (for _OpenWD, _CloseWD, _GetWDInfo) */
#define pb_ioWDIndex    26   /* INTEGER */
#define pb_ioWDProcID   28   /* LONGINT */
#define pb_ioWDVRefNum  32   /* INTEGER */
#define pb_ioWDDirID    48   /* LONGINT */

/* ioParam: permission byte (for _Open, _OpenRF) */
#define pb_ioPermssn    27   /* SignedByte */

/* Permission constants (IM IV) */
#define kFsCurPerm   0   /* whatever is allowed */
#define kFsRdPerm    1   /* read only */
#define kFsWrPerm    2   /* write permission */
#define kFsRdWrPerm  3   /* exclusive read/write */

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

/* Mac OS error codes */
#define kNoErr       0
#define kIoErr     (-36)
#define kEofErr    (-39)
#define kPosErr    (-40)
#define kTmfoErr   (-42)
#define kFnfErr    (-43)
#define kWPrErr    (-44)
#define kOpWrErr   (-49)
#define kParamErr  (-50)
#define kRfNumErr  (-51)
#define kWrPermErr (-61)
#define kNsvErr    (-35)

/* Host register-block command numbers (coarse, Phase 1) */
#define kCmdResolveAndOpen     0x0223
#define kCmdGetCatInfoResolved 0x0224
#define kCmdGetFileInfoByName  0x0222
#define kCmdFileOpByName       0x0225
#define kCmdClose              0x0206
#define kCmdRead               0x0205
#define kCmdWrite              0x0211
#define kCmdSetEOF             0x0218
#define kCmdGetVol             0x0201
#define kCmdOpenWD             0x020B
#define kCmdCloseWD            0x020C
#define kCmdGetWDInfo          0x020A
#define kCmdCreateDir          0x0215
#define kCmdCatMove            0x0216
#define kCmdGetDirInfo         0x0219
#define kCmdSetDirInfo         0x021A

/* PB-based command codes */
#define kPB_GetCatInfo         0x0230
#define kPB_GetFileInfo        0x0231
#define kPB_Open               0x0232
#define kPB_OpenRF             0x0233

/* FileOpByName sub-opcodes */
#define kFileOpCreate      0
#define kFileOpDelete      1
#define kFileOpRename      2
#define kFileOpSetFileInfo 3
#define kFileOpSetCatInfo  4

/* ---- Globals ---- */

typedef struct {
	char    *regBase;
	Ptr      vcb;
	Ptr      dqe;            /* drive queue element block (4 flag bytes + DrvQEl) */
	long     volFileCount;
	long     volTotalBytes;
	long     savedA4;        /* THINK C code resource A4 */
	short    rootWDRefNum;   /* permanent WD for root dir, created at init */
	short    defaultWDRefNum; /* WD refnum from last SetVol */
	short    ejected;        /* nonzero after _Eject */
} Globals;

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

/* ---- Guest globals via host storage ---- */

static Globals *get_globals(void)
{
	char *base = find_reg_base();
	if (base == NULL) return NULL;
	reg_set(base, 0, 0);
	reg_set(base, 1, 0);  /* GET */
	reg_command(base, 0x020E);
	return (Globals *)reg_get(base, 0);
}

static void set_globals(char *base, Globals *g)
{
	reg_set(base, 0, (unsigned long)g);
	reg_set(base, 1, 1);  /* SET */
	reg_command(base, 0x020E);
}

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

/* ---- Structured trap logging (host-side pretty-print) ---- */

#define LOG_PASSTHRU  0
#define LOG_HANDLED   1
#define LOG_ERROR     2

#define LOG_F_HFS     0x0001
#define LOG_F_PBMOD   0x0002

static void log_trap(char *base, unsigned short trapWord,
	char *pb, short action, short err, short flags)
{
	reg_set(base, 0, (unsigned long)trapWord);
	reg_set(base, 1, (unsigned long)pb);
	reg_set(base, 2, (unsigned long)action);
	reg_set(base, 3, (unsigned long)(short)err);
	reg_set(base, 4, (unsigned long)flags);
	reg_command(base, 0x020F);
}

/* ---- Fatal shutdown ---- */

static void dbg_fatal6(char *base, char *fmt,
	unsigned long a,unsigned long b,unsigned long c,
	unsigned long d,unsigned long e,unsigned long f)
{
	reg_set(base,0,(unsigned long)fmt);
	reg_set(base,1,a); reg_set(base,2,b);
	reg_set(base,3,c); reg_set(base,4,d);
	reg_set(base,5,e); reg_set(base,6,f);
	reg_command(base, 0x0214);
}
#define dbg_fatal(b,s)          dbg_fatal6(b,s,0,0,0,0,0,0)
#define dbg_fatal1(b,s,a)       dbg_fatal6(b,s,(long)(a),0,0,0,0,0)
#define dbg_fatal2(b,s,a,c)     dbg_fatal6(b,s,(long)(a),(long)(c),0,0,0,0)

/* ---- Hex dump ---- */

static char s_hexChars[] = "0123456789ABCDEF";
static char s_hexLine[80];

static void dbg_hexdump(char *regBase, char *label,
	unsigned char *addr, short len)
{
	short i, col;
	char *p;

	dbg_log2(regBase, "DUMP %s at %lx:",
		(unsigned long)label, (unsigned long)addr);

	for (i = 0; i < len; i += 16) {
		p = s_hexLine;
		*p++ = s_hexChars[(i >> 12) & 0xF];
		*p++ = s_hexChars[(i >> 8) & 0xF];
		*p++ = s_hexChars[(i >> 4) & 0xF];
		*p++ = s_hexChars[i & 0xF];
		*p++ = ':';
		*p++ = ' ';
		for (col = 0; col < 16 && (i + col) < len; col++) {
			unsigned char b = addr[i + col];
			*p++ = s_hexChars[b >> 4];
			*p++ = s_hexChars[b & 0xF];
			*p++ = ' ';
		}
		for (; col < 16; col++) {
			*p++ = ' '; *p++ = ' '; *p++ = ' ';
		}
		*p++ = ' ';
		for (col = 0; col < 16 && (i + col) < len; col++) {
			unsigned char b = addr[i + col];
			*p++ = (b >= 0x20 && b < 0x7F) ? b : '.';
		}
		*p = 0;
		dbg_log1(regBase, " %s", (unsigned long)s_hexLine);
	}
}

/* ---- Name buffer ---- */

static char s_nameBuf[64];
static char s_dirInfoBuf[32];  /* DInfo (16) + DXInfo (16) transfer buffer */

/* ---- Helpers ---- */

static void pstr_copy(char *dst, char *src)
{
	short i, len = (unsigned char)src[0];
	for (i = 0; i <= len; i++) dst[i] = src[i];
}

static void pstr_copy_max(char *dst, char *src, short maxLen)
{
	short len = (unsigned char)src[0];
	if (len > maxLen) len = maxLen;
	dst[0] = len;
	{ short i; for (i = 0; i < len; i++) dst[1+i] = src[1+i]; }
}

static void mem_zero(char *dst, short len)
{
	short i; for (i = 0; i < len; i++) dst[i] = 0;
}

static void mem_copy(char *dst, char *src, short len)
{
	short i; for (i = 0; i < len; i++) dst[i] = src[i];
}

static OSErr host_err(char *base)
{
	unsigned short r = reg_result(base);
	return (r == 0) ? kNoErr : -(short)r;
}

/* ---- Parameter extraction ---- */

typedef struct {
	short    vRefNum;
	long     dirID;
	unsigned long nameAddr;
} TrapLocation;

static TrapLocation ExtractLocation(char *pb, short isHFS, Globals *g)
{
	TrapLocation loc;
	loc.vRefNum  = *(short *)(pb + pb_ioVRefNum);
	loc.nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	if (isHFS) {
		loc.dirID = *(long *)(pb + 48);
	} else {
		loc.dirID = 0;
	}
	/* vRefNum 0 means "default volume".  Substitute the current default
	   WD refnum so the host resolveDir() can decode the directory from
	   the WD table.  Flat (non-HFS) calls never carry a dirID, so this
	   is the only way to convey directory context for them. */
	if (loc.vRefNum == 0)
		loc.vRefNum = g->defaultWDRefNum;
	/* Apps (THINK C) get ioWDVRefNum from GetWDInfo, which per Inside
	   Macintosh is always the true volume reference number (-32000).
	   On real HFS the volume ref happens to also be a WD refnum for
	   root, so Open(volRef, dirID=0) resolves through the WD table.
	   Ours doesn't.  When an app passes the raw volume ref or the
	   root WD with dirID=0, redirect to defaultWDRefNum which carries
	   the current subdirectory context via the WD table. */
	if (loc.dirID == 0
			&& (loc.vRefNum == kOurVRefNum
				|| loc.vRefNum == g->rootWDRefNum)
			&& g->defaultWDRefNum != g->rootWDRefNum)
		loc.vRefNum = g->defaultWDRefNum;
	return loc;
}

/* ---- FCB management ---- */

static short AllocFCB(Ptr vcb, unsigned long cnid,
	unsigned long eof, unsigned char flags)
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
	Globals *g = get_globals();
	Ptr fcb;
	if (g == NULL) return false;
	fcb = GetFCB(refNum);
	if (fcb == NULL) return false;
	return (*(long *)(fcb + kFCBVPtr) == (long)(g->vcb));
}

static Boolean IsOurVolume(short vRefNum)
{
	if (vRefNum == kOurVRefNum) return true;
	if (vRefNum == kOurDriveNum) return true;
	/* WD refnums we issue are encoded as -(wdRef+32000).
	   The host assigns wdRef starting from 1, so valid
	   encoded values are -32001, -32002, ...
	   Real File Manager WDCBs use small negative numbers (>= -32767)
	   that won't overlap with our range below -32000. */
	if (vRefNum < kOurVRefNum && vRefNum > -32100) return true;
	/* vRefNum 0 means "default volume". Check if the default VCB
	   is ours by reading the DefVCBPtr low-memory global. */
	if (vRefNum == 0) {
		Globals *g = get_globals();
		if (g != NULL && *(Ptr *)kDefVCBPtr == g->vcb)
			return true;
	}
	return false;
}

/* ================================================================ */
/*                 New trap handlers (Phase 2)                      */
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

	mark   = *(long *)(fcb + kFCBCrPs);
	eof    = *(long *)(fcb + kFCBEOF);
	handle = *(long *)(fcb + kFCBHostHandle);

	switch (posMode & 0x03) {
		case 1: mark = posOffset; break;
		case 2: mark = eof + posOffset; break;
		case 3: mark += posOffset; break;
	}
	if (mark < 0) {
		*(long *)(pb + pb_ioActCount)  = 0;
		*(long *)(pb + pb_ioPosOffset) = 0;
		*(long *)(fcb + kFCBCrPs) = 0;
		return kPosErr;
	}
	if (mark > eof) mark = eof;

	if (reqCount < 0) {
		*(long *)(pb + pb_ioActCount)  = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		*(long *)(fcb + kFCBCrPs) = mark;
		return kParamErr;
	}
	if (reqCount == 0) {
		*(long *)(pb + pb_ioActCount)  = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		*(long *)(fcb + kFCBCrPs) = mark;
		return kNoErr;
	}
	{
		short hitEof = 0;
		if (mark + reqCount > eof) {
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
		if (actual < (unsigned long)reqCount)
			return kEofErr;
		if (hitEof)
			return kEofErr;
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
	eof  = *(long *)(fcb + kFCBEOF);
	handle = *(long *)(fcb + kFCBHostHandle);

	switch (posMode & 0x03) {
		case 1: mark = posOffset; break;
		case 2: mark = eof + posOffset; break;
		case 3: mark += posOffset; break;
	}
	if (mark < 0) {
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = 0;
		return kPosErr;
	}

	if (reqCount < 0) {
		*(long *)(pb + pb_ioActCount) = 0;
		*(long *)(pb + pb_ioPosOffset) = mark;
		return kParamErr;
	}
	if (reqCount == 0) {
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
	if (mark > eof) {
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
	*(long *)(pb + pb_ioReqCount)  = 0;
	*(long *)(pb + pb_ioActCount)  = 0;
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
	eof  = *(long *)(fcb + kFCBEOF);
	switch (posMode & 0x03) {
		case 1: mark = posOffset; break;
		case 2: mark = eof + posOffset; break;
		case 3: mark += posOffset; break;
	}
	if (mark < 0) {
		*(long *)(fcb + kFCBCrPs) = 0;
		*(long *)(pb + pb_ioPosOffset) = 0;
		return kPosErr;
	}
	if (mark > eof) {
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
	reg_command(g->regBase, kPB_Open);
	if (reg_result(g->regBase) != 0) return host_err(g->regBase);

	{
		unsigned long handle = reg_get(g->regBase, 0);
		long size            = (long)reg_get(g->regBase, 1);
		unsigned long cnid   = reg_get(g->regBase, 2);
		unsigned char flags;

		/* Map permission to FCB flags */
		if (perm == kFsRdPerm)
			flags = 0x00;          /* read only */
		else
			flags = 0x01;          /* fcbWriteMask */

		{
			short refNum = AllocFCB(g->vcb, cnid, size, flags);
			if (refNum == 0) {
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
			long nextCNID = *(long *)(g->vcb + kVcbNxtCNID);
			if ((long)cnid >= nextCNID)
				*(long *)(g->vcb + kVcbNxtCNID) = (long)(cnid + 1);
		}
	}
	return kNoErr;
}

static OSErr TrapOpenRF(char *pb, Globals *g, short isHFS)
{
	unsigned char perm = *(unsigned char *)(pb + pb_ioPermssn);

	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_command(g->regBase, kPB_OpenRF);
	if (reg_result(g->regBase) != 0) return host_err(g->regBase);

	{
		unsigned long handle = reg_get(g->regBase, 0);
		long size            = (long)reg_get(g->regBase, 1);
		unsigned long cnid   = reg_get(g->regBase, 2);
		unsigned char flags;

		/* Map permission to FCB flags (resource fork bit = 0x02) */
		if (perm == kFsRdPerm)
			flags = 0x02;          /* resource fork, read only */
		else
			flags = 0x03;          /* resource fork + write */

		{
			short refNum = AllocFCB(g->vcb, cnid, size, flags);
			if (refNum == 0) {
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
			long nextCNID = *(long *)(g->vcb + kVcbNxtCNID);
			if ((long)cnid >= nextCNID)
				*(long *)(g->vcb + kVcbNxtCNID) = (long)(cnid + 1);
		}
	}
	return kNoErr;
}

static OSErr TrapGetFileInfo(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_command(g->regBase, kPB_GetFileInfo);
	return host_err(g->regBase);
}

static OSErr TrapSetFileInfo(char *pb, Globals *g, short isHFS)
{
	TrapLocation loc = ExtractLocation(pb, isHFS, g);
	if (loc.nameAddr == 0) return kParamErr;

	reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
	reg_set(g->regBase, 1, (unsigned long)loc.dirID);
	reg_set(g->regBase, 2, loc.nameAddr);
	reg_set(g->regBase, 3, kFileOpSetFileInfo);
	reg_set(g->regBase, 4, *(unsigned long *)(pb + pb_ioFlFndrInfo));
	reg_set(g->regBase, 5, *(unsigned long *)(pb + pb_ioFlFndrInfo + 4));
	reg_set(g->regBase, 6, (unsigned long)(unsigned short)
		*(short *)(pb + pb_ioFlFndrInfo + 8));
	reg_set(g->regBase, 7, *(unsigned long *)(pb + pb_ioFlFndrInfo + 10));
	reg_set(g->regBase, 8, (unsigned long)(unsigned short)
		*(short *)(pb + pb_ioFlFndrInfo + 14));
	reg_command(g->regBase, kCmdFileOpByName);
	return host_err(g->regBase);
}

static OSErr TrapGetCatInfo(char *pb, Globals *g, short isHFS)
{
	reg_set(g->regBase, 0, (unsigned long)pb);
	reg_command(g->regBase, kPB_GetCatInfo);
	return host_err(g->regBase);
}

static OSErr TrapSetCatInfo(char *pb, Globals *g, short isHFS)
{
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	short vRefNum = *(short *)(pb + pb_ioVRefNum);
	long dirID = *(long *)(pb + pb_ioDirID);

	if (nameAddr == 0) return kParamErr;

	/* FInfo/DInfo (16 bytes at offset 32) + FXInfo/DXInfo (16 bytes at
	   offset 84) sit at the same PB offsets for files and directories.
	   Pass all 32 bytes to host; it decides how to interpret them. */
	mem_copy(s_dirInfoBuf, pb + pb_ioFlFndrInfo, 16);
	mem_copy(s_dirInfoBuf + 16, pb + pb_ioFlXFndrInfo, 16);

	reg_set(g->regBase, 0, (unsigned long)(short)vRefNum);
	reg_set(g->regBase, 1, (unsigned long)dirID);
	reg_set(g->regBase, 2, nameAddr);
	reg_set(g->regBase, 3, kFileOpSetCatInfo);
	reg_set(g->regBase, 4, (unsigned long)s_dirInfoBuf);
	reg_command(g->regBase, kCmdFileOpByName);
	return host_err(g->regBase);
}

static OSErr TrapCreate(char *pb, Globals *g, short isHFS)
{
	TrapLocation loc = ExtractLocation(pb, isHFS, g);
	OSErr err;
	if (loc.nameAddr == 0) return kParamErr;
	reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
	reg_set(g->regBase, 1, (unsigned long)loc.dirID);
	reg_set(g->regBase, 2, loc.nameAddr);
	reg_set(g->regBase, 3, kFileOpCreate);
	reg_command(g->regBase, kCmdFileOpByName);
	err = host_err(g->regBase);
	if (err == kNoErr) {
		/* Host assigned a new CNID — bump vcbNxtCNID */
		long nextCNID = *(long *)(g->vcb + kVcbNxtCNID);
		*(long *)(g->vcb + kVcbNxtCNID) = nextCNID + 1;
	}
	return err;
}

static OSErr TrapDelete(char *pb, Globals *g, short isHFS)
{
	TrapLocation loc = ExtractLocation(pb, isHFS, g);
	if (loc.nameAddr == 0) return kParamErr;
	reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
	reg_set(g->regBase, 1, (unsigned long)loc.dirID);
	reg_set(g->regBase, 2, loc.nameAddr);
	reg_set(g->regBase, 3, kFileOpDelete);
	reg_command(g->regBase, kCmdFileOpByName);
	return host_err(g->regBase);
}

static OSErr TrapRename(char *pb, Globals *g, short isHFS)
{
	TrapLocation loc = ExtractLocation(pb, isHFS, g);
	unsigned long newNameAddr = *(unsigned long *)(pb + pb_ioMisc);
	if (loc.nameAddr == 0 || newNameAddr == 0) return kParamErr;
	reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
	reg_set(g->regBase, 1, (unsigned long)loc.dirID);
	reg_set(g->regBase, 2, loc.nameAddr);
	reg_set(g->regBase, 3, kFileOpRename);
	reg_set(g->regBase, 4, newNameAddr);
	reg_command(g->regBase, kCmdFileOpByName);
	return host_err(g->regBase);
}

static OSErr TrapGetVolInfo(char *pb, Globals *g, short isHFS)
{
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	Ptr v = g->vcb;
	unsigned long fileCount, dirCount, totalBytes;
	short vidx = *(short *)(pb + pb_ioVolIndex);

	/* Indexed VCB walk: only handle if Nth VCB is ours */
	if (vidx > 0) {
		Ptr vcb = *(Ptr *)(kVCBQHdr + 2); /* qHead */
		short i;
		for (i = 1; i < vidx && vcb != NULL; i++)
			vcb = *(Ptr *)vcb;
		if (vcb == NULL || vcb != g->vcb)
			return 1; /* not ours — sentinel for pass-through */
	} else if (vidx == 0) {
		short vRefNum = *(short *)(pb + pb_ioVRefNum);
		if (vRefNum != kOurDriveNum && !IsOurVolume(vRefNum))
			return 1; /* not ours */
	}

	/* Query host for fresh counts */
	reg_command(g->regBase, kCmdGetVol);
	fileCount  = reg_get(g->regBase, 0);
	totalBytes = reg_get(g->regBase, 1);
	dirCount   = reg_get(g->regBase, 2);

	if (nameAddr != 0) {
		unsigned char *p = (unsigned char *)nameAddr;
		p[0]=6; p[1]='S'; p[2]='h'; p[3]='a';
		p[4]='r'; p[5]='e'; p[6]='d';
	}

	/* Return the WD refnum (not raw volume ref) so apps that use the
	   returned ioVRefNum for subsequent Open calls carry directory context.
	   On a real HFS volume, GetVolInfo preserves WD refnums in ioVRefNum. */
	*(short *)(pb + pb_ioVRefNum) = g->defaultWDRefNum;
	*(long  *)(pb + 30) = *(long *)(v + 10);     /* ioVCrDate */
	*(long  *)(pb + 34) = *(long *)(v + 14);     /* ioVLsMod */
	*(short *)(pb + 38) = 0;                     /* ioVAtrb */
	*(short *)(pb + 40) = (short)fileCount;      /* ioVNmFls */
	*(short *)(pb + 42) = 0;                     /* ioVBitMap */
	*(short *)(pb + 44) = 0;                     /* ioVAllocPtr */
	*(short *)(pb + pb_ioVNmAlBlks) = kTotalAllocBlks;
	*(long  *)(pb + pb_ioVAlBlkSiz) = kAllocBlkSize;
	*(long  *)(pb + pb_ioVClpSiz) = kAllocBlkSize;
	*(short *)(pb + 56) = 0;                     /* ioAlBlSt */
	*(long  *)(pb + 58) = fileCount + 16;        /* ioVNxtCNID */
	{
		long usedBlks = (long)((totalBytes + kAllocBlkSize - 1)
			/ kAllocBlkSize);
		long freeBlks = kTotalAllocBlks - usedBlks;
		if (freeBlks < 0) freeBlks = 0;
		*(short *)(pb + pb_ioVFrBlk) = (short)freeBlks;
	}

	if (isHFS) {
		*(short *)(pb + 64) = 0x4244;                /* ioVSigWord */
		*(short *)(pb + 66) = kOurDriveNum;          /* ioVDrvInfo */
		*(short *)(pb + 68) = kOurDrvrRefNum;        /* ioVDRefNum */
		*(short *)(pb + 70) = 0x5344;                /* ioVFSID */
		*(long  *)(pb + 72) = 0;                     /* ioVBkUp */
		*(short *)(pb + 76) = 0;                     /* ioVSeqNum */
		*(long  *)(pb + 78) = 0;                     /* ioVWrCnt */
		*(long  *)(pb + 82) = fileCount;             /* ioVFilCnt */
		*(long  *)(pb + 86) = dirCount;              /* ioVDirCnt */
		mem_zero(pb + 90, 32);                       /* ioVFndrInfo */
	}
	return kNoErr;
}

static OSErr TrapGetVol(char *pb, Globals *g, short isHFS)
{
	unsigned long nmAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	if (nmAddr != 0) {
		unsigned char *p = (unsigned char *)nmAddr;
		p[0]=6; p[1]='S'; p[2]='h'; p[3]='a';
		p[4]='r'; p[5]='e'; p[6]='d';
	}
	*(short *)(pb + pb_ioVRefNum) = g->defaultWDRefNum;
	if (isHFS) {
		*(short *)(pb + 32) = kOurVRefNum;      /* ioWDVRefNum: real volume ref */
		*(long  *)(pb + 28) = 0;                /* ioWDProcID */
		/* ioWDDirID: resolve defaultWDRefNum to its directory */
		if (g->defaultWDRefNum != g->rootWDRefNum) {
			unsigned long wdRef = (unsigned long)
				(-(long)g->defaultWDRefNum - 32000);
			reg_set(g->regBase, 0, wdRef);
			reg_command(g->regBase, kCmdGetWDInfo);
			if (reg_result(g->regBase) == 0)
				*(long *)(pb + 48) = (long)reg_get(g->regBase, 1);
			else
				*(long *)(pb + 48) = kRootDirID;
		} else {
			*(long *)(pb + 48) = kRootDirID;    /* ioWDDirID */
		}
	}
	return kNoErr;
}

static OSErr TrapSetVol(char *pb, Globals *g, short isHFS)
{
	short vRefNum = *(short *)(pb + pb_ioVRefNum);
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);

	/* Check by name if vRefNum doesn't match */
	if (!IsOurVolume(vRefNum)) {
		if (nameAddr != 0) {
			unsigned char *p = (unsigned char *)nameAddr;
			if (p[0] == 6 && p[1] == 'S' && p[2] == 'h'
				&& p[3] == 'a' && p[4] == 'r'
				&& p[5] == 'e' && p[6] == 'd')
			{
				*(Ptr *)kDefVCBPtr = g->vcb;
				g->defaultWDRefNum = g->rootWDRefNum;
				return kNoErr;
			}
		}
		return 1; /* not ours — sentinel for pass-through */
	}

	/* Store the caller's WD refnum (or rootWDRefNum for raw vol ref).
	   HSetVol with the raw volume ref (-32000) carries the target
	   directory in ioWDDirID (pb+48) — open a WD for it so that
	   subsequent flat Open/Create calls with vRefNum=0 land there. */
	if (vRefNum < kOurVRefNum && vRefNum > -32100) {
		/* Caller passed an explicit WD refnum — use it directly */
		g->defaultWDRefNum = vRefNum;
	} else if (isHFS) {
		long dirID = *(long *)(pb + pb_ioWDDirID);
		if (dirID != 0 && dirID != (long)kRootDirID) {
			reg_set(g->regBase, 0, (unsigned long)kOurVRefNum);
			reg_set(g->regBase, 1, (unsigned long)dirID);
			reg_set(g->regBase, 2, 0);  /* procID = 0 for system WDs */
			reg_command(g->regBase, kCmdOpenWD);
			if (reg_result(g->regBase) == 0) {
				unsigned long wdRef = reg_get(g->regBase, 0);
				g->defaultWDRefNum = (short)(-(long)wdRef - 32000);
			} else {
				g->defaultWDRefNum = g->rootWDRefNum;
			}
		} else {
			g->defaultWDRefNum = g->rootWDRefNum;
		}
	} else {
		g->defaultWDRefNum = g->rootWDRefNum;
	}

	*(Ptr *)kDefVCBPtr = g->vcb;
	return kNoErr;
}

static OSErr TrapUnmountVol(char *pb, Globals *g, short isHFS)
{
	if (g->dqe != NULL) {
		Dequeue((QElemPtr)(g->dqe + 4), (QHdrPtr)kDrvQHdr);
		g->dqe = NULL;
	}
	Dequeue((QElemPtr)g->vcb, (QHdrPtr)kVCBQHdr);
	g->ejected = 1;
	return kNoErr;
}

static OSErr TrapEject(char *pb, Globals *g, short isHFS)
{
	if (g->dqe != NULL) {
		Dequeue((QElemPtr)(g->dqe + 4), (QHdrPtr)kDrvQHdr);
		g->dqe = NULL;
	}
	Dequeue((QElemPtr)g->vcb, (QHdrPtr)kVCBQHdr);
	g->ejected = 1;
	return kNoErr;
}

static OSErr TrapFlushVol(char *pb, Globals *g, short isHFS)
{
	return kNoErr;
}

static OSErr TrapOpenWD(char *pb, Globals *g, short isHFS)
{
	long dirID = *(long *)(pb + pb_ioWDDirID);
	long procID = *(long *)(pb + pb_ioWDProcID);

	reg_set(g->regBase, 0, (unsigned long)kOurVRefNum);
	reg_set(g->regBase, 1, (unsigned long)dirID);
	reg_set(g->regBase, 2, (unsigned long)procID);
	reg_command(g->regBase, kCmdOpenWD);
	if (reg_result(g->regBase) != 0) return kFnfErr;

	{
		unsigned long wdRef = reg_get(g->regBase, 0);
		*(short *)(pb + pb_ioVRefNum) = (short)(-(long)wdRef - 32000);
	}
	return kNoErr;
}

static OSErr TrapCloseWD(char *pb, Globals *g, short isHFS)
{
	short vRefNum = *(short *)(pb + pb_ioVRefNum);
	unsigned long wdRef = (unsigned long)(-(long)vRefNum - 32000);

	reg_set(g->regBase, 0, wdRef);
	reg_command(g->regBase, kCmdCloseWD);
	return kNoErr;
}

static OSErr TrapGetWDInfo(char *pb, Globals *g, short isHFS)
{
	short vRefNum = *(short *)(pb + pb_ioVRefNum);
	short wdIndex = *(short *)(pb + pb_ioWDIndex);
	unsigned long dirID;

	/* Only handle direct lookup (ioWDIndex == 0) */
	if (wdIndex != 0) return kNsvErr;

	if (vRefNum == kOurVRefNum || vRefNum == kOurDriveNum) {
		dirID = kRootDirID;
		*(long *)(pb + pb_ioWDProcID) = 0;
	} else {
		unsigned long wdRef = (unsigned long)(-(long)vRefNum - 32000);
		reg_set(g->regBase, 0, wdRef);
		reg_command(g->regBase, kCmdGetWDInfo);
		if (reg_result(g->regBase) != 0) return kNsvErr;
		*(long *)(pb + pb_ioWDProcID) = (long)reg_get(g->regBase, 0);
		dirID = reg_get(g->regBase, 1);
	}

	*(short *)(pb + pb_ioWDVRefNum) = kOurVRefNum;
	*(long *)(pb + pb_ioWDDirID) = dirID;
	{
		unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
		if (nameAddr != 0) {
			unsigned char *p = (unsigned char *)nameAddr;
			p[0]=6; p[1]='S'; p[2]='h'; p[3]='a';
			p[4]='r'; p[5]='e'; p[6]='d';
		}
	}
	return kNoErr;
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
	if (actual >= 2)
		*(short *)(bufAddr + 0) = 1;

	/* vMAttrib = 0 (no special capabilities) */
	if (actual >= 6)
		*(long *)(bufAddr + 2) = 0;

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
	if (fcb == NULL || *(long *)(fcb + kFCBFlNum) == 0)
		return -51; /* rfNumErr */

	if (nmAddr != 0)
		pstr_copy_max((char *)nmAddr, fcb + kFCBCName, 31);

	*(short *)(pb + pb_ioRefNum) = refNum;
	*(long  *)(pb + pb_ioFCBFlNm) = *(long *)(fcb + kFCBFlNum);
	*(short *)(pb + pb_ioFCBFlags) = (short)((unsigned short)(*(unsigned char *)(fcb + kFCBFlags)) << 8
		| (unsigned char)(*(unsigned char *)(fcb + kFCBTypByt)));
	*(short *)(pb + pb_ioFCBStBlk) = 0;
	*(long  *)(pb + pb_ioFCBEOF) = *(long *)(fcb + kFCBEOF);
	*(long  *)(pb + pb_ioFCBPLen) = *(long *)(fcb + kFCBPLen);
	*(long  *)(pb + pb_ioFCBCrPs) = *(long *)(fcb + kFCBCrPs);
	*(short *)(pb + pb_ioFCBVRefNum) = kOurVRefNum;
	*(long  *)(pb + pb_ioFCBClpSiz) = 0;
	*(long  *)(pb + pb_ioFCBParID) = *(long *)(fcb + kFCBDirID);
	return kNoErr;
}

static OSErr TrapDirCreate(char *pb, Globals *g, short isHFS)
{
	long dirID = *(long *)(pb + pb_ioDirID);
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	short vRefNum;

	if (nameAddr == 0) return kParamErr;

	vRefNum = *(short *)(pb + pb_ioVRefNum);
	reg_set(g->regBase, 0, (unsigned long)dirID);
	reg_set(g->regBase, 1, nameAddr);
	reg_command(g->regBase, kCmdCreateDir);
	if (reg_result(g->regBase) != 0)
		return host_err(g->regBase);
	*(long *)(pb + pb_ioDirID) = (long)reg_get(g->regBase, 0);
	return kNoErr;
}

static OSErr TrapCatMove(char *pb, Globals *g, short isHFS)
{
	long srcDirID = *(long *)(pb + pb_ioDirID);
	long dstDirID = *(long *)(pb + 36);  /* ioNewDirID */
	unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
	if (nameAddr == 0) return kParamErr;

	reg_set(g->regBase, 0, (unsigned long)srcDirID);
	reg_set(g->regBase, 1, nameAddr);
	reg_set(g->regBase, 2, (unsigned long)dstDirID);
	reg_command(g->regBase, kCmdCatMove);
	return host_err(g->regBase);
}

static OSErr TrapSetVInfo(char *pb, Globals *g, short isHFS)
{
	return kNoErr;
}

/* ================================================================ */
/*              Central dispatchers (called from stubs)             */
/* ================================================================ */

/* Pass-through sentinel: returned by handlers that discover the
   call is not for our volume, after internal ownership checks
   (e.g. TrapGetVolInfo's indexed walk, TrapSetVol's name match). */
#define kPassThrough  1

typedef OSErr (*TrapHandler)(char *pb, Globals *g, short isHFS);

typedef struct {
	short       trapNum;
	TrapHandler handler;
	short       refBased;  /* 1=check IsOurFCB, 0=check IsOurVolume */
} TrapEntry;

#define kMaxFlatTraps 23
#define kMaxHFSTraps  11

static TrapEntry sFlatTraps[kMaxFlatTraps];
static TrapEntry sHFSTraps[kMaxHFSTraps];

static void AddEntry(TrapEntry *table, short *idx, short maxN,
	short trapNum, TrapHandler handler, short refBased)
{
	if (*idx < maxN - 1) {
		table[*idx].trapNum  = trapNum;
		table[*idx].handler  = handler;
		table[*idx].refBased = refBased;
		(*idx)++;
		/* Keep sentinel zeroed (arrays are already cleared) */
	}
}

static void InitTrapTables(void)
{
	short fi = 0, hi = 0;

	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x00, TrapOpen,        0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x01, TrapClose,       1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x02, TrapRead,        1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x03, TrapWrite,       1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x07, TrapGetVolInfo,  0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x08, TrapCreate,      0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x09, TrapDelete,      0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0A, TrapOpenRF,      0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0B, TrapRename,      0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0C, TrapGetFileInfo, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0D, TrapSetFileInfo, 0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x0E, TrapUnmountVol,  0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x10, TrapAllocate,    0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x11, TrapGetEOF,      1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x12, TrapSetEOF,      1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x13, TrapFlushVol,    0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x14, TrapGetVol,      0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x15, TrapSetVol,      0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x17, TrapEject,       0);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x18, TrapGetFPos,     1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x44, TrapSetFPos,     1);
	AddEntry(sFlatTraps, &fi, kMaxFlatTraps, 0x45, TrapFlushFile,   1);

	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0001, TrapOpenWD,     0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0002, TrapCloseWD,    0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0005, TrapCatMove,    0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0006, TrapDirCreate,  0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0007, TrapGetWDInfo,  0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0008, TrapGetFCBInfo, 1);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0009, TrapGetCatInfo, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x000A, TrapSetCatInfo, 0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x000B, TrapSetVInfo,   0);
	AddEntry(sHFSTraps, &hi, kMaxHFSTraps, 0x0030, TrapGetVolParms,0);
}

static short DispatchFromTable(TrapEntry *table, short key,
	char *pb, Globals *g, short isHFS, short trapWord)
{
	TrapEntry *e;
	OSErr err;

	for (e = table; e->handler != NULL; e++) {
		if (e->trapNum != key) continue;

		/* Ownership check */
		if (e->refBased) {
			if (!IsOurFCB(*(short *)(pb + pb_ioRefNum)))
				return 1;
		} else {
			/* GetVolInfo and SetVol do their own ownership checks
			   internally and return kPassThrough if not ours */
		}

		err = e->handler(pb, g, isHFS);
		if (err == kPassThrough) {
			log_trap(g->regBase, trapWord, pb,
				LOG_PASSTHRU, 0, isHFS ? LOG_F_HFS : 0);
			return 1;
		}
		*(short *)(pb + pb_ioResult) = err;
		log_trap(g->regBase, trapWord, pb,
			err ? LOG_ERROR : LOG_HANDLED, err,
			(isHFS ? LOG_F_HFS : 0) | LOG_F_PBMOD);
		return 0;
	}
	return 1;  /* not in table — pass through */
}

short DispatchFlat(char *pb, short trapWord)
{
	short trapNum = trapWord & 0xFF;
	short isHFS   = (trapWord & 0x0200) != 0;
	Globals *g;
	short result;

	SetUpA4();
	g = get_globals();
	if (g == NULL || g->ejected) { RestoreA4(); return 1; }

	/* For non-refBased traps, check volume ownership before dispatch.
	   GetVolInfo and SetVol handle ownership internally. */
	{
		TrapEntry *e;
		for (e = sFlatTraps; e->handler != NULL; e++) {
			if (e->trapNum != trapNum) continue;
			if (!e->refBased && trapNum != 0x07 && trapNum != 0x15) {
				if (!IsOurVolume(*(short *)(pb + pb_ioVRefNum))) {
					log_trap(g->regBase, trapWord, pb,
						LOG_PASSTHRU, 0, isHFS ? LOG_F_HFS : 0);
					RestoreA4(); return 1;
				}
			}
			break;
		}
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
	if (g == NULL || g->ejected) { RestoreA4(); return 1; }

	/* For non-refBased traps, check volume ownership */
	{
		TrapEntry *e;
		for (e = sHFSTraps; e->handler != NULL; e++) {
			if (e->trapNum != selector) continue;
			if (!e->refBased) {
				if (!IsOurVolume(*(short *)(pb + pb_ioVRefNum))) {
					log_trap(g->regBase, selector, pb,
						LOG_PASSTHRU, 0, LOG_F_HFS);
					RestoreA4(); return 1;
				}
			}
			break;
		}
	}

	result = DispatchFromTable(sHFSTraps, selector, pb, g, 1, selector);
	if (result != 0) {
		/* Unknown HFS selector: return paramErr */
		*(short *)(pb + pb_ioResult) = kParamErr;
		log_trap(g->regBase, selector, pb,
			LOG_ERROR, kParamErr, LOG_F_HFS);
		result = 0;
	}
	RestoreA4();
	return result;
}

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

	The ROM trap dispatcher sets D1.W = full trap word before
	calling the handler. Bit 9 distinguishes PBHxxx ($A2xx)
	from PBxxx ($A0xx) — the "hierarchical" flag.
*/
static Ptr MakeFlatStub(long dispatchAddr, long oldAddr)
{
	Ptr p;
	short *w;

	p = NewPtrSys(42);
	if (p == NULL) return NULL;
	w = (short *)p;

	/* MOVEM.L D0-D2/A0-A1, -(SP) */
	*w++ = 0x48E7; *w++ = (short)0xE0C0;

	/* MOVE.W D1, -(SP) — push trap word from D1 */
	*w++ = 0x3F01;

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
	stub = MakeFlatStub(dispAddr, oldAddr);
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
	/* Create permanent root WD — mirrors real HFS boot-volume root WD.
	   This ensures -32000 (kOurVRefNum) never leaks to applications;
	   every vRefNum they see is a WD refnum resolvable through the
	   host WD table. */
	reg_set(regBase, 0, (unsigned long)kOurVRefNum);
	reg_set(regBase, 1, (unsigned long)kRootDirID);
	reg_set(regBase, 2, 0);  /* procID = 0 for root WD */
	reg_command(regBase, kCmdOpenWD);
	{
		unsigned long wdRef = reg_get(regBase, 0);
		g->rootWDRefNum = (short)(-(long)wdRef - 32000);
	}
	g->defaultWDRefNum = g->rootWDRefNum;

	/* Save A4 for code resource data access from stubs */
	{
		long *a4dst = &g->savedA4;
		asm {
			move.l a4dst, a0
			move.l a4, (a0)
		}
	}

	set_globals(regBase, g);

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
		*(short *)(v + 18)  = 0;             /* vcbAtrb: writable */
		*(short *)(v + 20)  = (short)g->volFileCount; /* vcbNmFls */
		*(short *)(v + 26)  = kTotalAllocBlks; /* vcbNmAlBlks */
		*(long  *)(v + 28)  = kAllocBlkSize; /* vcbAlBlkSiz */
		*(long  *)(v + 32)  = kAllocBlkSize; /* vcbClpSiz */
		*(long  *)(v + 38)  = 16;            /* vcbNxtCNID */
		{
			long used = (long)((g->volTotalBytes + kAllocBlkSize - 1)
				/ kAllocBlkSize);
			long free = kTotalAllocBlks - used;
			if (free < 0) free = 0;
			*(short *)(v + 42) = (short)free;  /* vcbFreeBks */
		}

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

	/* Add drive queue element so SFGetFile Drive button finds us.
	   Layout: 4 flag bytes + DrvQEl (16 bytes) = 20 bytes.
	   Flag bytes per TN#36: $00080000 = non-ejectable disk in place.
	   AddDrive fills in dQDrive and dQRefNum from its parameters. */
	{
		Ptr dqe = NewPtrSysClear(20);
		if (dqe != NULL) {
			*(long *)dqe = 0x00080000L;        /* flags: non-ejectable */
			*(short *)(dqe + 8)  = 1;          /* qType = 1 (use dQDrvSz2) */
			*(short *)(dqe + 14) = 0;          /* dQFSID = 0 (native HFS) */
			/* Drive size in 512-byte sectors:
			   32000 * 32768 / 512 = 2,048,000 sectors */
			{
				long sectors = (long)kTotalAllocBlks
					* (kAllocBlkSize / 512);
				*(short *)(dqe + 16) = (short)(sectors & 0xFFFF);
				*(short *)(dqe + 18) = (short)((sectors >> 16) & 0xFFFF);
			}
			AddDrive(kOurDrvrRefNum, kOurDriveNum,
				(DrvQElPtr)(dqe + 4));
			g->dqe = dqe;
			dbg_log(regBase, "SharedDrive: DQE added to drive queue");
		}
	}

	/* Build dispatch tables (can't use static init in THINK C code resources) */
	InitTrapTables();

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
	InstallFlatPatch(0xA014, regBase);  /* _GetVol */
	InstallFlatPatch(0xA015, regBase);  /* _SetVol */
	InstallFlatPatch(0xA017, regBase);  /* _Eject */
	InstallFlatPatch(0xA018, regBase);  /* _GetFPos */
	InstallFlatPatch(0xA044, regBase);  /* _SetFPos */
	InstallFlatPatch(0xA045, regBase);  /* _FlushFile */

	dbg_log(regBase, "SharedDrive: traps patched");

	/*
	 * Don't PostEvent — it would trigger _MountVol which would
	 * try to read boot blocks from drive 8 (no real disk).
	 * The VCB and DQE are already in their queues so the Finder
	 * and Standard File will discover the volume at startup.
	 */
	dbg_log(regBase, "SharedDrive INIT: done!");

bail:
	RestoreA4();
}
