/*
	PROGWND1.i
	Copyright (C) 2007 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	PROGress WiNDow Part 1
*/


/* -- error reporting -- */

#define EmptyStrConst ((StringPtr)"\000")

#define MouseOrKeyMask \
	(mDownMask | mUpMask | keyDownMask | keyUpMask | autoKeyMask)

#define rAboutAlert 128
#define rTextAlert 129

LOCALPROC MyAlertFromIdPStr(SInt16 alertID, StringPtr s)
{
	ParamText(s, EmptyStrConst, EmptyStrConst, EmptyStrConst);
	FlushEvents(MouseOrKeyMask, 0);
	ResetAlertStage();
	(void) Alert(alertID, NULL);
}

LOCALPROC MyAlertFromPStr(StringPtr s)
{
	MyAlertFromIdPStr(rTextAlert, s);
}

GLOBALPROC MyAlertFromCStr(char *s)
{
	Str255 t;

	PStrFromCStr(t, s);
	MyAlertFromPStr(t);
}

LOCALPROC GetVersLongStr(StringPtr s)
{
	ui3p p;
	Handle h = GetResource('vers', 1);
	if (NULL == h) {
		PStrClear(s);
	} else {
		HLock(h);
		p = (ui3p)*h;
		p += 6;
		p += *p + 1;
		PStrFromPtr((MyPtr)p + 1, *p, s);
		HUnlock(h);
	}
}

LOCALPROC ShowAboutMessage()
{
	MyPStr s;

	GetVersLongStr(s);
	MyAlertFromIdPStr(rAboutAlert, s);
}

LOCALVAR blnr AbortRequested = falseblnr;

#ifndef DbgCheck
#define DbgCheck 0
#endif

/* -- main program window management and event handling -- */

LOCALVAR WindowRef MyMainWind = NULL;

#define MainWindHeight 25
#define MainWindWidth 200

LOCALVAR blnr ProgramDone = falseblnr;

LOCALVAR blnr gBackgroundFlag = falseblnr;
LOCALVAR long NextCheckTick;
LOCALVAR long NextCheckEventTick;

#ifndef WantRealInputFile
#define WantRealInputFile 0
#endif

#ifndef AutoQuitIfStartUpFile
#define AutoQuitIfStartUpFile WantRealInputFile
#endif

#if AutoQuitIfStartUpFile
LOCALVAR blnr PastStartUpFiles = falseblnr;
LOCALVAR blnr SawStartUpFile = falseblnr;
#endif

#if WantRealInputFile
struct input_r { /* 16 bytes */
	MyDir_R d;
	ui4b filler1;
	Handle Name;
	ui5b filler2;
};
typedef struct input_r input_r;
#endif

#if WantRealInputFile
LOCALPROC DisposeInputR(input_r *r)
{
	DisposeHandle(r->Name);
}
#endif

#if WantRealInputFile
LOCALFUNC tMyErr Create_input_r_v2(MyDir_R *d, ps3p s, input_r *r)
{
	tMyErr err;
	Handle Name;

	if (noErr == (err = PStr2Hand_v2(s, &Name))) {
		r->d = *d;
		r->Name = Name;
	}

	return err;
}
#endif

#if WantRealInputFile
LOCALVAR xbh_r InputA = xbh_ZapVal;
#endif

#if WantRealInputFile
LOCALFUNC tMyErr InitInputA()
{
	return xbh_Init_v2(0, &InputA);
}
#endif

#if WantRealInputFile
LOCALFUNC blnr PopFromInputA(input_r *r)
{
	return (noErr == xbh_PopToPtr_v2(&InputA,
		(MyPtr)r, sizeof(input_r)));
}
#endif

#if WantRealInputFile
LOCALPROC ClearInputA()
{
	input_r r;

	while (PopFromInputA(&r)) {
		DisposeInputR(&r);
	}
}
#endif

#if WantRealInputFile
LOCALPROC UnInitInputA()
{
	if (xbh_Initted(&InputA)) {
		ClearInputA();
		xbh_UnInit(&InputA);
	}
}
#endif

#if WantRealInputFile
LOCALFUNC tMyErr ProcessInputFile_v2(MyDir_R *d, ps3p s)
{
	tMyErr err;
	input_r r;

#if AutoQuitIfStartUpFile
	if (! PastStartUpFiles) {
		SawStartUpFile = trueblnr;
	}
#endif

	if (noErr == (err = Create_input_r_v2(d, s, &r))) {
		if (noErr != (err = xbh_AppendPtr_v2(&InputA,
			(MyPtr)&r, sizeof(input_r))))
		{
			DisposeInputR(&r);
		} else {
			/* ok */
		}
	}

	return err;
}
#endif

