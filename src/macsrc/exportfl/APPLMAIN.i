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

LOCALFUNC tMyErr ExportFromFile2HostFile(short refNum, ui5r L,
	ui4r Name)
{
	tMyErr err;
	ui4r InsertVol;

	if (noErr == (err = ProgressBar_SetStage_v2(
		"Making the new file\311", 0)))
	if (noErr == (err = HostVolInsertNew_v2(L, Name, &InsertVol)))
	{
		if (noErr == (err = ProgressBar_SetStage_v2(
			"Running, type command-period to abort\311", L)))
		{
			err = WriteFromFileToVol_v2(refNum, InsertVol, 0, L);
		}
		err = NewDiskEject_v2(InsertVol, err);
	}

	return err;
}

LOCALFUNC tMyErr DoFilter2(void)
{
	tMyErr err;
	MyDir_R d;
	Str255 s;
	short refNum;
	ui4r Name;
	uimr n;

	GetInputFile(&d, s);
	if (noErr == (err = MyOpenOldFileRead_v2(&d, s, &refNum))) {
		if (noErr == (err = MyOpenFileGetEOF_v2(refNum, &n)))
		if (noErr == (err = PStr2Pbuf_v2(s, &Name)))
		if (noErr == (err = ExportFromFile2HostFile(refNum,
			n, Name)))
		{
		}
		err = CombineErr(err, MyCloseFile_v2(refNum));
	}

	return err;
}

LOCALPROC ProgramMain(void)
{
	(void) ProgressBar_SetStage_v2("Ready to export file\311", 0);
label_1:
	WaitForInput();
	if (GotInputCur) {
		if (! CheckSysErr(DoFilter2())) {
			ClearInputA();
			(void) ProgressBar_SetStage_v2(
				"Aborted, ready for another file\311", 0);
		} else {
			(void) ProgressBar_SetStage_v2(
				"Done, ready for another file\311", 0);
		}
		CloseInputCur();
		goto label_1;
	}
}

LOCALPROC ProgramZapVars(void)
{
}

LOCALPROC ProgramPreInit(void)
{
	OneWindAppPreInit();
}

LOCALFUNC blnr ExportExtnInit(void)
{
	ui4r version;
	ui5r features;
	blnr IsOk = falseblnr;

	if (! (noErr == HaveDiskExtenstion_v2())) {
		MyAlertFromCStr(
			"The Mini vMac extension mechanism is not available.");
	} else if (! CheckSysErr(DiskVersion_v2(&version))) {
	} else if (version < 2) {
		MyAlertFromCStr("The Disk Extension version is too old.");
	} else if (! CheckSysErr(DiskFeatures_v2(&features))) {
	} else if (0 == (features & ((ui5r)1 << kFeatureCmndDisk_New))) {
		MyAlertFromCStr(
			"The new file creation feature is not available.");
	} else if (0 == (features & ((ui5r)1 << kFeatureCmndDisk_RawMode)))
	{
		MyAlertFromCStr("Raw mode access feature is not available.");
	} else {
		IsOk = trueblnr;
	}

	return IsOk;
}

LOCALFUNC blnr ProgramInit(void)
{
	if (CheckSysErr(MyMemory_Init_v2()))
	if (ExportExtnInit())
	if (CheckSysErr(OneWindAppInit_v2()))
	{
		return trueblnr;
	}
	return falseblnr;
}

LOCALPROC ProgramUnInit(void)
{
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
