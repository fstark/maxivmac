/*
	maxivmac INIT — defs.h
	Shared definitions for the unified INIT resource.
	All .c files include this header.
*/

#ifndef DEFS_H
#define DEFS_H

#include <SetUpA4.h>
#include <Memory.h>
#include <OSUtils.h>
#include <Files.h>
#include <Devices.h>
#include <Events.h>
#include <Scrap.h>
#include <SegLoad.h>
#include <Shutdown.h>

/* ---- Low-memory globals ---- */

#define kSonyVarsPtr 0x0134
#define kCheckVal 0x841339E2UL
#define kVCBQHdr 0x0356
#define kDrvQHdr 0x0308
#define kFCBSPtr 0x034E
#define kDefVCBPtr 0x0352
#define kJGNEFilter 0x029A
#define kCurApRefNum 0x0900
#define kScrapCount 0x0968

/* ---- Volume/drive constants ---- */

#define kBaseVRefNum 32000
#define kBaseDriveNum 8
#define kMaxDrives 8
#define kOurVRefNum (-(kBaseVRefNum))
#define kOurDriveNum (kBaseDriveNum)
#define kOurDrvrRefNum (-64)
#define kRootDirID 2

/* Virtual volume geometry — report ~1 GB capacity so copy operations
   don't fail with "disk full".  The host filesystem provides the
   actual storage; these numbers are only what we tell the Finder. */
#define kAllocBlkSize 32768L  /* 32 KB allocation blocks */
#define kTotalAllocBlks 32000 /* 32000 * 32KB ~ 1 GB */

/* VCB field offsets (from Inside Macintosh IV) */
#define kVcbNxtCNID 38 /* LONGINT — next unused CNID */

/* ---- FCB offsets (from Inside Macintosh IV-179) ---- */

#define kFCBLen 94
#define kFCBFlNum 0	 /* LONGINT — file number (0=free) */
#define kFCBFlags 4	 /* byte — flags */
#define kFCBTypByt 5 /* byte — version number */
/* fcbSBlk at 6, 2 bytes — first alloc block (not used for HFS) */
#define kFCBEOF 8	/* LONGINT — logical EOF */
#define kFCBPLen 12 /* LONGINT — physical EOF */
#define kFCBCrPs 16 /* LONGINT — mark (current position) */
#define kFCBVPtr 20 /* LONGINT — pointer to VCB */
/* HFS-specific fields (from FSEqu.a in SuperMario sources) */
#define kFCBClmpSize 30 /* LONGINT — clump size */
#define kFCBBTCBPtr 34	/* LONGINT — B*-Tree control block ptr */
/* We repurpose kFCBBTCBPtr to store the host file handle.
   kFCBPLen must hold the real physical EOF so the ROM
   Resource Manager doesn't think the file is truncated. */
#define kFCBHostHandle kFCBBTCBPtr
#define kFCBExtRec 38 /* 12 bytes — first 3 file extents */
#define kFCBFType 50  /* LONGINT — file's 4 Finder type bytes */
#define kFCBCatPos 54 /* LONGINT — catalog hint for Close */
#define kFCBDirID 58  /* LONGINT — parent directory ID */
#define kFCBCName 62  /* 32 bytes — file name (Pascal string) */

/* ---- ParamBlock field offsets ---- */

/* Shared header (all variants) */
#define pb_ioResult 16
#define pb_ioNamePtr 18
#define pb_ioVRefNum 22
#define pb_ioRefNum 24 /* also ioFRefNum */

/* ioParam variant (for _Open, _Read, _Write, _Close, _GetEOF, _SetFPos...) */
#define pb_ioMisc 28	  /* Ptr — _GetEOF result goes here */
#define pb_ioBuffer 32	  /* Ptr */
#define pb_ioReqCount 36  /* LONGINT */
#define pb_ioActCount 40  /* LONGINT */
#define pb_ioPosMode 44	  /* INTEGER */
#define pb_ioPosOffset 46 /* LONGINT */

/* ioParam: permission byte (for _Open, _OpenRF) */
#define pb_ioPermssn 27 /* SignedByte */

/* fileParam variant (for _GetFileInfo, _SetFileInfo, _Create, _Delete) */
#define pb_ioFDirIndex 28  /* INTEGER */
#define pb_ioFlAttrib 30   /* SignedByte */
#define pb_ioFlFndrInfo 32 /* FInfo, 16 bytes */
#define pb_ioFlNum 48	   /* LONGINT — file number */
#define pb_ioFlStBlk 52	   /* INTEGER */
#define pb_ioFlLgLen 54	   /* LONGINT — data fork logical EOF */
#define pb_ioFlPyLen 58	   /* LONGINT — data fork physical EOF */
#define pb_ioFlRStBlk 62   /* INTEGER */
#define pb_ioFlRLgLen 64   /* LONGINT — rsrc fork logical EOF */
#define pb_ioFlRPyLen 68   /* LONGINT — rsrc fork physical EOF */
#define pb_ioFlCrDat 72	   /* LONGINT — creation date */
#define pb_ioFlMdDat 76	   /* LONGINT — modification date */