#if WantRealInputFile
LOCALVAR blnr GotInputCur = falseblnr;
LOCALVAR input_r InputCur;
#endif

#if WantRealInputFile
LOCALPROC CloseInputCur()
{
	if (GotInputCur) {
		DisposeInputR(&InputCur);
		GotInputCur = falseblnr;
	}
}
#endif

#if WantRealInputFile
LOCALPROC GetInputFile(MyDir_R *d, ps3p s)
{
	*d = InputCur.d;
	HandToPStr(InputCur.Name, s);
}
#endif

#if WantRealInputFile
LOCALFUNC tMyErr ProcessInputFileFSSpec_v2(FSSpec *spec)
{
	MyDir_R d;

	d.VRefNum = spec->vRefNum;
	d.DirId = spec->parID;

	return ProcessInputFile_v2(&d, spec->name);
}
#endif

#if WantRealInputFile
LOCALFUNC tMyErr ProcessInputFileNameNamevRef_v2(
	ConstStr255Param fileName, short vRefNum)
{
	tMyErr err;
	MyDir_R d;

	if (noErr == (err = MyDirFromWD_v2(vRefNum, &d)))
	if (noErr == (err = ProcessInputFile_v2(&d, (ps3p)fileName)))
	{
		/* ok */
	}

	return err;
}
#endif

#ifndef InputMayBeFolder
#define InputMayBeFolder 0
#endif

#ifndef InputMayBeFile
#define InputMayBeFile 1
#endif

#ifndef InputFileIsGeneric
#define InputFileIsGeneric 1
#endif

#if WantRealInputFile
LOCALPROC DoGetOldFile()
{
	tMyErr err;
	MyDir_R d;
	Str255 s;
#if ! InputMayBeFolder
#if InputFileIsGeneric
#define pfInputType NULL
#define nInputTypes (-1)
#else
	long fInputType = InputFileType;
#define pfInputType ((ConstSFTypeListPtr)&fInputType)
#define nInputTypes 1
#endif
#endif

#if AutoQuitIfStartUpFile
	PastStartUpFiles = trueblnr;
#endif

#if InputMayBeFolder
	err = MySelectFolder(&d, s);
#else
	err = MyFileGetOld_v2(nInputTypes, pfInputType,
		&d, s);
#endif
	if (kMyErrUsrAbort == err) {
	} else {
		if (noErr == err) {
			err = ProcessInputFile_v2(&d, s);
		}

		(void) CheckSysErr(err);
	}
}
#endif

#if WantRealInputFile
LOCALFUNC tMyErr MyAppFilesInit_v2()
{
	tMyErr err = noErr;
	short i;
	short message;
	short count;
	AppFile theFile;

	CountAppFiles(&message, &count);
	for (i = 1; i <= count; ++i) {
		GetAppFiles(i, &theFile);

		if (noErr != (err = ProcessInputFileNameNamevRef_v2(
			theFile.fName, theFile.vRefNum)))
		{
			goto error_exit;
		}

		ClrAppFiles(i);
	}

error_exit:
	return err;
}
#endif

#if WantRealInputFile
LOCALFUNC tMyErr ProcessInputFileFSSpecOrAlias_v2(FSSpec *spec)
{
	tMyErr err;
	Boolean isFolder;
	Boolean isAlias;

	if (noErr == (err = ResolveAliasFile(spec, true,
		&isFolder, &isAlias)))
	{
#if ! InputMayBeFolder
		if (isFolder) {
			err = paramErr;
		} else
#endif
#if ! InputMayBeFile
		if (! isFolder) {
			err = paramErr;
		} else
#endif
		{
			err = ProcessInputFileFSSpec_v2(spec);
		}
	}

	return err;
}
#endif

LOCALFUNC tMyErr GotRequiredParams_v2(const AppleEvent *theAppleEvent)
{
	tMyErr err;
	DescType typeCode;
	Size actualSize;

	/* Make sure there are no additional "required" parameters. */

	err = AEGetAttributePtr(theAppleEvent, keyMissedKeywordAttr,
		typeWildCard, &typeCode, NULL, 0, &actualSize);
	if (errAEDescNotFound == err) {
		/* no more required params */
		err = noErr;
	} else if (noErr == err) {
		/* missed required param */
		err = errAEEventNotHandled;
	} else {
		/* other error */
	}

	return err;
}

