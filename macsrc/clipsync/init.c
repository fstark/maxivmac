/*
	ClipSync INIT ? init.c

	THINK C code resource project. Builds as INIT resource.
	Installs a jGNEFilter that synchronises the host clipboard
	with the Mac desk scrap in each application's context.

	State is stored on the host via KV commands ($106/$107),
	so we need no A5 globals or per-partition storage.

	Register interface at extnBlockBase + $20:
	  $100 ClipVersion   -> p0 = version (must be >= 2)
	  $101 ClipExport    p0=buf addr, p1=byte count
	  $102 ClipImport    p0=buf addr, p1=capacity -> p1=actual
	  $103 ClipHasData   -> p0 = 0 or 1
	  $104 ClipGetLen    -> p0 = byte count
	  $105 ClipSeqNo     -> p0 = sequence number
	  $106 ClipKVSet     p0=key, p1=value
	  $107 ClipKVGet     p0=key -> p0=value
	  $108 DbgLog       p0=fmt string addr, p1-p6=args
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
#define REG_P(n)     (0x04 + (n) * 4)  /* p0=0x04, p1=0x08, ... p6=0x1C */

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

#define dbg_log(b,s)                 dbg_log6(b,s,0,0,0,0,0,0)
#define dbg_log1(b,s,a)              dbg_log6(b,s,a,0,0,0,0,0)
#define dbg_log2(b,s,a,c)            dbg_log6(b,s,(long)(a),(long)(c),0,0,0,0)
#define dbg_log3(b,s,a,c,d)          dbg_log6(b,s,a,c,d,0,0,0)
#define dbg_log4(b,s,a,c,d,e)        dbg_log6(b,s,a,c,d,e,0,0)
#define dbg_log5(b,s,a,c,d,e,f)      dbg_log6(b,s,a,c,d,e,f,0)

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
	Import host clipboard to Mac scrap.
	Calls ClipGetLen + ClipImport, then ZeroScrap + PutScrap.
	Returns: number of bytes imported, or -1 on error / 0 if
	empty.
*/
static long ImportHostToMac(char *regBase)
{
	long len, actual;
	Ptr buf;

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

	ZeroScrap();
	PutScrap(actual, 'TEXT', buf);
	DisposPtr(buf);

	return actual;
}

/* ---- filter globals (in system heap block) ---- */

typedef struct {
	long     oldFilter;     /* previous jGNEFilter */
	char    *regBase;       /* extension register base */
	long     lastTicks;     /* Ticks at last sync check */
} FilterGlobals;

/*
	We store a pointer to the FilterGlobals in a well-known
	location: right after the code in the system heap block.
	The asm entry stub retrieves it via a PC-relative load.

	Alternatively, for simplicity in THINK C we use a global
	pointer stashed in unused low-memory. We pick $0B00 which
	is in the application-parameter area that INITs can use
	during startup, but we only need it to survive as a
	system-heap pointer. A safer approach: the asm entry
	knows a fixed offset from itself to the globals.

	For maximum portability we store the FilterGlobals* at
	the end of the system-heap code block, and the asm stub
	uses a JSR/RTS trick to find its own PC.
*/

/*
	Actually, the simplest THINK C approach: allocate the
	globals in the system heap, store the pointer in a known
	'well' address. We use the 4 bytes at $0B00-$0B03
	(inside the "scratch" area).
*/
#define kGlobalsPtr     0x0B00

/* ---- sync logic ---- */

/*
	SyncOnGNE ? called from the jGNEFilter.
	Runs in the current application's partition context.
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
		ImportHostToMac(g->regBase);
		kv_set(g->regBase, key, hostSeq);
	}

	/* --- Mac -> Host --- */
	scrapCnt = *(short *)kScrapCount;

	key = (unsigned long)appId * 2 + 1;
	lastCnt = kv_get(g->regBase, key);

	if ((unsigned long)scrapCnt != lastCnt) {
		ExportMacToHost(g->regBase);
		kv_set(g->regBase, key,
			(unsigned long)scrapCnt);
	}
}

/* ---- jGNEFilter entry point ---- */

