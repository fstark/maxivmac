/*
	maxivmac INIT — pict.c
	PICT clipboard export/import.
	Renders PICT through QuickDraw into an offscreen buffer
	and sends raw pixels to the host for PNG conversion.
*/

#include "defs.h"

/* ---- helpers ---- */

/*
	Return pixel depth of main screen.
	Returns 1 on compact Macs (no Color QuickDraw).
*/
static short ScreenDepth(void)
{
	long qdVersion;
	GDHandle mainDev;

	if (Gestalt(gestaltQuickdrawVersion, &qdVersion) != noErr)
		return 1;
	if (qdVersion < gestalt8BitQD)
		return 1;

	mainDev = GetMainDevice();
	if (mainDev == NULL) return 1;
	return (**(**mainDev).gdPMap).pixelSize;
}

/* ---- Guest -> Host: PICT export ---- */

/*
	Export PICT scrap to host clipboard as rendered pixels.
	Two-pass rendering (white bg, black bg) for alpha detection.
	The host composites the two passes and encodes PNG.
	For depth == 1: uses plain BitMap + GrafPort.
	For depth > 1: uses 32-bit GWorld.  (Phase 6 adds this path.)
*/
void ExportPictToHost(char *regBase)
{
	Handle   h;
	long     offset, length;
	Rect     picFrame;
	short    depth;

	/* Get PICT data from desk scrap */
	h = NewHandle(0);
	if (h == NULL) return;

	length = GetScrap(h, 'PICT', &offset);
	if (length <= 0)
	{
		DisposHandle(h);
		return;
	}

	/*
		picFrame is at offset 2 in the PICT data.
		Bytes 0-1 are the picture size (word, may be inaccurate
		for PICT2), bytes 2-9 are the bounding Rect.
	*/
	picFrame = *(Rect *)(*h + 2);

	depth = ScreenDepth();

	if (depth == 1)
	{
		/* 1-bit: offscreen BitMap + temporary GrafPort */
		GrafPort offPort;
		BitMap   offBits;
		GrafPtr  savePort;
		short    rowBytes;
		long     bufSize;
		Ptr      bits;

		rowBytes = ((picFrame.right - picFrame.left + 15) / 16) * 2;
		bufSize  = (long)rowBytes * (picFrame.bottom - picFrame.top);
		bits     = NewPtr(bufSize);
		if (bits == NULL)
		{
			dbg_log(regBase, "pict: export alloc failed");
			DisposHandle(h);
			return;
		}

		offBits.baseAddr = bits;
		offBits.rowBytes = rowBytes;
		offBits.bounds   = picFrame;

		GetPort(&savePort);
		OpenPort(&offPort);
		SetPortBits(&offBits);
		offPort.portRect = picFrame;
		RectRgn(offPort.visRgn, &picFrame);
		RectRgn(offPort.clipRgn, &picFrame);

		/* --- Pass 0: white background --- */
		EraseRect(&picFrame);                   /* fills white (default) */
		DrawPicture((PicHandle)h, &picFrame);
		reg_set(regBase, 0, (unsigned long)&offBits);
		reg_set(regBase, 1, 0);                 /* pass = white */
		reg_command(regBase, kPictExport);

		/* --- Pass 1: black background --- */
		FillRect(&picFrame, &qd.black);
		DrawPicture((PicHandle)h, &picFrame);
		reg_set(regBase, 0, (unsigned long)&offBits);
		reg_set(regBase, 1, 1);                 /* pass = black */
		reg_command(regBase, kPictExport);

		SetPort(savePort);
		ClosePort(&offPort);
		DisposPtr(bits);
	}
	else
	{
		/* Color path — implemented in Phase 6 */
		dbg_log(regBase, "pict: color export not yet implemented");
	}

	DisposHandle(h);
}

/* ---- Host -> Guest: PICT import ---- */

/*
	Import host clipboard image to Mac desk scrap as PICT.
	Stub — implemented in Phase 10.
*/
void ImportPictFromHost(char *regBase)
{
	(void)regBase;
}