#if WantRealInputFile
#define openOnly 1
#define openPrint 2
#endif

#if WantRealInputFile
static pascal OSErr OpenOrPrintFiles(const AppleEvent *theAppleEvent,
	AppleEvent *reply, long aRefCon)
{
#pragma unused(reply, aRefCon)

	/*
		Adapted from IM VI: AppleEvent Manager:
			Handling Required AppleEvents
	*/
	tMyErr err;
	AEDescList docList;
	simr itemsInList;
	simr index;
	AEKeyword keywd;
	DescType typeCode;
	FSSpec myFSS;
	Size actualSize;

	/* put the direct parameter (a list of descriptors) into docList */
	if (noErr == (err = AEGetParamDesc(theAppleEvent, keyDirectObject,
		typeAEList, &docList)))
	{
		if (noErr == (err = GotRequiredParams_v2(theAppleEvent)))
			/* Check for missing required parameters */
		{
			if (noErr == (err = AECountItems(&docList, &itemsInList)))
			{
				for (index = 1; index <= itemsInList; ++index) {
					/*
						Get each descriptor from the list,
						get the alias record, open the file,
						maybe print it.
					*/
					if (noErr == (err = AEGetNthPtr(&docList, index,
						typeFSS, &keywd, &typeCode,
						(Ptr)&myFSS, sizeof(myFSS), &actualSize)))
					if (noErr == (err = ProcessInputFileFSSpec_v2(
						&myFSS /* , aRefCon == openPrint */)))
					{
						/* ok */
					}
					if (noErr != err) {
						goto label_fail;
					}
				}
label_fail:
				;
			}
		}
		err = CombineErr(err, AEDisposeDesc(&docList));
	}

	vCheckSysErr(err);

	return noErr;
}
#endif

static pascal OSErr DoOpenEvent(const AppleEvent *theAppleEvent,
	AppleEvent *reply, long aRefCon)
/*
	This is the alternative to getting an open document event
	on startup.
*/
{
#pragma unused(reply, aRefCon)
	tMyErr err;

#if AutoQuitIfStartUpFile
	PastStartUpFiles = trueblnr;
#endif
	err = GotRequiredParams_v2(theAppleEvent);

	vCheckSysErr(err);

	return noErr;
}

static pascal OSErr DoQuitEvent(const AppleEvent *theAppleEvent,
	AppleEvent *reply, long aRefCon)
{
#pragma unused(reply, aRefCon)
	tMyErr err;

	if (noErr == (err = GotRequiredParams_v2(theAppleEvent)))
	{
		ProgramDone = trueblnr;
	}

	vCheckSysErr(err);

	return noErr;
}

LOCALFUNC tMyErr MyInstallEventHandler(AEEventClass theAEEventClass,
	AEEventID theAEEventID, AEEventHandlerProcPtr p,
	long handlerRefcon, blnr isSysHandler)
{
	return AEInstallEventHandler(theAEEventClass,
		theAEEventID, p, handlerRefcon, isSysHandler);
}

LOCALFUNC tMyErr InstallAppleEventHandlers()
{
	tMyErr err;

	if (! HaveAppleEvtMgrAvail()) {
		err = noErr;
	} else {
		if (noErr == (err = AESetInteractionAllowed(
			kAEInteractWithLocal)))
		if (noErr == (err = MyInstallEventHandler(
			kCoreEventClass, kAEOpenApplication,
			DoOpenEvent, 0, false)))
#if WantRealInputFile
		if (noErr == (err = MyInstallEventHandler(
			kCoreEventClass, kAEOpenDocuments,
			OpenOrPrintFiles, openOnly, false)))
		if (noErr == (err = MyInstallEventHandler(
			kCoreEventClass, kAEPrintDocuments,
			OpenOrPrintFiles, openPrint, false)))
#endif
		if (noErr == (err = MyInstallEventHandler(
			kCoreEventClass, kAEQuitApplication,
			DoQuitEvent, 0, false)))
		{
		}
	}

	return err;
}