/* HFS-extended fileParam / CInfoPBRec offsets (Inside Mac IV-109) */
#define pb_ioFlBkDat 80		/* LONGINT — backup date */
#define pb_ioFlXFndrInfo 84 /* FXInfo, 16 bytes */
#define pb_ioFlClpSiz 104	/* LONGINT — clump size */

/* HFS-extended dirInfo offsets */
#define pb_ioDrBkDat 80	   /* LONGINT — backup date */
#define pb_ioDrFndrInfo 84 /* DXInfo, 16 bytes */

/* CInfoPBRec hFileInfo variant (for PBGetCatInfo, files) */
#define pb_ioFlParID 100 /* LONGINT — parent dir ID */

/* CInfoPBRec dirInfo variant (for PBGetCatInfo, directories) */
#define pb_ioDrUsrWds 32 /* DInfo, 16 bytes */
#define pb_ioDrDirID 48	 /* LONGINT — directory ID */
#define pb_ioDrNmFls 52	 /* INTEGER — number of files in dir */
#define pb_ioDrCrDat 72	 /* LONGINT */
#define pb_ioDrMdDat 76	 /* LONGINT */
#define pb_ioDrParID 100 /* LONGINT */

/* volumeParam variant (for _GetVolInfo / PBHGetVInfo) */
#define pb_ioVolIndex 28  /* INTEGER */
#define pb_ioVCrDate 30	  /* LONGINT */
#define pb_ioVLsMod 34	  /* LONGINT (HFS) */
#define pb_ioVAtrb 38	  /* INTEGER */
#define pb_ioVNmFls 40	  /* INTEGER */
#define pb_ioVNmAlBlks 46 /* INTEGER */
#define pb_ioVAlBlkSiz 48 /* LONGINT */
#define pb_ioVClpSiz 52	  /* LONGINT */
#define pb_ioVFrBlk 62	  /* INTEGER */

/* HFS-specific PBGetCatInfo: ioDirID is at same offset as ioFlNum */
#define pb_ioDirID 48

/* FCBPBRec variant (for PBGetFCBInfo) — TN087 corrected offsets */
#define pb_ioFCBIndx 28	   /* INTEGER (TN087: not LONGINT) */
#define pb_ioFCBFlNm 32	   /* LONGINT — file number */
#define pb_ioFCBFlags 36   /* INTEGER — FCB flags */
#define pb_ioFCBStBlk 38   /* INTEGER — first alloc block */
#define pb_ioFCBEOF 40	   /* LONGINT — logical EOF */
#define pb_ioFCBPLen 44	   /* LONGINT — physical EOF */
#define pb_ioFCBCrPs 48	   /* LONGINT — mark */
#define pb_ioFCBVRefNum 52 /* INTEGER — volume refnum */
#define pb_ioFCBClpSiz 54  /* LONGINT — clump size */
#define pb_ioFCBParID 58   /* LONGINT — parent dir ID */

/* WDParam variant (for _OpenWD, _CloseWD, _GetWDInfo) */
#define pb_ioWDIndex 26	  /* INTEGER */
#define pb_ioWDProcID 28  /* LONGINT */
#define pb_ioWDVRefNum 32 /* INTEGER */
#define pb_ioWDDirID 48	  /* LONGINT */

/* ---- Permission constants (IM IV) ---- */

#define kFsCurPerm 0  /* whatever is allowed */
#define kFsRdPerm 1	  /* read only */
#define kFsWrPerm 2	  /* write permission */
#define kFsRdWrPerm 3 /* exclusive read/write */

/* ---- HFS selectors ---- */

#define kGetCatInfo 0x0009
#define kSetCatInfo 0x000A
#define kOpenWD 0x0001
#define kCloseWD 0x0002
#define kGetWDInfo 0x0007
#define kCatMove 0x0005
#define kDirCreate 0x0006
#define kSetVInfo 0x000B
#define kCreateFileIDRef 0x0010
#define kDeleteFileIDRef 0x0011
#define kResolveFileIDRef 0x0012
#define kGetFCBInfo 0x0008
#define kGetVolParms 0x0030

/* ---- Mac OS error codes ---- */

#define kNoErr 0
#define kIoErr (-36)
#define kEofErr (-39)
#define kPosErr (-40)
#define kTmfoErr (-42)
#define kFnfErr (-43)
#define kWPrErr (-44)
#define kOpWrErr (-49)
#define kParamErr (-50)
#define kRfNumErr (-51)
#define kWrPermErr (-61)
#define kNsvErr (-35)

/* ---- Host command codes — drive range ($2xx) ---- */

#define kCmdClose 0x0206
#define kCmdRead 0x0205
#define kCmdWrite 0x0211
#define kCmdSetEOF 0x0218
#define kCmdGetVol 0x0201

#define kPB_GetCatInfo 0x0230
#define kPB_GetFileInfo 0x0231
#define kPB_Open 0x0232
#define kPB_OpenRF 0x0233
#define kPB_Create 0x0238
#define kPB_Delete 0x0239
#define kPB_Rename 0x023A
#define kPB_DirCreate 0x023D
#define kPB_CatMove 0x023E
#define kPB_SetFileInfo 0x023B
#define kPB_SetCatInfo 0x023C
#define kPB_OpenWD 0x0242
#define kPB_CloseWD 0x0243
#define kPB_GetWDInfo 0x0244
#define kPB_SetVol 0x0246
#define kPB_GetVol 0x0247

