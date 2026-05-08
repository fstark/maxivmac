/*
	maxivmac INIT — clip.c
	Clipboard synchronisation between host and Mac desk scrap.
*/

#include "defs.h"

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
	{
		dbg_log(regBase, "clip: export failed (NewHandle)");
		return -1;
	}

	length = GetScrap(h, 'TEXT', &offset);
	if (length <= 0)
	{
		DisposHandle(h);
		return (length == 0) ? 0 : -1;
	}

	HLock(h);
	reg_set(regBase, 0, (unsigned long)*h);
	reg_set(regBase, 1, (unsigned long)length);
	reg_command(regBase, kClipExport);
	HUnlock(h);
	DisposHandle(h);

	if (reg_result(regBase) != 0)
	{
		dbg_log(regBase, "clip: export failed (host rejected)");
		return -1;
	}
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
	reg_command(regBase, kClipGetLen);
	len = (long)reg_get(regBase, 0);
	if (len <= 0) return 0;

	buf = NewPtr(len);
	if (buf == NULL)
	{
		dbg_log(regBase, "clip: import failed (alloc)");
		return -1;
	}

	/* ClipImport */
	reg_set(regBase, 0, (unsigned long)buf);
	reg_set(regBase, 1, (unsigned long)len);
	reg_command(regBase, kClipImport);
	if (reg_result(regBase) != 0)
	{
		dbg_log(regBase, "clip: import failed (host error)");
		DisposPtr(buf);
		return -1;
	}
	actual = (long)reg_get(regBase, 1);

	err = ZeroScrap();
	if (err != 0)
	{
		dbg_log1(regBase, "Import: ZeroScrap=%ld", err);
		DisposPtr(buf);
		return -1;
	}

	err = PutScrap(actual, 'TEXT', buf);
	dbg_log2(regBase, "Import: PutScrap(%ld)=%ld", actual, err);

	DisposPtr(buf);
	return (err == 0) ? actual : -1;
}

/* ---- sync logic ---- */

/*
	SyncClipboard — called from the unified jGNEFilter.
	Runs in the current application's context.
	Checks host clipboard and Mac scrap for changes.
	Internally throttled to every 30 ticks (~0.5s).
*/
void SyncClipboard(Globals *g)
{
	long now;
	short appId;
	unsigned long hostSeq, lastSeq;
	short scrapCnt;
	unsigned long lastCnt;
	unsigned long key;

	/* Throttle: at most every 30 ticks (~0.5s) */
	now = TickCount();
	if (now - g->lastClipTicks < 30) return;
	g->lastClipTicks = now;

	appId = *(short *)kCurApRefNum;

	/* --- Host -> Mac --- */
	reg_command(g->regBase, kClipSeqNo);
	hostSeq = reg_get(g->regBase, 0);
	key = (unsigned long)appId * 2;
	lastSeq = kv_get(g->regBase, key);

	if (hostSeq != lastSeq)
	{
		dbg_log2(g->regBase, "Sync: host->mac seq %lx != %lx", hostSeq, lastSeq);
		if (ImportHostToMac(g->regBase) < 0)
			dbg_log(g->regBase, "clip: import error (ignored)");
		kv_set(g->regBase, key, hostSeq);
		/* Prevent feedback: update mac->host scrapCount too */
		kv_set(g->regBase, (unsigned long)appId * 2 + 1, (unsigned long)*(short *)kScrapCount);
	}

	/* --- Mac -> Host --- */
	scrapCnt = *(short *)kScrapCount;
	key = (unsigned long)appId * 2 + 1;
	lastCnt = kv_get(g->regBase, key);

	if ((unsigned long)scrapCnt != lastCnt)
	{
		dbg_log2(g->regBase, "Sync: mac->host cnt %ld != %ld", (unsigned long)scrapCnt, lastCnt);
		if (ExportMacToHost(g->regBase) < 0)
			dbg_log(g->regBase, "clip: export error (ignored)");
		kv_set(g->regBase, key, (unsigned long)scrapCnt);
	}
}