#if WantRealInputFile
static pascal OSErr GlobalTrackingHandler(short message,
	WindowRef pWindow, void *handlerRefCon, DragRef theDragRef)
{
#pragma unused(pWindow, handlerRefCon)
	RgnHandle hilightRgn;
	Rect Bounds;

	switch(message) {
		case kDragTrackingEnterWindow:
			hilightRgn = NewRgn();
			if (hilightRgn != NULL) {
				Bounds.left = 0;
				Bounds.top = 0;
				Bounds.right = MainWindWidth;
				Bounds.bottom = MainWindHeight;
				RectRgn(hilightRgn, &Bounds);
				ShowDragHilite(theDragRef, hilightRgn, true);
				DisposeRgn(hilightRgn);
			}
			break;
		case kDragTrackingLeaveWindow:
			HideDragHilite(theDragRef);
			break;
	}

	return noErr;

}
#endif

#if WantRealInputFile
static pascal OSErr GlobalReceiveHandler(WindowRef pWindow,
	void *handlerRefCon, DragRef theDragRef)
{
#pragma unused(pWindow, handlerRefCon)
	tMyErr err;
	unsigned short items;
	unsigned short index;
	DragItemRef theItem;
	Size SentSize;
	HFSFlavor r;

	if (noErr == (err = CountDragItems(theDragRef, &items))) {
		for (index = 1; index <= items; index++) {
			if (noErr == (err = GetDragItemReferenceNumber(theDragRef,
				index, &theItem)))
			if (noErr == (err = GetFlavorDataSize(theDragRef,
				theItem, kDragFlavorTypeHFS, &SentSize)))
			/*
				SentSize may be only big enough to hold the actual
				file name. So use '<=' instead of '=='.
			*/
			if (SentSize <= sizeof(HFSFlavor))
			if (noErr == (err = GetFlavorData(theDragRef, theItem,
				kDragFlavorTypeHFS, (Ptr)&r, &SentSize, 0)))
			if (noErr == (err = ProcessInputFileFSSpecOrAlias_v2(
				&r.fileSpec)))
			{
				/* ok */
			}
			if (noErr != err) {
				goto label_fail;
			}
		}

		{
			ProcessSerialNumber currentProcess = {0, kCurrentProcess};

			err = SetFrontProcess(&currentProcess);
		}
label_fail:
		;
	}

	vCheckSysErr(err);

	return noErr;
}
#endif

#if WantRealInputFile
LOCALVAR blnr gHaveDragMgr = falseblnr;
#endif

#if WantRealInputFile
LOCALPROC PrepareForDragging()
{
	if (HaveDragMgrAvail()) {
		if (noErr == InstallTrackingHandler(
			GlobalTrackingHandler, MyMainWind, nil))
		{
			if (noErr == InstallReceiveHandler(
				GlobalReceiveHandler, MyMainWind, nil))
			{
				gHaveDragMgr = trueblnr;
				return;
				/*
					RemoveReceiveHandler(
						GlobalReceiveHandler, MyMainWind);
				*/
			}
			RemoveTrackingHandler(GlobalTrackingHandler, MyMainWind);
		}
	}
}
#endif

#if WantRealInputFile
LOCALPROC UnPrepareForDragging()
{
	if (gHaveDragMgr) {
		RemoveReceiveHandler(GlobalReceiveHandler, MyMainWind);
		RemoveTrackingHandler(GlobalTrackingHandler, MyMainWind);
	}
}
#endif

#define appleMenu 1
#define fileMenu 2
#define EditMenu 3

#define chrEsc (unsigned char)27

LOCALFUNC Boolean EventIsAbort(EventRecord *theEvent)
{
	char theChar;
	Boolean CmdKeyDown;
	Boolean v = falseblnr;

	if ((keyDown == theEvent->what) || (autoKey == theEvent->what)) {
		theChar = theEvent->message & charCodeMask;
		CmdKeyDown = AndBits(theEvent->modifiers, cmdKey) != 0;
		if (CmdKeyDown && ('.' == theChar)) {
			v = trueblnr;
		} else if (chrEsc == theChar) {
			v = ((theEvent->message >> 8) & 255) != 71;
		} else {
			v = falseblnr;
		}
	}

	return v;
}

LOCALFUNC blnr EventPendingQuickCheck()
{
	EvQElPtr p = (EvQElPtr)GetEvQHdr()->qHead;

	while (p != NULL) {
		switch (p->evtQWhat) {
			case keyDown:
			case autoKey:
			case mouseDown:

			case updateEvt:
			case osEvt:
				/*
					these two aren't actually in this queue,
					and so won't be seen here
				*/
				return trueblnr;
				break;
			default:
				break;
		}
		p = (EvQElPtr)p->qLink;
	}
	return falseblnr;
}