/*
	The filter is called with:
	  A1 = pointer to EventRecord
	  D0 = boolean result from GetNextEvent
	  4(A7) = word copy of D0 (can be modified)

	We must preserve A1 and D0, call our C sync function,
	then chain to the previous filter (or RTS if none).

	THINK C inline asm approach: we write the entry code
	as a standalone function that will be copied into the
	system heap.

	For simplicity, we write the filter as a C function
	that the INIT installs. The asm glue is minimal.
*/

/*
	FilterProc ? jGNEFilter entry.

	Since THINK C doesn't let us easily write naked asm
	functions with PC-relative data, we use a simpler
	approach:

	The filter function is a normal C function. We patch
	jGNEFilter to point to a small asm stub in the system
	heap that calls SyncOnGNE and chains.

	But actually, jGNEFilter can point to a Pascal-calling-
	convention function. The simplest approach for THINK C:
	write the filter as a regular C function and install it.

	Since the INIT code resource stays loaded (we
	DetachResource it), and SyncOnGNE uses the globals
	pointer at $0B00, this works without any asm.

	The jGNEFilter calling convention:
	  - Called after GNE processing
	  - A1 = EventRecord*, D0 = result
	  - Filter can modify the event and 4(A7)
	  - Must chain to old filter via JMP or RTS

	In THINK C we can write this as:

	pascal void MyFilter(void)
	{
	    // A1 and D0 are live ? don't touch them
	    // except through asm
	    SyncOnGNE();
	    // chain to old filter
	}

	But we need asm to preserve regs and chain. Let's
	just write a tiny asm stub.
*/

/*
	We'll use a different strategy: store the filter code
	as raw bytes in a static array, patch in the globals
	pointer and old-filter address, copy to system heap.
*/

/* 68000 asm for the filter stub:
	MOVEM.L D0-D2/A0-A2, -(SP)    ; save regs
	MOVE.L  $0B00, A0              ; load FilterGlobals*
	MOVE.L  A0, -(SP)              ; push (unused, just for alignment)
	JSR     SyncOnGNE_addr         ; absolute address, patched
	ADDQ.L  #4, SP
	MOVEM.L (SP)+, D0-D2/A0-A2    ; restore regs
	MOVE.L  old_filter, A0         ; patched
	JMP     (A0)                   ; chain (or RTS if nil)

   But embedding absolute addresses requires patching.
   This is getting complex for inline C.

   SIMPLEST APPROACH: Since THINK C INIT code resources
   stay in memory after DetachResource, and THINK C
   supports inline asm, let's just make the filter a
   normal function and use SetTrapAddress-style patching.

   Actually, the absolute simplest: the jGNEFilter is
   just called as a procedure. We can set it to point to
   a C function IF that C function handles the register
   protocol. THINK C can do this with asm blocks.
*/

/*
	Final approach: write a C function with asm bookends.
	The INIT makes itself persistent (DetachResource +
	locked in system heap), so the code stays valid.
*/

static FilterGlobals *gFilterGlobals;

void FilterEntry(void)
{
char *regBase;
SetUpA4();

regBase = find_reg_base();

dbg_log2( regBase, "FilterEntry, next: %lx -> %lx", gFilterGlobals, gFilterGlobals->oldFilter );
SyncOnGNE();

RestoreA4();

return;
#if 0

	/*
		Save all regs we might clobber. A1/D0 are the
		GNE result ? we must preserve them across our
		C code. The word at 4(A7) (before our LINK) is
		the return-to-caller / old filter chain point.
	*/
	asm {
		MOVEM.L D0-D2/A0-A2, -(SP)
	}

	regBase = find_reg_base();
dbg_log( regBase, "FilterEntry" );

	SyncOnGNE();

dbg_log( regBase, "SyncOnGNE done" );

	asm {
		MOVEM.L (SP)+, D0-D2/A0-A2
	}

	/* Chain to old filter */
	if (gFilterGlobals != NULL
		&& gFilterGlobals->oldFilter != 0)
	{
		/*
			Jump to old filter. We can't just call it
			as a C function ? it expects the raw
			jGNEFilter calling convention. Use asm.
		*/
		asm {
			MOVE.L  gFilterGlobals, A0
			MOVE.L  FilterGlobals.oldFilter(A0), A0
			UNLK    A6
			JMP     (A0)
		}
	}
	/* If no old filter, just return */
#endif

}

/* ---- INIT entry point ---- */

