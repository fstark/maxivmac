/*
	ClipSync INIT ? init.c
	THINK C code resource project. Builds as INIT resource.

	Installs a jGNEFilter that synchronises the host clipboard
	with the Mac desk scrap in each application's context.
	State is stored on the host via KV commands ($106/$107),
	so we need no A5 globals or per-partition storage.

	Limitations: apps that maintain a private scrap (e.g.
	THINK C) only see host clipboard changes after a real
	MultiFinder context switch (activate Finder and back).
	Standard scrap apps work immediately.

	Register interface at extnBlockBase + $20:
	  $100 ClipVersion   -> p0 = version (must be >= 2)
	  $101 ClipExport    p0=buf addr, p1=byte count
	  $102 ClipImport    p0=buf addr, p1=capacity -> p1=actual
	  $103 ClipHasData   -> p0 = 0 or 1
	  $104 ClipGetLen    -> p0 = byte count
	  $105 ClipSeqNo     -> p0 = sequence number
	  $106 ClipKVSet     p0=key, p1=value
	  $107 ClipKVGet     p0=key -> p0=value
	  $108 DbgLog        p0=fmt string addr, p1-p6=args
*/

#include <SetUpA4.h>
#include <Memory.h>
#include <Scrap.h>
#include <Events.h>
#include <OSUtils.h>

/* ---- low-memory globals ---- */

#define kJGNEFilter     0x029A
#define kCurApRefNum    0x0900
#define kScrapCount     0x0968

/* ---- extension discovery ---- */

#define kSonyVarsPtr    0x0134
#define kCheckVal       0x841339E2UL

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
#define REG_P(n)     (0x04 + (n) * 4)  /* p0..p6 at 0x04..0x1C */

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
	reg_command(base, 0x0108);
}

#define dbg_log(b,s)              dbg_log6(b,s,0,0,0,0,0,0)
#define dbg_log1(b,s,a)           dbg_log6(b,s,a,0,0,0,0,0)
#define dbg_log2(b,s,a,c)         dbg_log6(b,s,(long)(a),(long)(c),0,0,0,0)
#define dbg_log3(b,s,a,c,d)       dbg_log6(b,s,a,c,d,0,0,0)

/* ---- KV helpers ---- */

static void kv_set(char *regBase, unsigned long key,
	unsigned long val)
{
	reg_set(regBase, 0, key);
	reg_set(regBase, 1, val);
	reg_command(regBase, 0x0106);
}

static unsigned long kv_get(char *regBase, unsigned long key)
{
	reg_set(regBase, 0, key);
	reg_command(regBase, 0x0107);
	return reg_get(regBase, 0);
}

/* ---- clipboard operations ---- */

/*
	Export Mac scrap to host clipboard.
	Reads 'TEXT' from the desk scrap, sends via ClipExport.
	Returns: number of bytes exported, or -1 on error.
*/
static long ExportMacToHost(char *regBase)
{
	Handle h;
	long offset;
	long length;

	h = NewHandle(0);
	if (h == NULL)
		return -1;

	length = GetScrap(h, 'TEXT', &offset);
	if (length <= 0) {
		DisposHandle(h);
		return (length == 0) ? 0 : -1;
	}

	HLock(h);
	reg_set(regBase, 0, (unsigned long)*h);
	reg_set(regBase, 1, (unsigned long)length);
	reg_command(regBase, 0x0101);
	HUnlock(h);
	DisposHandle(h);

	if (reg_result(regBase) != 0)
		return -1;
	return length;
}

/*
	Import host clipboard to Mac desk scrap.
	Calls ClipGetLen + ClipImport, then ZeroScrap + PutScrap.
	Returns: number of bytes imported, or -1 on error / 0 if
	empty.
*/
static long ImportHostToMac(char *regBase)
{
	long len, actual;
	Ptr buf;
	long err;

	/* ClipGetLen */
	reg_command(regBase, 0x0104);
	len = (long)reg_get(regBase, 0);
	if (len <= 0)
		return 0;

	buf = NewPtr(len);
	if (buf == NULL)
		return -1;

	/* ClipImport */
	reg_set(regBase, 0, (unsigned long)buf);
	reg_set(regBase, 1, (unsigned long)len);
	reg_command(regBase, 0x0102);
	if (reg_result(regBase) != 0) {
		DisposPtr(buf);
		return -1;
	}
	actual = (long)reg_get(regBase, 1);

	err = ZeroScrap();
	if (err != 0) {
		dbg_log1(regBase, "Import: ZeroScrap=%ld", err);
		DisposPtr(buf);
		return -1;
	}

	err = PutScrap(actual, 'TEXT', buf);
	dbg_log2(regBase, "Import: PutScrap(%ld)=%ld", actual, err);

	DisposPtr(buf);
	return (err == 0) ? actual : -1;
}

/* ---- filter globals ---- */

typedef struct {
	long     oldFilter;     /* previous jGNEFilter */
	char    *regBase;       /* extension register base */
	long     lastTicks;     /* Ticks at last sync check */
} FilterGlobals;

