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
		/* Color: 32-bit GWorld */
		GWorldPtr    gw;
		PixMapHandle pm;
		CGrafPtr     savePort;
		GDHandle     saveDevice;
		QDErr        err;

		err = NewGWorld(&gw, 32, &picFrame, NULL, NULL, 0);
		if (err != noErr)
		{
			dbg_log1(regBase, "pict: NewGWorld err=%d", (int)err);
			DisposHandle(h);
			return;
		}

		pm = GetGWorldPixMap(gw);
		if (!LockPixels(pm))
		{
			DisposeGWorld(gw);
			DisposHandle(h);
			return;
		}

		GetGWorld(&savePort, &saveDevice);
		SetGWorld(gw, NULL);

		/* --- Pass 0: white background --- */
		BackColor(whiteColor);
		ForeColor(blackColor);
		EraseRect(&picFrame);
		DrawPicture((PicHandle)h, &picFrame);

		/*
			Send PixMap pointer.  The host reads the struct from
			guest RAM: baseAddr, rowBytes (with 0x8000 flag),
			bounds, and pixelSize at offset +34.
			StripAddress needed for 24-bit addressing Macs.
		*/
		reg_set(regBase, 0, (unsigned long)StripAddress((Ptr)(*pm)));
		reg_set(regBase, 1, 0);
		reg_command(regBase, kPictExport);

		/* --- Pass 1: black background --- */
		BackColor(blackColor);
		EraseRect(&picFrame);
		BackColor(whiteColor);
		DrawPicture((PicHandle)h, &picFrame);

		reg_set(regBase, 0, (unsigned long)StripAddress((Ptr)(*pm)));
		reg_set(regBase, 1, 1);
		reg_command(regBase, kPictExport);

		SetGWorld(savePort, saveDevice);
		UnlockPixels(pm);
		DisposeGWorld(gw);
	}

	DisposHandle(h);
}

/* ---- Host -> Guest: PICT import ---- */

/*
	Import host clipboard image to Mac desk scrap as PICT.
	Queries host for image presence + dimensions, allocates
	an offscreen buffer at screen depth, receives pixels,
	then uses OpenPicture/CopyBits/ClosePicture to create
	a valid PICT and puts it on the desk scrap.
*/
void ImportPictFromHost(char *regBase)
{
	unsigned long hasImg, width, height;
	short         depth;
	Rect          r;

	/* Ask host if it has an image */
	reg_command(regBase, kPictHasImage);
	hasImg = reg_get(regBase, 0);
	if (!hasImg) return;
	width  = reg_get(regBase, 1);
	height = reg_get(regBase, 2);
	if (width == 0 || height == 0 || width > 4096 || height > 4096)
		return;

	depth = ScreenDepth();
	SetRect(&r, 0, 0, (short)width, (short)height);

	if (depth == 1)
	{
		/* 1-bit: plain BitMap + GrafPort */
		BitMap    offBits;
		GrafPort  offPort;
		GrafPtr   savePort;
		PicHandle pic;
		short     rowBytes;
		long      bufSize;
		Ptr       bits;

		rowBytes = ((width + 15) / 16) * 2;
		bufSize  = (long)rowBytes * height;
		bits = NewPtr(bufSize);
		if (bits == NULL)
		{
			dbg_log(regBase, "pict: import alloc failed");
			return;
		}

		/* Tell host to fill our buffer with 1-bit pixels */
		reg_set(regBase, 0, (unsigned long)bits);
		reg_set(regBase, 1, (unsigned long)rowBytes);
		reg_set(regBase, 2, 1);           /* depth */
		reg_set(regBase, 3, width);
		reg_set(regBase, 4, height);
		reg_command(regBase, kPictImport);

		if (reg_result(regBase) != 0)
		{
			dbg_log(regBase, "pict: import host error");
			DisposPtr(bits);
			return;
		}

		offBits.baseAddr = bits;
		offBits.rowBytes = rowBytes;
		offBits.bounds   = r;

		/* Create PICT by recording a CopyBits */
		GetPort(&savePort);
		OpenPort(&offPort);
		SetPortBits(&offBits);
		offPort.portRect = r;
		RectRgn(offPort.visRgn, &r);
		RectRgn(offPort.clipRgn, &r);

		pic = OpenPicture(&r);
		CopyBits(&offBits, &offPort.portBits, &r, &r, srcCopy, NULL);
		ClosePicture();

		SetPort(savePort);
		ClosePort(&offPort);
		DisposPtr(bits);

		if (pic != NULL && GetHandleSize((Handle)pic) > 10)
		{
			ZeroScrap();
			HLock((Handle)pic);
			PutScrap(GetHandleSize((Handle)pic), 'PICT', *pic);
			HUnlock((Handle)pic);
			KillPicture(pic);
		}
	}
	else
	{
		/* 32-bit: GWorld */
		GWorldPtr    gw;
		PixMapHandle pm;
		CGrafPtr     savePort;
		GDHandle     saveDevice;
		PicHandle    pic;
		QDErr        err;
		Ptr          baseAddr;
		long         rb;

		err = NewGWorld(&gw, 32, &r, NULL, NULL, 0);
		if (err != noErr)
		{
			dbg_log1(regBase, "pict: import NewGWorld err=%d", (int)err);
			return;
		}

		pm = GetGWorldPixMap(gw);
		if (!LockPixels(pm))
		{
			DisposeGWorld(gw);
			return;
		}

		baseAddr = GetPixBaseAddr(pm);
		rb = (**pm).rowBytes & 0x3FFF;

		/* Tell host to fill the PixMap buffer with XRGB pixels */
		reg_set(regBase, 0, (unsigned long)StripAddress(baseAddr));
		reg_set(regBase, 1, (unsigned long)rb);
		reg_set(regBase, 2, 32);
		reg_set(regBase, 3, width);
		reg_set(regBase, 4, height);
		reg_command(regBase, kPictImport);

		if (reg_result(regBase) != 0)
		{
			dbg_log(regBase, "pict: import host error (32-bit)");
			UnlockPixels(pm);
			DisposeGWorld(gw);
			return;
		}

		/* Create PICT by recording a CopyBits from the GWorld */
		GetGWorld(&savePort, &saveDevice);
		SetGWorld(gw, NULL);

		pic = OpenPicture(&r);
		CopyBits((BitMap *)*pm,
				 (BitMap *)*pm,
				 &r, &r, srcCopy, NULL);
		ClosePicture();

		SetGWorld(savePort, saveDevice);
		UnlockPixels(pm);
		DisposeGWorld(gw);

		if (pic != NULL && GetHandleSize((Handle)pic) > 10)
		{
			ZeroScrap();
			HLock((Handle)pic);
			PutScrap(GetHandleSize((Handle)pic), 'PICT', *pic);
			HUnlock((Handle)pic);
			KillPicture(pic);
		}
	}
}