void main(void)
{
	char *regBase;
	unsigned long version;
	FilterGlobals *g;
	Handle self;

	Ptr myINITPtr;
	asm
	{
		move.l a0, myINITPtr;
	}
	RememberA0();
	SetUpA4();

	/* Find extension register base */
	regBase = find_reg_base();
	if (regBase == NULL)
		return;

	dbg_log1(regBase, "ClipSync INIT: starting, regBase=%lx",
		(unsigned long)regBase);

	/* Check version ? need >= 2 for KV commands */
	reg_command(regBase, 0x0100);
	version = reg_get(regBase, 0);
	if (version < 2) {
		dbg_log1(regBase, "ClipSync INIT: version %ld < 2, bailing",
			version);
		return;
	}

	/* Allocate globals in system heap */
	g = (FilterGlobals *)NewPtrSys(
		sizeof(FilterGlobals));
	if (g == NULL) {
		dbg_log(regBase, "ClipSync INIT: NewPtrSys failed!");
		return;
	}

	g->regBase = regBase;
	g->lastTicks = 0;
	g->oldFilter = *(long *)kJGNEFilter;

	/* Store globals pointer at well-known address
	   AND in a C global for FilterEntry to use */
	*(FilterGlobals **)kGlobalsPtr = g;
	gFilterGlobals = g;

	dbg_log2(regBase, "ClipSync INIT: regBase=%lx version=%ld",
		(unsigned long)regBase, version);

	dbg_log2(regBase, "ClipSync INIT: globals at %lx, oldFilter=%lx",
		(unsigned long)g, (unsigned long)g->oldFilter);

	/* Make our INIT code resource persistent.
	   INIT 31 passes the resource handle in A0 on entry.
	   Since THINK C code resources don't start at main(),
	   RecoverHandle((Ptr)main) would give a bogus result.
	   Instead, we use the handle that INIT 31 saved for us.
	
	   THINK C INIT convention: the handle is on the stack
	   as a parameter (or we can grab it before main's
	   prologue clobbers A0).  Since we can't reliably
	   get A0 after the C prologue, the safest approach is
	   to just call RecoverHandle on the very first byte
	   of the code resource ? but we don't know where that
	   is either.
	
	   Simplest fix: skip DetachResource entirely.  Instead,
	   copy FilterEntry into a system-heap Ptr, and point
	   jGNEFilter there.  The code resource itself can be
	   released ? we don't need it after main() returns.
	
	   BUT ? the filter calls SyncOnGNE and other static
	   functions that are in this code resource.  So the
	   code resource MUST stay loaded.
	
	   Alternative: HGetResource to get our own handle.
	   GetResource('INIT', 128) should return us.
	*/
	self = GetResource('INIT', 314);
	dbg_log1(regBase, "ClipSync INIT: GetResource=%lx",
		(unsigned long)self);
	if (self != NULL) {
		DetachResource(self);
		HLock(self);
		HNoPurge(self);
		dbg_log( regBase, "INIT detached, locked, no purge" );
	} else {
		dbg_log(regBase, "ClipSync INIT: WARNING no handle, code may be purged!");
	}

	/* Install our filter */
	*(long *)kJGNEFilter = (long)FilterEntry;

	dbg_log1(regBase, "ClipSync INIT: filter installed at %lx",
		(unsigned long)FilterEntry);

#if 0
	/* Sync initial state for the boot app (Finder).
	   Read current scrapCount and hostSeqNo so we
	   don't trigger a spurious import/export on the
	   first GNE call. */
	{
		short appId;
		unsigned long hostSeq;
		short scrapCnt;

		appId = *(short *)kCurApRefNum;

		reg_command(regBase, 0x0105);
		hostSeq = reg_get(regBase, 0);
		kv_set(regBase, (unsigned long)appId * 2,
			hostSeq);

		scrapCnt = *(short *)kScrapCount;
		kv_set(regBase, (unsigned long)appId * 2 + 1,
			(unsigned long)scrapCnt);

		dbg_log3(regBase, "ClipSync INIT: seeded appId=%ld seq=%lx cnt=%ld",
			(unsigned long)appId, hostSeq, (unsigned long)scrapCnt);
	}
#endif

	dbg_log(regBase, "ClipSync INIT: installed OK");
	
	RestoreA4();
}