#define kExtFSPollMount 0x0219
#define kExtFSGetVolName 0x021A
#define kExtFSGuestCmd 0x0220

/* Host returns this when the volume is not ours — guest passes through to ROM */
#define kNotOurs 0xFFFE

/* Pass-through sentinel: returned by handlers that discover the
   call is not for our volume.  The dispatch loop treats this as
   "not handled — fall through to ROM". */
#define kPassThrough 1

/* ---- Host command codes — clipboard range ($1xx) ---- */

#define kClipVersion 0x0100
#define kClipExport 0x0101
#define kClipImport 0x0102
#define kClipHasData 0x0103
#define kClipGetLen 0x0104
#define kClipSeqNo 0x0105
#define kClipKVSet 0x0106
#define kClipKVGet 0x0107
#define kClipDbgLog 0x0108

/* ---- Trap-log flags ---- */

#define LOG_PASSTHRU 0
#define LOG_HANDLED 1
#define LOG_ERROR 2

#define LOG_F_HFS 0x0001
#define LOG_F_PBMOD 0x0002

/* ---- Extension discovery struct ---- */

typedef struct
{
	unsigned long zeroes[4];
	unsigned long checkval;
	unsigned long pokeaddr;
} MyDriverDat_R;

/* ---- Merged Globals ---- */

typedef struct
{
	/* comm */
	char *regBase;
	long savedA4;	   /* THINK C code resource A4 */

	/* drive */
	Ptr vcb[kMaxDrives];
	Ptr dqe[kMaxDrives]; /* drive queue element blocks (4 flag bytes + DrvQEl) */
	short driveCount;
	long volFileCount;
	long volTotalBytes;
	short ejected;	   /* nonzero after _Eject */

	/* clip */
	long lastClipTicks;  /* throttle clipboard checks */

	/* filter */
	long oldFilter;	   /* previous jGNEFilter */
	long lastPollTick; /* TickCount at last poll */
} Globals;

/* ---- Register access macros ---- */

#define REG_COMMAND 0x00
#define REG_RESULT 0x02
#define REG_P(n) (0x04 + (n) * 4)

/* ---- Debug log macros ---- */

#define dbg_log(b, s) dbg_log6(b, s, 0, 0, 0, 0, 0, 0)
#define dbg_log1(b, s, a) dbg_log6(b, s, (long)(a), 0, 0, 0, 0, 0)
#define dbg_log2(b, s, a, c) dbg_log6(b, s, (long)(a), (long)(c), 0, 0, 0, 0)
#define dbg_log3(b, s, a, c, d) dbg_log6(b, s, (long)(a), (long)(c), (long)(d), 0, 0, 0)

#define dbg_fatal(b, s) dbg_fatal6(b, s, 0, 0, 0, 0, 0, 0)
#define dbg_fatal1(b, s, a) dbg_fatal6(b, s, (long)(a), 0, 0, 0, 0, 0)
#define dbg_fatal2(b, s, a, c) dbg_fatal6(b, s, (long)(a), (long)(c), 0, 0, 0, 0)

/* ---- Function prototypes — comm.c ---- */

char *find_reg_base(void);
void reg_set(char *base, int n, unsigned long v);
unsigned long reg_get(char *base, int n);
void reg_command(char *base, unsigned short cmd);
unsigned short reg_result(char *base);
Globals *get_globals(void);
void set_globals(char *base, Globals *g);
void dbg_log6(char *base, char *fmt, unsigned long a, unsigned long b, unsigned long c,
			  unsigned long d, unsigned long e, unsigned long f);
void dbg_fatal6(char *base, char *fmt, unsigned long a, unsigned long b, unsigned long c,
				unsigned long d, unsigned long e, unsigned long f);
void log_trap(char *base, unsigned short trapWord, char *pb, short action, short err,
			  short flags);
void kv_set(char *regBase, unsigned long key, unsigned long val);
unsigned long kv_get(char *regBase, unsigned long key);
void pstr_copy(char *dst, char *src);
void pstr_copy_max(char *dst, char *src, short maxLen);
void mem_zero(char *dst, short len);
void mem_copy(char *dst, char *src, short len);
Boolean pstr_equal(unsigned char *a, unsigned char *b);
OSErr host_err(char *base);
OSErr host_err_passthrough(char *base);
void dbg_hexdump(char *regBase, char *label, unsigned char *addr, short len);

/* ---- Function prototypes — clip.c ---- */

void SyncClipboard(Globals *g);

/* ---- Function prototypes — drive.c ---- */

short DispatchFlat(char *pb, short trapWord);
short DispatchHFS(char *pb, short selector);
void InitTrapTables(void);
void MountNewDrive(Globals *g, short slot, short vRefNum, short driveNum);

/* ---- Function prototypes — init.c ---- */

void FilterEntry(void);

#endif /* DEFS_H */