LOCALPROC MyMainWindowUnInit()
{
	if (MyMainWind != NULL) {
		DisposeWindow(MyMainWind);
	}
}

/* *** DA utilities **** */

LOCALPROC MyDeskAccClose(WindowRef w)
{
	SelectWindow(w);
	SetPortWindowPort(w);
	CloseDeskAcc(GetWindowKind(w));
}

LOCALPROC CloseAWindow(WindowRef w)
/*
	Close a window which isn't ours.
*/
{
	if (GetWindowKind(w) < 0) {
		MyDeskAccClose(w);
	} else {
		DisposeWindow(w); /* get rid of it, whatever it is */
	}
}

LOCALPROC CloseAllExtraWindows()
/*
	assuming our window is gone, get
	rid of the rest of the windows,
	such as DAs.
*/
{
	while (FrontWindow() != NULL) {
		CloseAWindow(FrontWindow());
	}
}

LOCALPROC MyDeskAccOpen(StringPtr name)
{
	EventRecord theEvent;

	/* SetCursor(&(qd.arrow)); */
	(void) OpenDeskAcc(name);
	while (! EventAvail(everyEvent, &theEvent)) {
	}
}

LOCALPROC MyDeskAccMenuItem(short theitem)
{
	Str255 s;

	GetMenuItemText(GetMenuHandle(appleMenu), theitem, s);
	MyDeskAccOpen(s);
}

LOCALPROC DecodeSysMenus(long theMenuItem)
{
	short theMenu = HiWord(theMenuItem);
	short theItem = LoWord(theMenuItem);

	switch (theMenu) {
		case appleMenu:
			if (1 == theItem) {
				ShowAboutMessage();
			} else {
				MyDeskAccMenuItem(theItem);
			}
			break;
		case fileMenu:
			switch (theItem) {
				case 1:
#if WantRealInputFile
					DoGetOldFile();
					break;
				case 3:
#endif
					ProgramDone = trueblnr;
					break;
			}
			break;
	}
	HiliteMenu(0);
}

LOCALPROC DoUpdateMenus()
{
}

LOCALPROC MyMenusInit()
{
#define chrApple ((unsigned char)20)
	Str255 s;
	MenuRef myMenu;

	PStrFromChar(s, chrApple);
	myMenu = NewMenu(appleMenu, s);
	if (myMenu != NULL) {
		PStrFromCStr(s, "About ");
		PStrAppend(s, LMGetCurApName());
		AppendMenu(myMenu, s);
		PStrFromCStr(s, "(-");
		AppendMenu(myMenu, s);
		AppendResMenu(myMenu, 'DRVR');
		InsertMenu(myMenu, 0);
	}
	PStrFromCStr(s, "File");
	myMenu = NewMenu(fileMenu, s);
	if (myMenu != NULL) {
#if WantRealInputFile
		PStrFromCStr(s, "Open\311/O");
		AppendMenu(myMenu, s);
		PStrFromCStr(s, "(-");
		AppendMenu(myMenu, s);
#endif
		PStrFromCStr(s, "Quit/Q");
		AppendMenu(myMenu, s);
		InsertMenu(myMenu, 0);
	}
	DrawMenuBar();
}

/* *** event handling **** */

LOCALVAR EventRecord curevent;
LOCALVAR WindowRef MacEventWind;

LOCALPROC inMenuBarAction()
{
	DoUpdateMenus();
	DecodeSysMenus(MenuSelect(curevent.where));
}

LOCALPROC inSysWindowAction()
{
	SystemClick(&curevent, MacEventWind);
}

LOCALPROC inContentAction()
{
	if (MacEventWind == MyMainWind) {
		if (MacEventWind != FrontWindow()) {
			SelectWindow(MacEventWind);
		} else {
#if WantRealInputFile
			ProgressBar_InvertAll();
			while (Button()) {
			}
			ProgressBar_InvertAll();
			DoGetOldFile();
#endif
		}
	}
}

LOCALPROC inDragAction()
{
	Rect totalrect;

	if (MacEventWind == MyMainWind) {
		totalrect = qd.screenBits.bounds;
		totalrect.top = totalrect.top + 20; /* room for menu bar */
		InsetRect(&totalrect, 4, 4);
		DragWindow(MacEventWind, curevent.where, &totalrect);
	} else {
#if DbgCheck
		MyAlertFromCStr("Unexpected drag for unknown window");
#endif
	}
}

