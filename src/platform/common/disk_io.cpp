#include "platform/common/disk_io.h"

FILE *Drives[NumDrives]; /* open disk image files */
char *DriveNames[NumDrives]; /* paths of open disk images */



void InitDrives()
{
	/*
		This isn't really needed, Drives[i] and DriveNames[i]
		need not have valid values when not vSonyIsInserted[i].
	*/
	DriveIndex i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = nullptr;
		DriveNames[i] = nullptr;
	}
}

 tMacErr vSonyTransfer(bool IsWrite, uint8_t * Buffer,
	DriveIndex Drive_No, uint32_t Sony_Start, uint32_t Sony_Count,
	uint32_t *Sony_ActCount)
{
	/*
		OSGLUxxx common:
		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = tMacErr::miscErr;
	FILE * refnum = Drives[Drive_No];
	uint32_t NewSony_Count = 0;

	if (fseek(refnum, Sony_Start, SEEK_SET) >= 0) {
		if (IsWrite) {
			NewSony_Count = fwrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = fread(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = tMacErr::noErr;
		}
	}

	if (nullptr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err; /*& figure out what really to return &*/
}

 tMacErr vSonyGetSize(DriveIndex Drive_No, uint32_t *Sony_Count)
{
	/*
		OSGLUxxx common:
		set Sony_Count to the size of disk image number Drive_No.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = tMacErr::miscErr;
	FILE * refnum = Drives[Drive_No];
	long v;

	if (fseek(refnum, 0, SEEK_END) >= 0) {
		v = ftell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = tMacErr::noErr;
		}
	}

	return err; /*& figure out what really to return &*/
}

static tMacErr vSonyEject0(DriveIndex Drive_No, bool deleteit)
{
	/*
		OSGLUxxx common:
		close disk image number Drive_No.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	FILE * refnum = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

	if (deleteit && DriveNames[Drive_No] != nullptr) {
		(void) remove(DriveNames[Drive_No]);
	}

	fclose(refnum);
	Drives[Drive_No] = nullptr; /* not really needed */

	free(DriveNames[Drive_No]);
	DriveNames[Drive_No] = nullptr;

	return tMacErr::noErr;
}

 tMacErr vSonyEject(DriveIndex Drive_No)
{
	return vSonyEject0(Drive_No, false);
}

 tMacErr vSonyEjectDelete(DriveIndex Drive_No)
{
	return vSonyEject0(Drive_No, true);
}

 tMacErr vSonyGetName(DriveIndex Drive_No, PbufIndex *r)
{
	char *path = DriveNames[Drive_No];
	if (nullptr == path) {
		return tMacErr::miscErr;
	}

	/* extract last path component */
	char *name = strrchr(path, '/');
	if (name != nullptr) {
		++name;
	} else {
		name = path;
	}

	uint32_t L;
	tMacErr err = UniCodeStrLength(name, &L);
	if (tMacErr::noErr != err) {
		return err;
	}

	PbufIndex t;
	err = PbufNew(L, &t);
	if (tMacErr::noErr != err) {
		return err;
	}

	UniCodeStr2MacRoman(name, static_cast<char *>(PbufDat[t]));
	*r = t;
	return tMacErr::noErr;
}

void UnInitDrives()
{
	DriveIndex i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

bool Sony_Insert0(FILE * refnum, bool locked,
	char *drivepath)
{
	/*
		OSGLUxxx common:
		Given reference to open file, mount it as a disk image file.
		if "locked", then mount it as a locked disk.
	*/

	DriveIndex Drive_No;
	bool IsOk = false;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(Localize(kStrTooManyImagesTitle), Localize(kStrTooManyImagesMessage),
			false);
	} else {
		/* printf("Sony_Insert0 %d\n", (int)Drive_No); */

		{
			Drives[Drive_No] = refnum;
			DriveNames[Drive_No] = (drivepath != nullptr)
				? strdup(drivepath) : nullptr;
			DiskInsertNotify(Drive_No, locked);

			IsOk = true;
		}
	}

	if (! IsOk) {
		fclose(refnum);
	}

	return IsOk;
}

bool Sony_Insert1(char *drivepath, bool silentfail)
{
	bool locked = false;
	/* printf("Sony_Insert1 %s\n", drivepath); */
	FILE * refnum = fopen(drivepath, "rb+");
	if (nullptr == refnum) {
		locked = true;
		refnum = fopen(drivepath, "rb");
	}
	if (nullptr == refnum) {
		if (! silentfail) {
			MacMsg(Localize(kStrOpenFailTitle), Localize(kStrOpenFailMessage), false);
		}
	} else {
		return Sony_Insert0(refnum, locked, drivepath);
	}
	return false;
}

static bool WriteZero(FILE *refnum, uint32_t L)
{
	uint8_t buffer[2048];
	memset(buffer, 0, sizeof(buffer));

	while (L > 0) {
		uint32_t i = (L > sizeof(buffer)) ? sizeof(buffer) : L;
		if (fwrite(buffer, 1, i, refnum) != i) {
			return false;
		}
		L -= i;
	}
	return true;
}

static void MakeNewDisk0(uint32_t L, char *drivepath)
{
	bool IsOk = false;
	FILE *refnum = fopen(drivepath, "wb+");
	if (nullptr == refnum) {
		MacMsg(Localize(kStrOpenFailTitle), Localize(kStrOpenFailMessage), false);
	} else {
		if (WriteZero(refnum, L)) {
			IsOk = Sony_Insert0(refnum, false, drivepath);
			refnum = nullptr;
		}
		if (refnum != nullptr) {
			fclose(refnum);
		}
		if (! IsOk) {
			(void) remove(drivepath);
		}
	}
}

void MakeNewDisk(uint32_t L, char *drivename)
{
	/* Create new disk in working directory / "out" subdirectory */
	char s[256];

	snprintf(s, sizeof(s), "out/%s", drivename);
	/* Ensure "out" directory exists */
	(void) mkdir("out", 0755);
	MakeNewDisk0(L, s);
	fprintf(stderr, "Exported file: %s\n", s);
}

