/*
	ClipIn.c

	Copyright (C) 2006 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/* based upon Apple's sample DA */

#include <types.h>
#include <osutils.h>
#include <memory.h>
#include <devices.h>
#include <events.h>
#include <quickdraw.h>
#include <fonts.h>
#include <windows.h>
#include <files.h>
#include <errors.h>
#include <toolutils.h>
#include <packages.h>
#include <Desk.h>
#include <Scrap.h>


typedef unsigned long ui5b;
typedef unsigned short ui4b;
typedef unsigned char ui3b;

typedef long si5b;
typedef short si4b;
typedef char si3b;

#define blnr int
#define trueblnr 1
#define falseblnr 0

#define UseParamBuffersExtension 1
#define UseHostClipExchangeExtension 1

#include "ExtnGlue.i"

static XtnsDR xdr;

static void DoTheTask(void)
{
	XtnsDR xdr;

	if (noErr == InitExtensions(&xdr)) {
		ui4b Pbuf_No;
		if (noErr == HTCImport(&xdr, &Pbuf_No)) {
			ui5b L;
			if (noErr == PBufGetSize(&xdr, Pbuf_No, &L)) {
				Handle h = NewHandle(L);
				if (h != NULL) {
					if (noErr == PbufTransfer(&xdr, *h,
						Pbuf_No, 0, L, falseblnr))
					{
						if (noErr == ZeroScrap()) {
							HLock(h);
							(void) PutScrap(L, 'TEXT', (Ptr)*h);
							HUnlock(h);
						}
					}
					DisposHandle(h);
				}
			}
			(void) PbufDispose(&xdr, Pbuf_No);
		}
	}
}


/* This structure type holds the global variables used by this desk accessory */

typedef struct {
	Boolean GotActivate;
	Boolean GotUpdate;
	Boolean RunCounter;
} Globals;


/*
	Display the contents of the window.
	The current port is assumed to be set to the window.
*/
static void drawWindow(Handle CtlStorage)
{
#pragma unused(CtlStorage)
	TextMode(srcCopy);
	TextFont(monaco);
	TextSize(9);

	MoveTo(6, 12);
	DrawString("\pTransfer");
}

static void doCtlEvent(EventRecord *theEvent, DCtlPtr dCtl)
{
	WindowPtr myWindow;

	switch (theEvent->what) {
		case updateEvt:
			myWindow = (WindowPtr) theEvent->message;
			SetPort(myWindow);
			BeginUpdate(myWindow);
			if (myWindow == dCtl->dCtlWindow) {
				drawWindow(dCtl->dCtlStorage);
				((Globals *)(*dCtl->dCtlStorage))->GotUpdate = true;
			}
			EndUpdate(myWindow);
			break;
		case activateEvt:
			if ((theEvent->modifiers & activeFlag) != 0) {
				((Globals *)(*dCtl->dCtlStorage))->GotActivate = true;
			}
			break;
	}
}

static void doPeriodic(DCtlPtr dCtl)
{
	Handle h = dCtl->dCtlStorage;
	((Globals *)(*h))->RunCounter += 1;
	if ((((Globals *)(*h))->GotUpdate &&
		((Globals *)(*h))->GotActivate &&
		(((Globals *)(*h))->RunCounter >= 3))
		||
		(((Globals *)(*h))->RunCounter >= 30))
	{
		DoTheTask();
		CloseDriver(dCtl->dCtlRefNum);
		/* CloseDeskAcc (which is right? seen both used) */
	}
}


pascal short DRVRControl(CntrlParam *ctlPB, DCtlPtr dCtl)
{
	/*
		The current grafPort is saved & restored by the Desk Manager
	*/
	if (dCtl->dCtlWindow != nil) {
		switch (ctlPB->csCode) {
			case accEvent: /* accEvent */
				doCtlEvent(*((EventRecord **) &ctlPB->csParam[0]), dCtl);
				break;

			case accRun: /* periodicEvent */
				doPeriodic(dCtl);
				break;

			default:
				break;
		}
	}
	return 0;
}

pascal short DRVRPrime(CntrlParam *ctlPB, DCtlPtr dCtl)
{
#pragma unused (ctlPB, dCtl)
	return noErr; /* Not used in this desk accessory */
}

pascal short DRVRStatus(CntrlParam *ctlPB, DCtlPtr dCtl)
{
#pragma unused (ctlPB, dCtl)
	return noErr; /* Not used in this desk accessory */
}

pascal short DRVRClose(char *ctlPB, DCtlPtr dCtl)
{ /* Save & Restore current grafPort? */
#pragma unused (ctlPB)
	WindowPtr window;

	window = (WindowPtr) dCtl->dCtlWindow;
	if (window != nil) {
		dCtl->dCtlWindow = nil;
		DisposHandle(dCtl->dCtlStorage);
		DisposeWindow(window);
	}

	return 0;
}

pascal short DRVROpen(CntrlParam *ctlPB, DCtlPtr dCtl)
{
#pragma unused (ctlPB)
	GrafPtr savePort;
	WindowPeek myWindow;
	Rect boundsRect;
	short err = noErr;

	if (dCtl->dCtlWindow != nil) {
		/*
			If the windowPtr is non-nil, we already have a window open.
			This desk accessory ignores multiple opens.
		*/
	} else {
		GetPort(&savePort);

		/*
			Get a handle to some storage that will hold our pseudo-global
			variables.  Save the handle in a location accessible by
			all the driver routines.
		*/
		dCtl->dCtlStorage = NewHandle(sizeof(Globals));
		if (dCtl->dCtlStorage != nil) {
			((Globals *)(*dCtl->dCtlStorage))->GotActivate = false;
			((Globals *)(*dCtl->dCtlStorage))->GotUpdate = false;
			((Globals *)(*dCtl->dCtlStorage))->RunCounter = 0;

			SetRect(&boundsRect, 10, 42, 74, 58);
			myWindow = (WindowPeek)NewWindow(nil /* wStorage */,
				&boundsRect,
				"\p",
				false /* visible */,
				plainDBox /* theProc */,
				(WindowPtr) -1 /* behind */,
				true /* goAwayFlag */,
				0x0 /* refCon */);
			if (myWindow != nil) {
				/*
					Set windowKind to the DA refNum, which is negative.
				*/
				myWindow->windowKind = dCtl->dCtlRefNum;
				/*
					Store the windowPtr in the Device Control Entry
				*/
				dCtl->dCtlWindow = (WindowPtr)myWindow;
			}
		}

		SetPort(savePort);
	}

	return err;
}
