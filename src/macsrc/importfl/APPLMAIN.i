/*
	APPLMAIN.c
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
	APPLication MAIN
*/


LOCALVAR blnr CapturingInserts = falseblnr;
LOCALVAR ui5r SaveCallBack;
LOCALVAR ui4r SaveRawMode;

LOCALPROC CapturingInsertsOn(void)
{
	tMyErr err;

	if (! CapturingInserts) {
		if  (noErr == (err = DiskGetCallBack_v2(&SaveCallBack))) {
			if  (noErr == (err = DiskSetCallBack_v2(0))) {
				if  (noErr == (err = DiskGetRawMode_v2(&SaveRawMode))) {
					if  (noErr == (err = DiskSetRawMode_v2(1))) {
						CapturingInserts = trueblnr;
					}
					if (noErr != err) {
						(void) DiskSetRawMode_v2(SaveRawMode);
							/*
								ignore any error, since already got one
							*/
					}
				}
			}
			if (noErr != err) {
				(void) DiskSetCallBack_v2(SaveCallBack);
					/* ignore any error, since already got one */
			}
		}

		(void) CheckSysErr(err);
	}
}

LOCALPROC CapturingInsertsOff(void)
{
	tMyErr err;

	if (CapturingInserts) {
		err = DiskSetRawMode_v2(SaveRawMode);
		err = CombineErr(err, DiskSetCallBack_v2(SaveCallBack));

		(void) CheckSysErr(err);

		CapturingInserts = falseblnr;
	}
}

LOCALFUNC blnr TryCaptureInsert(tDrive *r)
{
	OSErr err = DiskNextPendingInsert(r);
	blnr v = falseblnr;
	if (noErr == err) {
		v = trueblnr;
	} else if (err == (OSErr)0xFFC8) {
		/* nothing yet */
	} else {
		vCheckSysErr(err);
	}
	return v;
}

LOCALVAR blnr ReadyToImport = falseblnr;

LOCALPROC CheckStateAfterEvents(void)
{
	if (ReadyToImport != (! gBackgroundFlag)) {
		ReadyToImport = ! ReadyToImport;
		if (ReadyToImport) {
			(void) CheckSysErr(
				ProgressBar_SetStage_v2("Ready to import\311", 0));
			CapturingInsertsOn();
		} else {
			CapturingInsertsOff();
			(void) CheckSysErr(ProgressBar_SetStage_v2("", 0));
		}
	}
}


LOCALPROC TryImportHostFile(void)
{
	tMyErr err;
	tDrive InsertVol;
	MyPStr s;
	ui5r L;
	MyDir_R d;
	short refNum;

	if (TryCaptureInsert(&InsertVol)) {
		if (noErr == (err = DiskGetName2Pstr_v2(InsertVol, s)))
		if (noErr == (err = DiskGetSize_v2(InsertVol, &L)))
		if (noErr == (err =
			ProgressBar_SetStage_v2("Select destination\311", 0)))
		{
			UpdateProgressBar();
			err = CreateOpenNewFile_v2((StringPtr)"\pOutput File", s,
				'SITx' /* creator (ExportFl) */,
				'BINA' /* type */,
				&d, s, &refNum);
			if (noErr == err) {
				if (noErr == (err =
					ProgressBar_SetStage_v2(
						"Importing, type command period to abort", L)))
				if (noErr == (err =
					MyOpenFileSetEOF_v2(refNum, L)))
				if (noErr == (err =
					WriteFromVolToFile_v2(refNum, InsertVol, 0, L)))
				{
					/* ok */
				}
				err = MyCloseNewFile_v3(refNum, &d, s, err);
			}
		}
		err = CombineErr(err, DiskEject_v2(InsertVol));

		err = CombineErr(err, ProgressBar_SetStage_v2((noErr == err)
			? "Done, ready to import another\311"
				: "Aborted, ready to import another\311",
			0));
		if (kMyErrUsrCancel != err) {
			(void) CheckSysErr(err);
		}
	}
}

LOCALPROC ProgramMain(void)
{
	while (! ProgramDone) {
		UpdateProgressBar();
		ReportErrors();
		MyDoNextEvents(CapturingInserts ? 5
			: 5 * 60 * 60);
		CheckStateAfterEvents();
		if (CapturingInserts) {
			TryImportHostFile();
		}
	}
}

LOCALPROC ProgramZapVars(void)
{
}

LOCALPROC ProgramPreInit(void)
{
	OneWindAppPreInit();
}

LOCALFUNC blnr ImportExtnInit(void)
{
	ui4r version;
	ui5r features;
	blnr IsOk = falseblnr;

	if (! (noErr == HaveDiskExtenstion_v2())) {
		MyAlertFromCStr(
			"The Mini vMac extension mechanism is not available.");
	} else
	if (! CheckSysErr(DiskVersion_v2(&version))) {
	} else
	if (version < 2) {
		MyAlertFromCStr("The Disk Extension version is too old.");
	} else
	if (! CheckSysErr(DiskFeatures_v2(&features))) {
	} else
	if (0 == (features & ((ui5r)1 << kFeatureCmndDisk_RawMode))) {
		MyAlertFromCStr("Raw mode access feature is not available.");
	} else
	if (0 == (features & ((ui5r)1 << kFeatureCmndDisk_GetName))) {
		MyAlertFromCStr("Get Disk name feature is not available.");
	} else
	{
		IsOk = trueblnr;
	}

	return IsOk;
}

LOCALFUNC blnr ProgramInit(void)
{
	if (CheckSysErr(MyMemory_Init_v2()))
	if (ImportExtnInit())
	if (CheckSysErr(OneWindAppInit_v2()))
	{
		return trueblnr;
	}
	return falseblnr;
}

LOCALPROC ProgramUnInit(void)
{
	CapturingInsertsOff();
	OneWindAppUnInit();
	MyMemory_UnInit();
}

int main(void)
{
	ProgramZapVars();
	ProgramPreInit();
	if (ProgramInit()) {
		ProgramMain();
	}
	ProgramUnInit();

	return 0;
}