LOCALPROC ProcessMouseDown()
{
	short which_part;

	which_part = FindWindow(curevent.where, &MacEventWind);
	switch (which_part) {
		case inDesk:
			/* nothing */
			break;
		case inMenuBar:
			inMenuBarAction();
			break;
		case inSysWindow:
			inSysWindowAction();
			break;
		case inContent:
			inContentAction();
			break;
		case inGrow:
			/* nothing */
			break;
		case inDrag:
			inDragAction();
			break;
		case inZoomIn:
		case inZoomOut:
			/* nothing */
			break;
		case inGoAway:
			/* nothing */
			break;
	}
}

LOCALPROC ProcessCommandKey(char theChar)
{
	DoUpdateMenus();
	DecodeSysMenus(MenuKey(theChar));
}

LOCALPROC ProcessKeyDown()
{
	if (EventIsAbort(&curevent)) {
		AbortRequested = trueblnr;
	} else {
		char key;
		key = curevent.message & charCodeMask;
		if (0 != (curevent.modifiers & cmdKey)) { /* Command key down */
			if (keyDown == curevent.what) {
				ProcessCommandKey(key);
			}
		}
	}
}

LOCALPROC ProcessUpdateEvt()
{
	WindowRef MacEventWind;

	MacEventWind = (WindowRef)curevent.message;
	if (MacEventWind == MyMainWind) {
		BeginUpdate(MacEventWind);
		ProgressBar_Draw();
		EndUpdate(MacEventWind);
	} else {
#if DbgCheck
		MyAlertFromCStr("Unexpected update for unknown window");
#endif
	}
}

LOCALPROC ProcessDiskEvt()
{
	Point where;

	if (HiWord(curevent.message) != noErr) {
		SetPt(&where, 70, 50);
		/* SetCursor(&(qd.arrow)); */
		(void) DIBadMount(where, curevent.message);
	}
}

LOCALPROC ProcessHighLevelEvt()
{
	AEEventClass thisClass;

	thisClass = (AEEventClass)curevent.message;
	if (kCoreEventClass == thisClass) {
		OSErr err = AEProcessAppleEvent(&curevent);
		if (errAEEventNotHandled == err) {
			/* ignore */
		} else {
			vCheckSysErr(err);
		}
	} else {
		vCheckSysErr(errAENotAppleEvent);
	}
}

LOCALPROC ProcessMacEvent()
{
	switch (curevent.what) {
		case mouseDown:
			ProcessMouseDown();
			break;
		case keyDown:
		case autoKey:
			ProcessKeyDown();
			break;
		case updateEvt:
			ProcessUpdateEvt();
			break;
		case diskEvt:
			ProcessDiskEvt();
			break;
		case kHighLevelEvent:
			ProcessHighLevelEvt();
			break;
		case osEvt:
			if (AndBits(0xFF000000, curevent.message) == 0x01000000) {
				gBackgroundFlag = ! TestBit(curevent.message, 0);
				if (! gBackgroundFlag) {
					SetCursor(&(qd.arrow));
				}
			}
			break;
		default:
			/* ignore mouseUp, activateEvt, keyUp */
			break;

	}
}

LOCALPROC MyDoNextEvents(unsigned long sleep)
{
	UpdateProgressBar();
	do {
		if (HaveWaitNextEventAvail()) {
			if (WaitNextEvent(everyEvent, &curevent, sleep, NULL)) {
				ProcessMacEvent();
			}
		} else {
			SystemTask();
			if (GetNextEvent(everyEvent, &curevent)) {
				ProcessMacEvent();
			}
		}
		/*
			keep calling WaitNextEvent while event is
			available, but avoid calling it when
			nothing event is pending
		*/
	} while (EventAvail(everyEvent, &curevent) && (! AbortRequested));
}

LOCALPROC CheckAbort0_v2()
{
	blnr ShouldGetEvent;
	long CurrentTick = LMGetTicks();
	NextCheckTick = CurrentTick + 6;

	if (gBackgroundFlag) {
		ShouldGetEvent = trueblnr;
		/*
			while in background, relinquish processor
			ten times a second
		*/
	} else if (CurrentTick > NextCheckEventTick) {
		ShouldGetEvent = trueblnr;
		/*
			while in foreground, relinquish processor
			once a second at minimum
		*/
	} else {
		ShouldGetEvent = EventPendingQuickCheck();
	}
	if (ShouldGetEvent) {
		NextCheckEventTick = CurrentTick + 60;
		MyDoNextEvents(0);
	} else {
		UpdateProgressBar();
	}
}

