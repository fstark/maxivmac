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
	short scrapCnt;
	unsigned long hostSeq;

	/* Throttle: at most every 30 ticks (~0.5s) */
	now = TickCount();
	if (now - g->lastClipTicks < 30) return;
	g->lastClipTicks = now;

	scrapCnt = *(short *)kScrapCount;

	/* --- Mac changed? Export to host --- */
	if (scrapCnt != g->lastScrapCnt)
	{
		dbg_log2(g->regBase, "Sync: mac changed cnt %ld->%ld, exporting",
				 (long)g->lastScrapCnt, (long)scrapCnt);
		ExportMacToHost(g->regBase);
		ExportPictToHost(g->regBase);
		reg_command(g->regBase, kClipCommit);
		g->lastHostSeq  = reg_get(g->regBase, 0);
		g->lastScrapCnt = scrapCnt;
		dbg_log1(g->regBase, "Sync: commit -> hostSeq=%lu",
				 g->lastHostSeq);
		return;
	}

	/* --- Host changed? Import to Mac --- */
	reg_command(g->regBase, kClipSeqNo);
	hostSeq = reg_get(g->regBase, 0);

	if (hostSeq != g->lastHostSeq)
	{
		dbg_log2(g->regBase, "Sync: host changed seq %lu->%lu, importing",
				 g->lastHostSeq, hostSeq);
		ImportHostToMac(g->regBase);
		ImportPictFromHost(g->regBase);
		g->lastHostSeq  = hostSeq;
		g->lastScrapCnt = *(short *)kScrapCount;
		dbg_log1(g->regBase, "Sync: imported, scrapCnt now %ld",
				 (long)g->lastScrapCnt);
	}
}