/*
	Globals pointer stored at $0B00 (scratch area).
	Allocated in system heap, survives across app launches.
*/
#define kGlobalsPtr     0x0B00

/* ---- sync logic ---- */

/*
	SyncOnGNE ? called from the jGNEFilter.
	Runs in the current application's context.
	Checks host clipboard and Mac scrap for changes.
*/
static void SyncOnGNE(void)
{
	FilterGlobals *g;
	long now;
	short appId;
	unsigned long hostSeq, lastSeq;
	short scrapCnt;
	unsigned long lastCnt;
	unsigned long key;

	g = *(FilterGlobals **)kGlobalsPtr;
	if (g == NULL)
		return;

	/* Throttle: at most every 30 ticks (~0.5s) */
	now = TickCount();
	if (now - g->lastTicks < 30)
		return;
	g->lastTicks = now;

	appId = *(short *)kCurApRefNum;

	/* --- Host -> Mac --- */
	reg_command(g->regBase, 0x0105);     /* ClipSeqNo */
	hostSeq = reg_get(g->regBase, 0);
	key = (unsigned long)appId * 2;
	lastSeq = kv_get(g->regBase, key);

	if (hostSeq != lastSeq) {
		dbg_log2(g->regBase, "Sync: host->mac seq %lx != %lx",
			hostSeq, lastSeq);
		ImportHostToMac(g->regBase);
		kv_set(g->regBase, key, hostSeq);
		/* Prevent feedback: update mac->host scrapCount too */
		kv_set(g->regBase, (unsigned long)appId * 2 + 1,
			(unsigned long)*(short *)kScrapCount);
	}

	/* --- Mac -> Host --- */
	scrapCnt = *(short *)kScrapCount;
	key = (unsigned long)appId * 2 + 1;
	lastCnt = kv_get(g->regBase, key);

	if ((unsigned long)scrapCnt != lastCnt) {
		dbg_log2(g->regBase, "Sync: mac->host cnt %ld != %ld",
			(unsigned long)scrapCnt, lastCnt);
		ExportMacToHost(g->regBase);
		kv_set(g->regBase, key,
			(unsigned long)scrapCnt);
	}
}

/* ---- jGNEFilter entry point ---- */

/*
	FilterEntry: jGNEFilter callback.
	Saves D0-D2/A0-A2 (GNE clobber set), calls SyncOnGNE,
	restores regs, chains to previous filter via UNLK/JMP.
*/
void FilterEntry(void)
{
	long oldFilter;

	asm { MOVEM.L D0-D2/A0-A2, -(SP) }
	SetUpA4();
	SyncOnGNE();
	RestoreA4();
	asm { MOVEM.L (SP)+, D0-D2/A0-A2 }

	/* Chain to previous filter */
	oldFilter = (*(FilterGlobals **)kGlobalsPtr)->oldFilter;
	if (oldFilter != 0) {
		asm {
			MOVE.L  oldFilter, A0
			UNLK    A6
			JMP     (A0)
		}
	}
}

/* ---- INIT entry point ---- */

void main(void)
{
	char *regBase;
	unsigned long version;
	FilterGlobals *g;
	Handle self;
	Ptr myINITPtr;

	asm { move.l a0, myINITPtr }
	RememberA0();
	SetUpA4();

	/* Find extension register base */
	regBase = find_reg_base();
	if (regBase == NULL)
		return;

	dbg_log1(regBase, "ClipSync INIT: regBase=%lx",
		(unsigned long)regBase);

	/* Check version ? need >= 2 for KV commands */
	reg_command(regBase, 0x0100);
	version = reg_get(regBase, 0);
	if (version < 2) {
		dbg_log1(regBase, "ClipSync INIT: version %ld < 2",
			version);
		return;
	}

	/* Allocate globals in system heap */
	g = (FilterGlobals *)NewPtrSys(sizeof(FilterGlobals));
	if (g == NULL) {
		dbg_log(regBase, "ClipSync INIT: NewPtrSys failed!");
		return;
	}

	g->regBase = regBase;
	g->lastTicks = 0;
	g->oldFilter = *(long *)kJGNEFilter;
	*(FilterGlobals **)kGlobalsPtr = g;

	dbg_log2(regBase, "ClipSync INIT: globals=%lx oldFilter=%lx",
		(unsigned long)g, (unsigned long)g->oldFilter);

	/* Keep our code resource in memory */
	self = GetResource('INIT', 314);
	if (self != NULL) {
		DetachResource(self);
		HLock(self);
		HNoPurge(self);
	} else {
		dbg_log(regBase, "ClipSync INIT: WARNING no handle!");
	}

	/* Install our filter */
	*(long *)kJGNEFilter = (long)FilterEntry;
	dbg_log1(regBase, "ClipSync INIT: filter at %lx",
		(unsigned long)FilterEntry);

	dbg_log(regBase, "ClipSync INIT: installed OK");
	RestoreA4();
}
