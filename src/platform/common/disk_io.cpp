#include "platform/common/disk_io.h"

#include <cstdio>

FILE *Drives[NumDrives];	 /* open disk image files */
char *DriveNames[NumDrives]; /* paths of open disk images */


void InitDrives()
{
	/*
		This isn't really needed, Drives[i] and DriveNames[i]
		need not have valid values when not vSonyIsInserted[i].
	*/
	DriveIndex i;

	for (i = 0; i < NumDrives; ++i)
	{
		Drives[i] = nullptr;
		DriveNames[i] = nullptr;
	}
}

tMacErr vSonyTransfer(bool isWrite, uint8_t *buffer, DriveIndex driveNo, uint32_t sonyStart,
					  uint32_t sonyCount, uint32_t *sonyActCount)
{
	/*
		OSGLUxxx common:
		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = tMacErr::miscErr;
	FILE *refnum = Drives[driveNo];
	uint32_t newSonyCount = 0;

	if (fseek(refnum, sonyStart, SEEK_SET) >= 0)
	{
		if (isWrite)
		{
			newSonyCount = fwrite(buffer, 1, sonyCount, refnum);
		}
		else
		{
			newSonyCount = fread(buffer, 1, sonyCount, refnum);
		}

		if (newSonyCount == sonyCount)
		{
			err = tMacErr::noErr;
		}
	}

	if (nullptr != sonyActCount)
	{
		*sonyActCount = newSonyCount;
	}

	return err; /*& figure out what really to return &*/
}

tMacErr vSonyGetSize(DriveIndex driveNo, uint32_t *sonyCount)
{
	/*
		OSGLUxxx common:
		set sonyCount to the size of disk image number driveNo.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = tMacErr::miscErr;
	FILE *refnum = Drives[driveNo];
	long v;

	if (fseek(refnum, 0, SEEK_END) >= 0)
	{
		v = ftell(refnum);
		if (v >= 0)
		{
			*sonyCount = v;
			err = tMacErr::noErr;
		}
	}

	return err; /*& figure out what really to return &*/
}

static tMacErr vSonyEject0(DriveIndex driveNo, bool deleteit)
{
	/*
		OSGLUxxx common:
		close disk image number driveNo.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	FILE *refnum = Drives[driveNo];

	DiskEjectedNotify(driveNo);

	if (deleteit && DriveNames[driveNo] != nullptr)
	{
		(void)remove(DriveNames[driveNo]);
	}

	fclose(refnum);
	Drives[driveNo] = nullptr; /* not really needed */

	free(DriveNames[driveNo]);
	DriveNames[driveNo] = nullptr;

	return tMacErr::noErr;
}

tMacErr vSonyEject(DriveIndex driveNo)
{
	return vSonyEject0(driveNo, false);
}

tMacErr vSonyEjectDelete(DriveIndex driveNo)
{
	return vSonyEject0(driveNo, true);
}

tMacErr vSonyGetName(DriveIndex driveNo, PbufIndex *r)
{
	char *path = DriveNames[driveNo];
	if (nullptr == path)
	{
		return tMacErr::miscErr;
	}

	/* extract last path component */
	char *name = strrchr(path, '/');
	if (name != nullptr)
	{
		++name;
	}
	else
	{
		name = path;
	}

	uint32_t L;
	tMacErr err = UniCodeStrLength(name, &L);
	if (tMacErr::noErr != err)
	{
		return err;
	}

	PbufIndex t;
	err = PbufNew(L, &t);
	if (tMacErr::noErr != err)
	{
		return err;
	}

	UniCodeStr2MacRoman(name, static_cast<char *>(PbufDat[t]));
	*r = t;
	return tMacErr::noErr;
}

void UnInitDrives()
{
	DriveIndex i;

	for (i = 0; i < NumDrives; ++i)
	{
		if (vSonyIsInserted(i))
		{
			(void)vSonyEject(i);
		}
	}
}

bool Sony_Insert0(FILE *refnum, bool locked, char *drivepath)
{
	/*
		OSGLUxxx common:
		Given reference to open file, mount it as a disk image file.
		if "locked", then mount it as a locked disk.
	*/

	DriveIndex driveNo;
	bool IsOk = false;

	if (!FirstFreeDisk(&driveNo))
	{
		fprintf(stderr, "Error: Too many disk images\n");
	}
	else
	{
		/* printf("Sony_Insert0 %d\n", (int)driveNo); */

		{
			Drives[driveNo] = refnum;
			DriveNames[driveNo] = (drivepath != nullptr) ? strdup(drivepath) : nullptr;
			DiskInsertNotify(driveNo, locked);

			IsOk = true;
		}
	}

	if (!IsOk)
	{
		fclose(refnum);
	}

	return IsOk;
}

bool Sony_Insert1(char *drivepath, bool silentfail)
{
	bool locked = false;
	/* printf("Sony_Insert1 %s\n", drivepath); */
	FILE *refnum = fopen(drivepath, "rb+");
	if (nullptr == refnum)
	{
		locked = true;
		refnum = fopen(drivepath, "rb");
	}
	if (nullptr == refnum)
	{
		if (!silentfail)
		{
			fprintf(stderr, "Error: Could not open disk image\n");
		}
	}
	else
	{
		return Sony_Insert0(refnum, locked, drivepath);
	}
	return false;
}

static bool WriteZero(FILE *refnum, uint32_t L)
{
	uint8_t buffer[2048];
	memset(buffer, 0, sizeof(buffer));

	while (L > 0)
	{
		uint32_t i = (L > sizeof(buffer)) ? sizeof(buffer) : L;
		if (fwrite(buffer, 1, i, refnum) != i)
		{
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
	if (nullptr == refnum)
	{
		fprintf(stderr, "Error: Could not create disk image\n");
	}
	else
	{
		if (WriteZero(refnum, L))
		{
			IsOk = Sony_Insert0(refnum, false, drivepath);
			refnum = nullptr;
		}
		if (refnum != nullptr)
		{
			fclose(refnum);
		}
		if (!IsOk)
		{
			(void)remove(drivepath);
		}
	}
}

void MakeNewDisk(uint32_t L, char *drivename)
{
	/* Create new disk in working directory / "out" subdirectory */
	char s[256];

	snprintf(s, sizeof(s), "out/%s", drivename);
	/* Ensure "out" directory exists */
	(void)mkdir("out", 0755);
	MakeNewDisk0(L, s);
	fprintf(stderr, "Exported file: %s\n", s);
}