GLOBALFUNC tMyErr CheckAbortRequested()
{
	if (LMGetTicks() >= NextCheckTick) {
		CheckAbort0_v2();
	}

	return AbortRequested ? kMyErrUsrAbort : noErr;
}

LOCALPROC ReportErrors()
{
	if ((SavedSysErr != noErr) || AbortRequested) {
		if (AbortRequested) {
			MyAlertFromCStr("Aborted!");
		} else if ((kMyErrReported == SavedSysErr)
			|| (kMyErrUsrCancel == SavedSysErr))
		{
			/* nothing more needed */
		} else if (SavedSysErr != noErr) {
			MyAlertFromCStr(GetTextForSavedSysErr_v2(SavedSysErr));
		}
		SavedSysErr = noErr;
		AbortRequested = falseblnr;
#if WantRealInputFile
		ClearInputA();
#endif
	}
}

LOCALPROC OneWindAppPreInit()
{
	MyMacHeapInit();
	MyMacToolBoxInit();
}

LOCALFUNC tMyErr SaferNewWindow(Ptr wStorage, const Rect *boundsRect,
	const Str255 title, Boolean visible, short theProc,
	WindowPtr behind, Boolean goAwayFlag, long refCon, WindowPtr *w)
{
	tMyErr err;
	WindowPtr w0;

	if (noErr == (err = MyMemoryCheckSpare())) {
		w0 = NewWindow(wStorage, boundsRect, title,
			visible, theProc, behind, goAwayFlag, refCon);
		if (NULL == w0) {
			err = kMyErrSysUnknown;
		} else {
			if (noErr == (err = MyMemoryCheckSpare())) {
				*w = w0;
			} else {
				if (NULL == wStorage) {
					DisposeWindow(w0);
				} else {
					CloseAWindow(w0);
				}
			}
		}
	}

	return err;
}

LOCALFUNC tMyErr OneWindAppInit_v2()
{
	tMyErr err;
	Rect tempRect;

	NextCheckTick = LMGetTicks();
	NextCheckEventTick = NextCheckTick;

#if WantRealInputFile
	if (noErr == (err = InitInputA()))
#endif
	if (noErr == (err = InstallAppleEventHandlers()))
	{
		MyMenusInit();

		tempRect = qd.screenBits.bounds;
		tempRect.left += ((qd.screenBits.bounds.right
			- qd.screenBits.bounds.left - MainWindWidth) >> 1);
		tempRect.right = tempRect.left + MainWindWidth;
		tempRect.top += 41;
		tempRect.bottom = tempRect.top + MainWindHeight;

		if (noErr == (err = SaferNewWindow(NULL, &tempRect,
			LMGetCurApName(), true, noGrowDocProc, (WindowRef)(-1),
			false, (long)NULL, &MyMainWind)))
		{
#if WantRealInputFile
			(void) CheckSysErr(MyAppFilesInit_v2());
			PrepareForDragging();
#endif
			tempRect.left = 0;
			tempRect.right = MainWindWidth;
			tempRect.top = 0;
			tempRect.bottom = MainWindHeight;
			if (noErr == (err = ProgressBar_Init_v2(
				MyMainWind, &tempRect)))
			{
				/* ok */
			}
		}
	}

	return err;
}

LOCALPROC OneWindAppUnInit()
{
	ReportErrors();
#if WantRealInputFile
	UnPrepareForDragging();
	CloseInputCur();
	UnInitInputA();
#endif
	ProgressBar_UnInit();
	MyMainWindowUnInit();
	CloseAllExtraWindows();
}

#if WantRealInputFile
LOCALPROC WaitForInput()
{
	while ((! GotInputCur) && (! ProgramDone)) {
		ReportErrors();
		if (PopFromInputA(&InputCur)) {
			GotInputCur = trueblnr;
		} else
#if AutoQuitIfStartUpFile
		if (SawStartUpFile) {
			ProgramDone = trueblnr;
		} else
#endif
		{
			MyDoNextEvents(5 * 60 * 60);
		}
	}
}
#endif
