/*
	OSGLUXWN.c

	Copyright (C) 2009 Michael Hanni, Christian Bauer,
	Stephan Kochen, Paul C. Pratt, and others

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
	Operating System GLUe for X WiNdow system

	All operating system dependent code for the
	X Window System should go here.

	This code is descended from Michael Hanni's X
	port of vMac, by Philip Cummins.
	I learned more about how X programs work by
	looking at other programs such as Basilisk II,
	the UAE Amiga Emulator, Bochs, QuakeForge,
	DooM Legacy, and the FLTK. A few snippets
	from them are used here.

	Drag and Drop support is based on the specification
	"XDND: Drag-and-Drop Protocol for the X Window System"
	developed by John Lindal at New Planet Software, and
	looking at included examples, one by Paul Sheer.
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"

#ifdef WantOSGLUXWN

/* --- some simple utilities --- */

void MyMoveBytes(uint8_t * srcPtr, uint8_t * destPtr, int32_t byteCount)
{
	(void) memcpy((char *)destPtr, (char *)srcPtr, byteCount);
}

/* --- control mode and internationalization --- */

#define NeedCell2PlainAsciiMap 1

#include "platform/common/intl_chars.h" /* was intl_chars_impl.h — now a separate TU */


static char *d_arg = NULL;
static char *n_arg = NULL;

#if CanGetAppPath
static char *app_parent = NULL;
static char *app_name = NULL;
#endif

static tMacErr ChildPath(char *x, char *y, char **r)
{
	tMacErr err = mnvm_miscErr;
	int nx = strlen(x);
	int ny = strlen(y);
	{
		if ((nx > 0) && ('/' == x[nx - 1])) {
			--nx;
		}
		{
			int nr = nx + 1 + ny;
			char *p = malloc(nr + 1);
			if (p != NULL) {
				char *p2 = p;
				(void) memcpy(p2, x, nx);
				p2 += nx;
				*p2++ = '/';
				(void) memcpy(p2, y, ny);
				p2 += ny;
				*p2 = 0;
				*r = p;
				err = mnvm_noErr;
			}
		}
	}

	return err;
}

#if UseActvFile || IncludeSonyNew
static tMacErr FindOrMakeChild(char *x, char *y, char **r)
{
	tMacErr err;
	struct stat folder_info;
	char *r0;

	if (mnvm_noErr == (err = ChildPath(x, y, &r0))) {
		if (0 != stat(r0, &folder_info)) {
			if (0 != mkdir(r0, S_IRWXU)) {
				err = mnvm_miscErr;
			} else {
				*r = r0;
				err = mnvm_noErr;
			}
		} else {
			if (! S_ISDIR(folder_info.st_mode)) {
				err = mnvm_miscErr;
			} else {
				*r = r0;
				err = mnvm_noErr;
			}
		}
	}

	return err;
}
#endif

static void MyMayFree(char *p)
{
	if (NULL != p) {
		free(p);
	}
}

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#ifndef dbglog_ToStdErr
#define dbglog_ToStdErr 0
#endif

#if ! dbglog_ToStdErr
static FILE *dbglog_File = NULL;
#endif

static bool dbglog_open0(void)
{
#if dbglog_ToStdErr
	return true;
#else
	dbglog_File = fopen("dbglog.txt", "w");
	return (NULL != dbglog_File);
#endif
}

static void dbglog_write0(char *s, uint32_t L)
{
#if dbglog_ToStdErr
	(void) fwrite(s, 1, L, stderr);
#else
	if (dbglog_File != NULL) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
#endif
}

static void dbglog_close0(void)
{
#if ! dbglog_ToStdErr
	if (dbglog_File != NULL) {
		fclose(dbglog_File);
		dbglog_File = NULL;
	}
#endif
}

#endif

/* --- debug settings and utilities --- */

#if ! dbglog_HAVE
#define WriteExtraErr(s)
#else
static void WriteExtraErr(char *s)
{
	dbglog_writeCStr("*** error: ");
	dbglog_writeCStr(s);
	dbglog_writeReturn();
}
#endif

static Display *x_display = NULL;

#define MyDbgEvents (dbglog_HAVE && 0)

#if MyDbgEvents
static void WriteDbgAtom(char *s, Atom x)
{
	char *name = XGetAtomName(x_display, x);
	if (name != NULL) {
		dbglog_writeCStr("Atom ");
		dbglog_writeCStr(s);
		dbglog_writeCStr(": ");
		dbglog_writeCStr(name);
		dbglog_writeReturn();
		XFree(name);
	}
}
#endif

/* --- information about the environment --- */

static Atom MyXA_DeleteW = (Atom)0;
#if EnableDragDrop
static Atom MyXA_UriList = (Atom)0;
static Atom MyXA_DndAware = (Atom)0;
static Atom MyXA_DndEnter = (Atom)0;
static Atom MyXA_DndLeave = (Atom)0;
static Atom MyXA_DndDrop = (Atom)0;
static Atom MyXA_DndPosition = (Atom)0;
static Atom MyXA_DndStatus = (Atom)0;
static Atom MyXA_DndActionCopy = (Atom)0;
static Atom MyXA_DndActionPrivate = (Atom)0;
static Atom MyXA_DndSelection = (Atom)0;
static Atom MyXA_DndFinished = (Atom)0;
static Atom MyXA_MinivMac_DndXchng = (Atom)0;
static Atom MyXA_NetActiveWindow = (Atom)0;
static Atom MyXA_NetSupported = (Atom)0;
#endif
#if IncludeHostTextClipExchange
static Atom MyXA_CLIPBOARD = (Atom)0;
static Atom MyXA_TARGETS = (Atom)0;
static Atom MyXA_MinivMac_Clip = (Atom)0;
#endif

static void LoadMyXA(void)
{
	MyXA_DeleteW = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
#if EnableDragDrop
	MyXA_UriList = XInternAtom (x_display, "text/uri-list", False);
	MyXA_DndAware = XInternAtom (x_display, "XdndAware", False);
	MyXA_DndEnter = XInternAtom(x_display, "XdndEnter", False);
	MyXA_DndLeave = XInternAtom(x_display, "XdndLeave", False);
	MyXA_DndDrop = XInternAtom(x_display, "XdndDrop", False);
	MyXA_DndPosition = XInternAtom(x_display, "XdndPosition", False);
	MyXA_DndStatus = XInternAtom(x_display, "XdndStatus", False);
	MyXA_DndActionCopy = XInternAtom(x_display,
		"XdndActionCopy", False);
	MyXA_DndActionPrivate = XInternAtom(x_display,
		"XdndActionPrivate", False);
	MyXA_DndSelection = XInternAtom(x_display, "XdndSelection", False);
	MyXA_DndFinished = XInternAtom(x_display, "XdndFinished", False);
	MyXA_MinivMac_DndXchng = XInternAtom(x_display,
		"_MinivMac_DndXchng", False);
	MyXA_NetActiveWindow = XInternAtom(x_display,
		"_NET_ACTIVE_WINDOW", False);
	MyXA_NetSupported = XInternAtom(x_display,
		"_NET_SUPPORTED", False);
#endif
#if IncludeHostTextClipExchange
	MyXA_CLIPBOARD = XInternAtom(x_display, "CLIPBOARD", False);
	MyXA_TARGETS = XInternAtom(x_display, "TARGETS", False);
	MyXA_MinivMac_Clip = XInternAtom(x_display,
		"_MinivMac_Clip", False);
#endif
}

#if EnableDragDrop
static bool NetSupportedContains(Atom x)
{
	/*
		Note that the window manager could be replaced at
		any time, so don't cache results of this function.
	*/
	Atom ret_type;
	int ret_format;
	unsigned long ret_item;
	unsigned long remain_byte;
	unsigned long i;
	unsigned char *s = 0;
	bool foundit = false;
	Window rootwin = XRootWindow(x_display,
		DefaultScreen(x_display));

	if (Success != XGetWindowProperty(x_display, rootwin,
		MyXA_NetSupported,
		0, 65535, False, XA_ATOM, &ret_type, &ret_format,
		&ret_item, &remain_byte, &s))
	{
		WriteExtraErr("XGetWindowProperty failed");
	} else if (! s) {
		WriteExtraErr("XGetWindowProperty failed");
	} else if (ret_type != XA_ATOM) {
		WriteExtraErr("XGetWindowProperty returns wrong type");
	} else {
		Atom *v = (Atom *)s;

		for (i = 0; i < ret_item; ++i) {
			if (v[i] == x) {
				foundit = true;
				/* fprintf(stderr, "found the hint\n"); */
			}
		}
	}
	if (s) {
		XFree(s);
	}
	return foundit;
}
#endif

#define WantColorTransValid 1

#include "platform/common/osglu_common.h" /* was osglu_common_impl.h — now a separate TU */

#include "platform/common/param_buffers.h" /* was param_buffers_impl.h — now a separate TU */

#include "platform/common/control_mode.h" /* was control_mode_impl.h — now a separate TU */

/* --- text translation --- */

#if IncludePbufs
/* this is table for Windows, any changes needed for X? */
static const uint8_t Native2MacRomanTab[] = {
	0xAD, 0xB0, 0xE2, 0xC4, 0xE3, 0xC9, 0xA0, 0xE0,
	0xF6, 0xE4, 0xB6, 0xDC, 0xCE, 0xB2, 0xB3, 0xB7,
	0xB8, 0xD4, 0xD5, 0xD2, 0xD3, 0xA5, 0xD0, 0xD1,
	0xF7, 0xAA, 0xC5, 0xDD, 0xCF, 0xB9, 0xC3, 0xD9,
	0xCA, 0xC1, 0xA2, 0xA3, 0xDB, 0xB4, 0xBA, 0xA4,
	0xAC, 0xA9, 0xBB, 0xC7, 0xC2, 0xBD, 0xA8, 0xF8,
	0xA1, 0xB1, 0xC6, 0xD7, 0xAB, 0xB5, 0xA6, 0xE1,
	0xFC, 0xDA, 0xBC, 0xC8, 0xDE, 0xDF, 0xF0, 0xC0,
	0xCB, 0xE7, 0xE5, 0xCC, 0x80, 0x81, 0xAE, 0x82,
	0xE9, 0x83, 0xE6, 0xE8, 0xED, 0xEA, 0xEB, 0xEC,
	0xF5, 0x84, 0xF1, 0xEE, 0xEF, 0xCD, 0x85, 0xF9,
	0xAF, 0xF4, 0xF2, 0xF3, 0x86, 0xFA, 0xFB, 0xA7,
	0x88, 0x87, 0x89, 0x8B, 0x8A, 0x8C, 0xBE, 0x8D,
	0x8F, 0x8E, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95,
	0xFD, 0x96, 0x98, 0x97, 0x99, 0x9B, 0x9A, 0xD6,
	0xBF, 0x9D, 0x9C, 0x9E, 0x9F, 0xFE, 0xFF, 0xD8
};
#endif

#if IncludePbufs
static tMacErr NativeTextToMacRomanPbuf(char *x, tPbuf *r)
{
	if (NULL == x) {
		return mnvm_miscErr;
	} else {
		uint8_t * p;
		uint32_t L = strlen(x);

		p = (uint8_t *)malloc(L);
		if (NULL == p) {
			return mnvm_miscErr;
		} else {
			uint8_t *p0 = (uint8_t *)x;
			uint8_t *p1 = (uint8_t *)p;
			int i;

			for (i = L; --i >= 0; ) {
				uint8_t v = *p0++;
				if (v >= 128) {
					v = Native2MacRomanTab[v - 128];
				} else if (10 == v) {
					v = 13;
				}
				*p1++ = v;
			}

			return PbufNewFromPtr(p, L, r);
		}
	}
}
#endif

#if IncludePbufs
/* this is table for Windows, any changes needed for X? */
static const uint8_t MacRoman2NativeTab[] = {
	0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1,
	0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,
	0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3,
	0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,
	0x86, 0xB0, 0xA2, 0xA3, 0xA7, 0x95, 0xB6, 0xDF,
	0xAE, 0xA9, 0x99, 0xB4, 0xA8, 0x80, 0xC6, 0xD8,
	0x81, 0xB1, 0x8D, 0x8E, 0xA5, 0xB5, 0x8A, 0x8F,
	0x90, 0x9D, 0xA6, 0xAA, 0xBA, 0xAD, 0xE6, 0xF8,
	0xBF, 0xA1, 0xAC, 0x9E, 0x83, 0x9A, 0xB2, 0xAB,
	0xBB, 0x85, 0xA0, 0xC0, 0xC3, 0xD5, 0x8C, 0x9C,
	0x96, 0x97, 0x93, 0x94, 0x91, 0x92, 0xF7, 0xB3,
	0xFF, 0x9F, 0xB9, 0xA4, 0x8B, 0x9B, 0xBC, 0xBD,
	0x87, 0xB7, 0x82, 0x84, 0x89, 0xC2, 0xCA, 0xC1,
	0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,
	0xBE, 0xD2, 0xDA, 0xDB, 0xD9, 0xD0, 0x88, 0x98,
	0xAF, 0xD7, 0xDD, 0xDE, 0xB8, 0xF0, 0xFD, 0xFE
};
#endif

#if IncludePbufs
static bool MacRomanTextToNativePtr(tPbuf i, bool IsFileName,
	uint8_t * *r)
{
	uint8_t * p;
	void *Buffer = PbufDat[i];
	uint32_t L = PbufSize[i];

	p = (uint8_t *)malloc(L + 1);
	if (p != NULL) {
		uint8_t *p0 = (uint8_t *)Buffer;
		uint8_t *p1 = (uint8_t *)p;
		int j;

		if (IsFileName) {
			for (j = L; --j >= 0; ) {
				uint8_t x = *p0++;
				if (x < 32) {
					x = '-';
				} else if (x >= 128) {
					x = MacRoman2NativeTab[x - 128];
				} else {
					switch (x) {
						case '/':
						case '<':
						case '>':
						case '|':
						case ':':
							x = '-';
						default:
							break;
					}
				}
				*p1++ = x;
			}
			if ('.' == p[0]) {
				p[0] = '-';
			}
		} else {
			for (j = L; --j >= 0; ) {
				uint8_t x = *p0++;
				if (x >= 128) {
					x = MacRoman2NativeTab[x - 128];
				} else if (13 == x) {
					x = '\n';
				}
				*p1++ = x;
			}
		}
		*p1 = 0;

		*r = p;
		return true;
	}
	return false;
}
#endif

static void NativeStrFromCStr(char *r, char *s)
{
	uint8_t ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */

#define NotAfileRef NULL

static FILE *Drives[NumDrives]; /* open disk image files */
#if IncludeSonyGetName || IncludeSonyNew
static char *DriveNames[NumDrives];
#endif

static void InitDrives(void)
{
	/*
		This isn't really needed, Drives[i] and DriveNames[i]
		need not have valid values when not vSonyIsInserted[i].
	*/
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
#if IncludeSonyGetName || IncludeSonyNew
		DriveNames[i] = NULL;
#endif
	}
}

 tMacErr vSonyTransfer(bool IsWrite, uint8_t * Buffer,
	tDrive Drive_No, uint32_t Sony_Start, uint32_t Sony_Count,
	uint32_t *Sony_ActCount)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	uint32_t NewSony_Count = 0;

	if (0 == fseek(refnum, Sony_Start, SEEK_SET)) {
		if (IsWrite) {
			NewSony_Count = fwrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = fread(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = mnvm_noErr;
		}
	}

	if (nullptr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err; /*& figure out what really to return &*/
}

 tMacErr vSonyGetSize(tDrive Drive_No, uint32_t *Sony_Count)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	long v;

	if (0 == fseek(refnum, 0, SEEK_END)) {
		v = ftell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = mnvm_noErr;
		}
	}

	return err; /*& figure out what really to return &*/
}

#ifndef HaveAdvisoryLocks
#define HaveAdvisoryLocks 1
#endif

/*
	What is the difference between fcntl(fd, F_SETLK ...
	and flock(fd ... ?
*/

#if HaveAdvisoryLocks
static bool MyLockFile(FILE *refnum)
{
	bool IsOk = false;

#if 1
	struct flock fl;
	int fd = fileno(refnum);

	fl.l_start = 0; /* starting offset */
	fl.l_len = 0; /* len = 0 means until end of file */
	/* fl.pid_t l_pid; */ /* lock owner, don't need to set */
	fl.l_type = F_WRLCK; /* lock type: read/write, etc. */
	fl.l_whence = SEEK_SET; /* type of l_start */
	if (-1 == fcntl(fd, F_SETLK, &fl)) {
		MacMsg(kStrImageInUseTitle, kStrImageInUseMessage,
			false);
	} else {
		IsOk = true;
	}
#else
	int fd = fileno(refnum);

	if (-1 == flock(fd, LOCK_EX | LOCK_NB)) {
		MacMsg(kStrImageInUseTitle, kStrImageInUseMessage,
			false);
	} else {
		IsOk = true;
	}
#endif

	return IsOk;
}
#endif

#if HaveAdvisoryLocks
static void MyUnlockFile(FILE *refnum)
{
#if 1
	struct flock fl;
	int fd = fileno(refnum);

	fl.l_start = 0; /* starting offset */
	fl.l_len = 0; /* len = 0 means until end of file */
	/* fl.pid_t l_pid; */ /* lock owner, don't need to set */
	fl.l_type = F_UNLCK;     /* lock type: read/write, etc. */
	fl.l_whence = SEEK_SET;   /* type of l_start */
	if (-1 == fcntl(fd, F_SETLK, &fl)) {
		/* an error occurred */
	}
#else
	int fd = fileno(refnum);

	if (-1 == flock(fd, LOCK_UN)) {
	}
#endif
}
#endif

static tMacErr vSonyEject0(tDrive Drive_No, bool deleteit)
{
	FILE *refnum = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

#if HaveAdvisoryLocks
	MyUnlockFile(refnum);
#endif

	fclose(refnum);
	Drives[Drive_No] = NotAfileRef; /* not really needed */

#if IncludeSonyGetName || IncludeSonyNew
	{
		char *s = DriveNames[Drive_No];
		if (NULL != s) {
			if (deleteit) {
				remove(s);
			}
			free(s);
			DriveNames[Drive_No] = NULL; /* not really needed */
		}
	}
#endif

	return mnvm_noErr;
}

 tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, false);
}

#if IncludeSonyNew
 tMacErr vSonyEjectDelete(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, true);
}
#endif

static void UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

#if IncludeSonyGetName
 tMacErr vSonyGetName(tDrive Drive_No, tPbuf *r)
{
	char *drivepath = DriveNames[Drive_No];
	if (NULL == drivepath) {
		return mnvm_miscErr;
	} else {
		char *s = strrchr(drivepath, '/');
		if (NULL == s) {
			s = drivepath;
		} else {
			++s;
		}
		return NativeTextToMacRomanPbuf(s, r);
	}
}
#endif

static bool Sony_Insert0(FILE *refnum, bool locked,
	char *drivepath)
{
	tDrive Drive_No;
	bool IsOk = false;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(kStrTooManyImagesTitle, kStrTooManyImagesMessage,
			false);
	} else {
		/* printf("Sony_Insert0 %d\n", (int)Drive_No); */

#if HaveAdvisoryLocks
		if (locked || MyLockFile(refnum))
#endif
		{
			Drives[Drive_No] = refnum;
			DiskInsertNotify(Drive_No, locked);

#if IncludeSonyGetName || IncludeSonyNew
			{
				uint32_t L = strlen(drivepath);
				char *p = malloc(L + 1);
				if (p != NULL) {
					(void) memcpy(p, drivepath, L + 1);
				}
				DriveNames[Drive_No] = p;
			}
#endif

			IsOk = true;
		}
	}

	if (! IsOk) {
		fclose(refnum);
	}

	return IsOk;
}

static bool Sony_Insert1(char *drivepath, bool silentfail)
{
	bool locked = false;
	/* printf("Sony_Insert1 %s\n", drivepath); */
	FILE *refnum = fopen(drivepath, "rb+");
	if (NULL == refnum) {
		locked = true;
		refnum = fopen(drivepath, "rb");
	}
	if (NULL == refnum) {
		if (! silentfail) {
			MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, false);
		}
	} else {
		return Sony_Insert0(refnum, locked, drivepath);
	}
	return false;
}

static tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	FILE *ROM_File;
	int File_Size;

	ROM_File = fopen(path, "rb");
	if (NULL == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		File_Size = fread(ROM, 1, kROM_Size, ROM_File);
		if (kROM_Size != File_Size) {
			if (feof(ROM_File)) {
				MacMsgOverride(kStrShortROMTitle,
					kStrShortROMMessage);
				err = mnvm_eofErr;
			} else {
				MacMsgOverride(kStrNoReadROMTitle,
					kStrNoReadROMMessage);
				err = mnvm_miscErr;
			}
		} else {
			err = ROM_IsValid();
		}
		fclose(ROM_File);
	}

	return err;
}

static bool Sony_Insert1a(char *drivepath, bool silentfail)
{
	bool v;

	if (! ROM_loaded) {
		v = (mnvm_noErr == LoadMacRomFrom(drivepath));
	} else {
		v = Sony_Insert1(drivepath, silentfail);
	}

	return v;
}

static bool Sony_Insert2(char *s)
{
	char *d =
#if CanGetAppPath
		(NULL == d_arg) ? app_parent :
#endif
		d_arg;
	bool IsOk = false;

	if (NULL == d) {
		IsOk = Sony_Insert1(s, true);
	} else {
		char *t;

		if (mnvm_noErr == ChildPath(d, s, &t)) {
			IsOk = Sony_Insert1(t, true);
			free(t);
		}
	}

	return IsOk;
}

static bool Sony_InsertIth(int i)
{
	bool v;

	if ((i > 9) || ! FirstFreeDisk(nullptr)) {
		v = false;
	} else {
		char s[] = "disk?.dsk";

		s[4] = '0' + i;

		v = Sony_Insert2(s);
	}

	return v;
}

static bool LoadInitialImages(void)
{
	if (! AnyDiskInserted()) {
		int i;

		for (i = 1; Sony_InsertIth(i); ++i) {
			/* stop on first error (including file not found) */
		}
	}

	return true;
}

#if IncludeSonyNew
static bool WriteZero(FILE *refnum, uint32_t L)
{
#define ZeroBufferSize 2048
	uint32_t i;
	uint8_t buffer[ZeroBufferSize];

	memset(&buffer, 0, ZeroBufferSize);

	while (L > 0) {
		i = (L > ZeroBufferSize) ? ZeroBufferSize : L;
		if (fwrite(buffer, 1, i, refnum) != i) {
			return false;
		}
		L -= i;
	}
	return true;
}
#endif

#if IncludeSonyNew
static void MakeNewDisk0(uint32_t L, char *drivepath)
{
	bool IsOk = false;
	FILE *refnum = fopen(drivepath, "wb+");
	if (NULL == refnum) {
		MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, false);
	} else {
		if (WriteZero(refnum, L)) {
			IsOk = Sony_Insert0(refnum, false, drivepath);
			refnum = NULL;
		}
		if (refnum != NULL) {
			fclose(refnum);
		}
		if (! IsOk) {
			(void) remove(drivepath);
		}
	}
}
#endif

#if IncludeSonyNew
static void MakeNewDisk(uint32_t L, char *drivename)
{
	char *d =
#if CanGetAppPath
		(NULL == d_arg) ? app_parent :
#endif
		d_arg;

	if (NULL == d) {
		MakeNewDisk0(L, drivename); /* in current directory */
	} else {
		tMacErr err;
		char *t = NULL;
		char *t2 = NULL;

		if (mnvm_noErr == (err = FindOrMakeChild(d, "out", &t)))
		if (mnvm_noErr == (err = ChildPath(t, drivename, &t2)))
		{
			MakeNewDisk0(L, t2);
		}

		MyMayFree(t2);
		MyMayFree(t);
	}
}
#endif

#if IncludeSonyNew
static void MakeNewDiskAtDefault(uint32_t L)
{
	char s[ClStrMaxLength + 1];

	NativeStrFromCStr(s, "untitled.dsk");
	MakeNewDisk(L, s);
}
#endif

/* --- ROM --- */

static char *rom_path = NULL;

#if 0
#include <pwd.h>
#include <unistd.h>
#endif

static tMacErr FindUserHomeFolder(char **r)
{
	tMacErr err;
	char *s;
#if 0
	struct passwd *user;
#endif

	if (NULL != (s = getenv("HOME"))) {
		*r = s;
		err = mnvm_noErr;
	} else
#if 0
	if ((NULL != (user = getpwuid(getuid())))
		&& (NULL != (s = user->pw_dir)))
	{
		/*
			From getpwuid man page:
			"An application that wants to determine its user's
			home directory should inspect the value of HOME
			(rather than the value getpwuid(getuid())->pw_dir)
			since this allows the user to modify their notion of
			"the home directory" during a login session."

			But it is possible for HOME to not be set.
			Some sources say to use getpwuid in that case.
		*/
		*r = s;
		err = mnvm_noErr;
	} else
#endif
	{
		err = mnvm_fnfErr;
	}

	return err;
}

static tMacErr LoadMacRomFromHome(void)
{
	tMacErr err;
	char *s;
	char *t = NULL;
	char *t2 = NULL;
	char *t3 = NULL;

	if (mnvm_noErr == (err = FindUserHomeFolder(&s)))
	if (mnvm_noErr == (err = ChildPath(s, ".gryphel", &t)))
	if (mnvm_noErr == (err = ChildPath(t, "mnvm_rom", &t2)))
	if (mnvm_noErr == (err = ChildPath(t2, RomFileName, &t3)))
	{
		err = LoadMacRomFrom(t3);
	}

	MyMayFree(t3);
	MyMayFree(t2);
	MyMayFree(t);

	return err;
}

#if CanGetAppPath
static tMacErr LoadMacRomFromAppPar(void)
{
	tMacErr err;
	char *d =
#if CanGetAppPath
		(NULL == d_arg) ? app_parent :
#endif
		d_arg;
	char *t = NULL;

	if (NULL == d) {
		err = mnvm_fnfErr;
	} else {
		if (mnvm_noErr == (err = ChildPath(d, RomFileName,
			&t)))
		{
			err = LoadMacRomFrom(t);
		}
	}

	MyMayFree(t);

	return err;
}
#endif

static bool LoadMacRom(void)
{
	tMacErr err;

	if ((NULL == rom_path)
		|| (mnvm_fnfErr == (err = LoadMacRomFrom(rom_path))))
#if CanGetAppPath
	if (mnvm_fnfErr == (err = LoadMacRomFromAppPar()))
#endif
	if (mnvm_fnfErr == (err = LoadMacRomFromHome()))
	if (mnvm_fnfErr == (err = LoadMacRomFrom(RomFileName)))
	{
	}

	return true; /* keep launching Mini vMac, regardless */
}

#if UseActvFile

#define ActvCodeFileName "act_1"

static tMacErr ActvCodeFileLoad(uint8_t * p)
{
	tMacErr err;
	char *s;
	char *t = NULL;
	char *t2 = NULL;
	char *t3 = NULL;

	if (mnvm_noErr == (err = FindUserHomeFolder(&s)))
	if (mnvm_noErr == (err = ChildPath(s, ".gryphel", &t)))
	if (mnvm_noErr == (err = ChildPath(t, "mnvm_act", &t2)))
	if (mnvm_noErr == (err = ChildPath(t2, ActvCodeFileName, &t3)))
	{
		FILE *Actv_File;
		int File_Size;

		Actv_File = fopen(t3, "rb");
		if (NULL == Actv_File) {
			err = mnvm_fnfErr;
		} else {
			File_Size = fread(p, 1, ActvCodeFileLen, Actv_File);
			if (File_Size != ActvCodeFileLen) {
				if (feof(Actv_File)) {
					err = mnvm_eofErr;
				} else {
					err = mnvm_miscErr;
				}
			} else {
				err = mnvm_noErr;
			}
			fclose(Actv_File);
		}
	}

	MyMayFree(t3);
	MyMayFree(t2);
	MyMayFree(t);

	return err;
}

static tMacErr ActvCodeFileSave(uint8_t * p)
{
	tMacErr err;
	char *s;
	char *t = NULL;
	char *t2 = NULL;
	char *t3 = NULL;

	if (mnvm_noErr == (err = FindUserHomeFolder(&s)))
	if (mnvm_noErr == (err = FindOrMakeChild(s, ".gryphel", &t)))
	if (mnvm_noErr == (err = FindOrMakeChild(t, "mnvm_act", &t2)))
	if (mnvm_noErr == (err = ChildPath(t2, ActvCodeFileName, &t3)))
	{
		FILE *Actv_File;
		int File_Size;

		Actv_File = fopen(t3, "wb+");
		if (NULL == Actv_File) {
			err = mnvm_fnfErr;
		} else {
			File_Size = fwrite(p, 1, ActvCodeFileLen, Actv_File);
			if (File_Size != ActvCodeFileLen) {
				err = mnvm_miscErr;
			} else {
				err = mnvm_noErr;
			}
			fclose(Actv_File);
		}
	}

	MyMayFree(t3);
	MyMayFree(t2);
	MyMayFree(t);

	return err;
}

#endif /* UseActvFile */

/* --- video out --- */

static Window my_main_wind = 0;
static GC my_gc = NULL;
static bool NeedFinishOpen1 = false;
static bool NeedFinishOpen2 = false;

static XColor x_black;
static XColor x_white;

#if MayFullScreen
static short hOffset;
static short vOffset;
#endif

#if VarFullScreen
static bool UseFullScreen = (WantInitFullScreen != 0);
#endif

#if EnableMagnify
static bool UseMagnify = (WantInitMagnify != 0);
#endif

static bool gBackgroundFlag = false;
static bool gTrueBackgroundFlag = false;
static bool CurSpeedStopped = true;

#ifndef UseColorImage
#define UseColorImage (0 != vMacScreenDepth)
#endif

static XImage *my_image = NULL;

#if EnableMagnify
static XImage *my_Scaled_image = NULL;
#endif

#if EnableMagnify
#define MaxScale MyWindowScale
#else
#define MaxScale 1
#endif

#define WantScalingTabl (EnableMagnify || UseColorImage)

#if WantScalingTabl

static uint8_t * ScalingTabl = nullptr;

#define ScalingTablsz1 (256 * MaxScale)

#if UseColorImage
#define ScalingTablsz (ScalingTablsz1 << 5)
#else
#define ScalingTablsz ScalingTablsz1
#endif

#endif /* WantScalingTabl */


#define WantScalingBuff (EnableMagnify || UseColorImage)

#if WantScalingBuff

static uint8_t * ScalingBuff = nullptr;


#if UseColorImage
#define ScalingBuffsz \
	(vMacScreenNumPixels * 4 * MaxScale * MaxScale)
#else
#define ScalingBuffsz ((long)vMacScreenMonoNumBytes \
	* MaxScale * MaxScale)
#endif

#endif /* WantScalingBuff */


#if EnableMagnify && ! UseColorImage
static void SetUpScalingTabl(void)
{
	uint8_t *p4;
	int i;
	int j;
	int k;
	uint8_t bitsRemaining;
	uint8_t t1;
	uint8_t t2;

	p4 = ScalingTabl;
	for (i = 0; i < 256; ++i) {
		bitsRemaining = 8;
		t2 = 0;
		for (j = 8; --j >= 0; ) {
			t1 = (i >> j) & 1;
			for (k = MyWindowScale; --k >= 0; ) {
				t2 = (t2 << 1) | t1;
				if (--bitsRemaining == 0) {
					*p4++ = t2;
					bitsRemaining = 8;
					t2 = 0;
				}
			}
		}
	}
}
#endif

#if EnableMagnify && (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
static void SetUpColorScalingTabl(void)
{
	int i;
	int j;
	int k;
	int a;
	uint32_t v;
	uint32_t * p4;

	p4 = (uint32_t *)ScalingTabl;
	for (i = 0; i < 256; ++i) {
		for (k = 1 << (3 - vMacScreenDepth); --k >= 0; ) {
			j = (i >> (k << vMacScreenDepth)) & (CLUT_size - 1);
			v = (((long)CLUT_reds[j] & 0xFF00) << 8)
				| ((long)CLUT_greens[j] & 0xFF00)
				| (((long)CLUT_blues[j] & 0xFF00) >> 8);
			for (a = MyWindowScale; --a >= 0; ) {
				*p4++ = v;
			}
		}
	}
}
#endif

#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
static void SetUpColorTabl(void)
{
	int i;
	int j;
	int k;
	uint32_t * p4;

	p4 = (uint32_t *)ScalingTabl;
	for (i = 0; i < 256; ++i) {
		for (k = 1 << (3 - vMacScreenDepth); --k >= 0; ) {
			j = (i >> (k << vMacScreenDepth)) & (CLUT_size - 1);
			*p4++ = (((long)CLUT_reds[j] & 0xFF00) << 8)
				| ((long)CLUT_greens[j] & 0xFF00)
				| (((long)CLUT_blues[j] & 0xFF00) >> 8);
		}
	}
}
#endif

#if EnableMagnify && UseColorImage
static void SetUpBW2ColorScalingTabl(void)
{
	int i;
	int k;
	int a;
	uint32_t v;
	uint32_t * p4;

	p4 = (uint32_t *)ScalingTabl;
	for (i = 0; i < 256; ++i) {
		for (k = 8; --k >= 0; ) {
			if (0 != ((i >> k) & 1)) {
				v = 0;
			} else {
				v = 0xFFFFFF;
			}

			for (a = MyWindowScale; --a >= 0; ) {
				*p4++ = v;
			}
		}
	}
}
#endif

#if UseColorImage
static void SetUpBW2ColorTabl(void)
{
	int i;
	int k;
	uint32_t v;
	uint32_t * p4;

	p4 = (uint32_t *)ScalingTabl;
	for (i = 0; i < 256; ++i) {
		for (k = 8; --k >= 0; ) {
			if (0 != ((i >> k) & 1)) {
				v = 0;
			} else {
				v = 0xFFFFFF;
			}
			*p4++ = v;
		}
	}
}
#endif


#if EnableMagnify && ! UseColorImage

#define ScrnMapr_DoMap UpdateScaledBWCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 0
#define ScrnMapr_Map ScalingTabl
#define ScrnMapr_Scale MyWindowScale

#include "platform/common/screen_map.h"

#endif


#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)

#define ScrnMapr_DoMap UpdateMappedColorCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map ScalingTabl

#include "platform/common/screen_map.h"

#endif


#if EnableMagnify && (0 != vMacScreenDepth) && (vMacScreenDepth < 4)

#define ScrnMapr_DoMap UpdateMappedScaledColorCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map ScalingTabl
#define ScrnMapr_Scale MyWindowScale

#include "platform/common/screen_map.h"

#endif


#if vMacScreenDepth >= 4

#define ScrnTrns_DoTrans UpdateTransColorCopy
#define ScrnTrns_Src GetCurDrawBuff()
#define ScrnTrns_Dst ScalingBuff
#define ScrnTrns_SrcDepth vMacScreenDepth
#define ScrnTrns_DstDepth 5

#include "platform/common/screen_translate.h"

#endif

#if EnableMagnify && (vMacScreenDepth >= 4)

#define ScrnTrns_DoTrans UpdateTransScaledColorCopy
#define ScrnTrns_Src GetCurDrawBuff()
#define ScrnTrns_Dst ScalingBuff
#define ScrnTrns_SrcDepth vMacScreenDepth
#define ScrnTrns_DstDepth 5
#define ScrnTrns_Scale MyWindowScale

#include "platform/common/screen_translate.h"

#endif


#if EnableMagnify && UseColorImage

#define ScrnMapr_DoMap UpdateMappedScaledBW2ColorCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map ScalingTabl
#define ScrnMapr_Scale MyWindowScale

#include "platform/common/screen_map.h"

#endif


#if UseColorImage

#define ScrnMapr_DoMap UpdateMappedBW2ColorCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map ScalingTabl

#include "platform/common/screen_map.h"

#endif


static void HaveChangedScreenBuff(uint16_t top, uint16_t left,
	uint16_t bottom, uint16_t right)
{
	int XDest;
	int YDest;
	char *the_data;

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		if (top < ViewVStart) {
			top = ViewVStart;
		}
		if (left < ViewHStart) {
			left = ViewHStart;
		}
		if (bottom > ViewVStart + ViewVSize) {
			bottom = ViewVStart + ViewVSize;
		}
		if (right > ViewHStart + ViewHSize) {
			right = ViewHStart + ViewHSize;
		}

		if ((top >= bottom) || (left >= right)) {
			goto label_exit;
		}
	}
#endif

	XDest = left;
	YDest = top;

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		XDest -= ViewHStart;
		YDest -= ViewVStart;
	}
#endif

#if EnableMagnify
	if (UseMagnify) {
		XDest *= MyWindowScale;
		YDest *= MyWindowScale;
	}
#endif

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		XDest += hOffset;
		YDest += vOffset;
	}
#endif

#if EnableMagnify
	if (UseMagnify) {
#if UseColorImage
#if 0 != vMacScreenDepth
		if (UseColorMode) {
#if vMacScreenDepth < 4
			if (! ColorTransValid) {
				SetUpColorScalingTabl();
				ColorTransValid = true;
			}

			UpdateMappedScaledColorCopy(top, left, bottom, right);
#else
			UpdateTransScaledColorCopy(top, left, bottom, right);
#endif
		} else
#endif /* 0 != vMacScreenDepth */
		{
			if (! ColorTransValid) {
				SetUpBW2ColorScalingTabl();
				ColorTransValid = true;
			}

			UpdateMappedScaledBW2ColorCopy(top, left, bottom, right);
		}
#else /* ! UseColorImage */
		/* assume 0 == vMacScreenDepth */
		{
			if (! ColorTransValid) {
				SetUpScalingTabl();
				ColorTransValid = true;
			}

			UpdateScaledBWCopy(top, left, bottom, right);
		}
#endif /* UseColorImage */

		{
			char *saveData = my_Scaled_image->data;
			my_Scaled_image->data = (char *)ScalingBuff;

			XPutImage(x_display, my_main_wind, my_gc, my_Scaled_image,
				left * MyWindowScale, top * MyWindowScale,
				XDest, YDest,
				(right - left) * MyWindowScale,
				(bottom - top) * MyWindowScale);

			my_Scaled_image->data = saveData;
		}
	} else
#endif /* EnableMagnify */
	{
#if UseColorImage
#if 0 != vMacScreenDepth
		if (UseColorMode) {
#if vMacScreenDepth < 4

			if (! ColorTransValid) {
				SetUpColorTabl();
				ColorTransValid = true;
			}

			UpdateMappedColorCopy(top, left, bottom, right);

			the_data = (char *)ScalingBuff;
#else
			/*
				if vMacScreenDepth == 5 and MSBFirst, could
				copy directly with the_data = (char *)GetCurDrawBuff();
			*/
			UpdateTransColorCopy(top, left, bottom, right);

			the_data = (char *)ScalingBuff;
#endif
		} else
#endif /* 0 != vMacScreenDepth */
		{
			if (! ColorTransValid) {
				SetUpBW2ColorTabl();
				ColorTransValid = true;
			}

			UpdateMappedBW2ColorCopy(top, left, bottom, right);

			the_data = (char *)ScalingBuff;
		}
#else /* ! UseColorImage */
		{
			the_data = (char *)GetCurDrawBuff();
		}
#endif /* UseColorImage */

		{
			char *saveData = my_image->data;
			my_image->data = the_data;

			XPutImage(x_display, my_main_wind, my_gc, my_image,
				left, top, XDest, YDest,
				right - left, bottom - top);

			my_image->data = saveData;
		}
	}

#if MayFullScreen
label_exit:
	;
#endif
}

static void MyDrawChangesAndClear(void)
{
	if (ScreenChangedBottom > ScreenChangedTop) {
		HaveChangedScreenBuff(ScreenChangedTop, ScreenChangedLeft,
			ScreenChangedBottom, ScreenChangedRight);
		ScreenClearChanges();
	}
}

/* --- mouse --- */

/* cursor hiding */

static bool HaveCursorHidden = false;
static bool WantCursorHidden = false;

static void ForceShowCursor(void)
{
	if (HaveCursorHidden) {
		HaveCursorHidden = false;
		if (my_main_wind) {
			XUndefineCursor(x_display, my_main_wind);
		}
	}
}

static Cursor blankCursor = None;

static bool CreateMyBlankCursor(Window rootwin)
/*
	adapted from X11_CreateNullCursor in context.x11.c
	in quakeforge 0.5.5, copyright Id Software, Inc.
	Zephaniah E. Hull, and Jeff Teunissen.
*/
{
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	bool IsOk = false;

	cursormask = XCreatePixmap(x_display, rootwin, 1, 1, 1);
	if (None == cursormask) {
		WriteExtraErr("XCreatePixmap failed.");
	} else {
		xgc.function = GXclear;
		gc = XCreateGC(x_display, cursormask, GCFunction, &xgc);
		if (None == gc) {
			WriteExtraErr("XCreateGC failed.");
		} else {
			XFillRectangle(x_display, cursormask, gc, 0, 0, 1, 1);
			XFreeGC(x_display, gc);

			blankCursor = XCreatePixmapCursor(x_display, cursormask,
							cursormask, &x_black, &x_white, 0, 0);
			if (None == blankCursor) {
				WriteExtraErr("XCreatePixmapCursor failed.");
			} else {
				IsOk = true;
			}
		}

		XFreePixmap(x_display, cursormask);
		/*
			assuming that XCreatePixmapCursor doesn't think it
			owns the pixmaps passed to it. I've seen code that
			assumes this, and other code that seems to assume
			the opposite.
		*/
	}
	return IsOk;
}

/* cursor moving */

#if EnableMoveMouse
static bool MyMoveMouse(int16_t h, int16_t v)
{
	int NewMousePosh;
	int NewMousePosv;
	int root_x_return;
	int root_y_return;
	Window root_return;
	Window child_return;
	unsigned int mask_return;
	bool IsOk;
	int attempts = 0;

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		h -= ViewHStart;
		v -= ViewVStart;
	}
#endif

#if EnableMagnify
	if (UseMagnify) {
		h *= MyWindowScale;
		v *= MyWindowScale;
	}
#endif

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		h += hOffset;
		v += vOffset;
	}
#endif

	do {
		XWarpPointer(x_display, None, my_main_wind, 0, 0, 0, 0, h, v);
		XQueryPointer(x_display, my_main_wind,
			&root_return, &child_return,
			&root_x_return, &root_y_return,
			&NewMousePosh, &NewMousePosv,
			&mask_return);
		IsOk = (h == NewMousePosh) && (v == NewMousePosv);
		++attempts;
	} while ((! IsOk) && (attempts < 10));
	return IsOk;
}
#endif

#if EnableFSMouseMotion
static void StartSaveMouseMotion(void)
{
	if (! HaveMouseMotion) {
		if (MyMoveMouse(ViewHStart + (ViewHSize / 2),
			ViewVStart + (ViewVSize / 2)))
		{
			SavedMouseH = ViewHStart + (ViewHSize / 2);
			SavedMouseV = ViewVStart + (ViewVSize / 2);
			HaveMouseMotion = true;
		}
	}
}
#endif

#if EnableFSMouseMotion
static void StopSaveMouseMotion(void)
{
	if (HaveMouseMotion) {
		(void) MyMoveMouse(CurMouseH, CurMouseV);
		HaveMouseMotion = false;
	}
}
#endif

/* cursor state */

#if EnableFSMouseMotion
static void MyMouseConstrain(void)
{
	int16_t shiftdh;
	int16_t shiftdv;

	if (SavedMouseH < ViewHStart + (ViewHSize / 4)) {
		shiftdh = ViewHSize / 2;
	} else if (SavedMouseH > ViewHStart + ViewHSize - (ViewHSize / 4)) {
		shiftdh = - ViewHSize / 2;
	} else {
		shiftdh = 0;
	}
	if (SavedMouseV < ViewVStart + (ViewVSize / 4)) {
		shiftdv = ViewVSize / 2;
	} else if (SavedMouseV > ViewVStart + ViewVSize - (ViewVSize / 4)) {
		shiftdv = - ViewVSize / 2;
	} else {
		shiftdv = 0;
	}
	if ((shiftdh != 0) || (shiftdv != 0)) {
		SavedMouseH += shiftdh;
		SavedMouseV += shiftdv;
		if (! MyMoveMouse(SavedMouseH, SavedMouseV)) {
			HaveMouseMotion = false;
		}
	}
}
#endif

static void MousePositionNotify(int NewMousePosh, int NewMousePosv)
{
	bool ShouldHaveCursorHidden = true;

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		NewMousePosh -= hOffset;
		NewMousePosv -= vOffset;
	}
#endif

#if EnableMagnify
	if (UseMagnify) {
		NewMousePosh /= MyWindowScale;
		NewMousePosv /= MyWindowScale;
	}
#endif

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		NewMousePosh += ViewHStart;
		NewMousePosv += ViewVStart;
	}
#endif

#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		MyMousePositionSetDelta(NewMousePosh - SavedMouseH,
			NewMousePosv - SavedMouseV);
		SavedMouseH = NewMousePosh;
		SavedMouseV = NewMousePosv;
	} else
#endif
	{
		if (NewMousePosh < 0) {
			NewMousePosh = 0;
			ShouldHaveCursorHidden = false;
		} else if (NewMousePosh >= vMacScreenWidth) {
			NewMousePosh = vMacScreenWidth - 1;
			ShouldHaveCursorHidden = false;
		}
		if (NewMousePosv < 0) {
			NewMousePosv = 0;
			ShouldHaveCursorHidden = false;
		} else if (NewMousePosv >= vMacScreenHeight) {
			NewMousePosv = vMacScreenHeight - 1;
			ShouldHaveCursorHidden = false;
		}

#if VarFullScreen
		if (UseFullScreen)
#endif
#if MayFullScreen
		{
			ShouldHaveCursorHidden = true;
		}
#endif

		/* if (ShouldHaveCursorHidden || CurMouseButton) */
		/*
			for a game like arkanoid, would like mouse to still
			move even when outside window in one direction
		*/
		MyMousePositionSet(NewMousePosh, NewMousePosv);
	}

	WantCursorHidden = ShouldHaveCursorHidden;
}

static void CheckMouseState(void)
{
	int NewMousePosh;
	int NewMousePosv;
	int root_x_return;
	int root_y_return;
	Window root_return;
	Window child_return;
	unsigned int mask_return;

	XQueryPointer(x_display, my_main_wind,
		&root_return, &child_return,
		&root_x_return, &root_y_return,
		&NewMousePosh, &NewMousePosv,
		&mask_return);
	MousePositionNotify(NewMousePosh, NewMousePosv);
}

/* --- keyboard input --- */

/*
	translation table - X11 KeySym -> Mac key code

	Used to create KC2MKC table (X11 key code -> Mac key code)

	Includes effect of any key mapping set with the
	mini vmac '-km' compile time option.

	The real CapsLock key needs special treatment,
	so use MKC_real_CapsLock here,
	which is later remapped to MKC_formac_CapsLock.

	Ordered to match order of keycodes on Linux, the most
	common port using this X11 code, making the code using
	this table more efficient.
*/

/*
	The actual data is in the comments of this enum,
	from which MT2KeySym and MT2MKC are created by script.
*/
enum {
	kMT_Escape, /* XK_Escape MKC_formac_Escape */
	kMT_1, /* XK_1 MKC_1 */
	kMT_2, /* XK_2 MKC_2 */
	kMT_3, /* XK_3 MKC_3 */
	kMT_4, /* XK_4 MKC_4 */
	kMT_5, /* XK_5 MKC_5 */
	kMT_6, /* XK_6 MKC_6 */
	kMT_7, /* XK_7 MKC_7 */
	kMT_8, /* XK_8 MKC_8 */
	kMT_9, /* XK_9 MKC_9 */
	kMT_0, /* XK_0 MKC_0 */
	kMT_minus, /* XK_minus MKC_Minus */
	kMT_underscore, /* XK_underscore MKC_Minus */
	kMT_equal, /* XK_equal MKC_Equal */
	kMT_plus, /* XK_plus MKC_Equal */
	kMT_BackSpace, /* XK_BackSpace MKC_BackSpace */
	kMT_Tab, /* XK_Tab MKC_Tab */
	kMT_q, /* XK_q MKC_Q */
	kMT_Q, /* XK_Q MKC_Q */
	kMT_w, /* XK_w MKC_W */
	kMT_W, /* XK_W MKC_W */
	kMT_e, /* XK_e MKC_E */
	kMT_E, /* XK_E MKC_E */
	kMT_r, /* XK_r MKC_R */
	kMT_R, /* XK_R MKC_R */
	kMT_t, /* XK_t MKC_T */
	kMT_T, /* XK_T MKC_T */
	kMT_y, /* XK_y MKC_Y */
	kMT_Y, /* XK_Y MKC_Y */
	kMT_u, /* XK_u MKC_U */
	kMT_U, /* XK_U MKC_U */
	kMT_i, /* XK_i MKC_I */
	kMT_I, /* XK_I MKC_I */
	kMT_o, /* XK_o MKC_O */
	kMT_O, /* XK_O MKC_O */
	kMT_p, /* XK_p MKC_P */
	kMT_P, /* XK_P MKC_P */
	kMT_bracketleft, /* XK_bracketleft MKC_LeftBracket */
	kMT_braceleft, /* XK_braceleft MKC_LeftBracket */
	kMT_bracketright, /* XK_bracketright MKC_RightBracket */
	kMT_braceright, /* XK_braceright MKC_RightBracket */
	kMT_Return, /* XK_Return MKC_Return */
	kMT_Control_L, /* XK_Control_L MKC_formac_Control */
	kMT_a, /* XK_a MKC_A */
	kMT_A, /* XK_A MKC_A */
	kMT_s, /* XK_s MKC_S */
	kMT_S, /* XK_S MKC_S */
	kMT_d, /* XK_d MKC_D */
	kMT_D, /* XK_D MKC_D */
	kMT_f, /* XK_f MKC_F */
	kMT_F, /* XK_F MKC_F */
	kMT_g, /* XK_g MKC_G */
	kMT_G, /* XK_G MKC_G */
	kMT_h, /* XK_h MKC_H */
	kMT_H, /* XK_H MKC_H */
	kMT_j, /* XK_j MKC_J */
	kMT_J, /* XK_J MKC_J */
	kMT_k, /* XK_k MKC_K */
	kMT_K, /* XK_K MKC_K */
	kMT_l, /* XK_l MKC_L */
	kMT_L, /* XK_L MKC_L */
	kMT_semicolon, /* XK_semicolon MKC_SemiColon */
	kMT_colon, /* XK_colon MKC_SemiColon */
	kMT_apostrophe, /* XK_apostrophe MKC_SingleQuote */
	kMT_quotedbl, /* XK_quotedbl MKC_SingleQuote */
	kMT_grave, /* XK_grave MKC_formac_Grave */
	kMT_asciitilde, /* XK_asciitilde MKC_formac_Grave */
	kMT_Shift_L, /* XK_Shift_L MKC_formac_Shift */
	kMT_backslash, /* XK_backslash MKC_formac_BackSlash */
	kMT_bar, /* XK_bar MKC_formac_BackSlash */
	kMT_z, /* XK_z MKC_Z */
	kMT_Z, /* XK_Z MKC_Z */
	kMT_x, /* XK_x MKC_X */
	kMT_X, /* XK_X MKC_X */
	kMT_c, /* XK_c MKC_C */
	kMT_C, /* XK_C MKC_C */
	kMT_v, /* XK_v MKC_V */
	kMT_V, /* XK_V MKC_V */
	kMT_b, /* XK_b MKC_B */
	kMT_B, /* XK_B MKC_B */
	kMT_n, /* XK_n MKC_N */
	kMT_N, /* XK_N MKC_N */
	kMT_m, /* XK_m MKC_M */
	kMT_M, /* XK_M MKC_M */
	kMT_comma, /* XK_comma MKC_Comma */
	kMT_period, /* XK_period MKC_Period */
	kMT_greater, /* XK_greater MKC_Period */
	kMT_slash, /* XK_slash MKC_formac_Slash */
	kMT_question, /* XK_question MKC_formac_Slash */
	kMT_Shift_R, /* XK_Shift_R MKC_formac_RShift */
	kMT_KP_Multiply, /* XK_KP_Multiply MKC_KPMultiply */
	kMT_Alt_L, /* XK_Alt_L MKC_formac_Command */
	kMT_space, /* XK_space MKC_Space */
	kMT_Caps_Lock, /* XK_Caps_Lock MKC_real_CapsLock */
	kMT_F1, /* XK_F1 MKC_formac_F1 */
	kMT_F2, /* XK_F2 MKC_formac_F2 */
	kMT_F3, /* XK_F3 MKC_formac_F3 */
	kMT_F4, /* XK_F4 MKC_formac_F4 */
	kMT_F5, /* XK_F5 MKC_formac_F5 */
	kMT_F6, /* XK_F6 MKC_F6 */
	kMT_F7, /* XK_F7 MKC_F7 */
	kMT_F8, /* XK_F8 MKC_F8 */
	kMT_F9, /* XK_F9 MKC_F9 */
	kMT_F10, /* XK_F10 MKC_F10 */
	kMT_Num_Lock, /* XK_Num_Lock MKC_Clear */
#ifdef XK_Scroll_Lock
	kMT_Scroll_Lock, /* XK_Scroll_Lock MKC_ScrollLock */
#endif
#ifdef XK_F14
	kMT_F14, /* XK_F14 MKC_ScrollLock */
#endif

	kMT_KP_7, /* XK_KP_7 MKC_KP7 */
#ifdef XK_KP_Home
	kMT_KP_Home, /* XK_KP_Home MKC_KP7 */
#endif

	kMT_KP_8, /* XK_KP_8 MKC_KP8 */
#ifdef XK_KP_Up
	kMT_KP_Up, /* XK_KP_Up MKC_KP8 */
#endif

	kMT_KP_9, /* XK_KP_9 MKC_KP9 */
#ifdef XK_KP_Page_Up
	kMT_KP_Page_Up, /* XK_KP_Page_Up MKC_KP9 */
#else
#ifdef XK_KP_Prior
	kMT_KP_Prior, /* XK_KP_Prior MKC_KP9 */
#endif
#endif

	kMT_KP_Subtract, /* XK_KP_Subtract MKC_KPSubtract */

	kMT_KP_4, /* XK_KP_4 MKC_KP4 */
#ifdef XK_KP_Left
	kMT_KP_Left, /* XK_KP_Left MKC_KP4 */
#endif

	kMT_KP_5, /* XK_KP_5 MKC_KP5 */
#ifdef XK_KP_Begin
	kMT_KP_Begin, /* XK_KP_Begin MKC_KP5 */
#endif

	kMT_KP_6, /* XK_KP_6 MKC_KP6 */
#ifdef XK_KP_Right
	kMT_KP_Right, /* XK_KP_Right MKC_KP6 */
#endif

	kMT_KP_Add, /* XK_KP_Add MKC_KPAdd */

	kMT_KP_1, /* XK_KP_1 MKC_KP1 */
#ifdef XK_KP_End
	kMT_KP_End, /* XK_KP_End MKC_KP1 */
#endif

	kMT_KP_2, /* XK_KP_2 MKC_KP2 */
#ifdef XK_KP_Down
	kMT_KP_Down, /* XK_KP_Down MKC_KP2 */
#endif

	kMT_KP_3, /* XK_KP_3 MKC_KP3 */
#ifdef XK_Page_Down
	kMT_KP_Page_Down, /* XK_KP_Page_Down MKC_KP3 */
#else
#ifdef XK_KP_Next
	kMT_KP_Next, /* XK_KP_Next MKC_KP3 */
#endif
#endif

	kMT_KP_0, /* XK_KP_0 MKC_KP0 */
#ifdef XK_KP_Insert
	kMT_KP_Insert, /* XK_KP_Insert MKC_KP0 */
#endif
#ifdef XK_KP_Delete
	kMT_KP_Delete, /* XK_KP_Delete MKC_Decimal */
#endif

	/* XK_ISO_Level3_Shift */
	/* nothing */
#ifdef XK_less
	kMT_less, /* XK_less MKC_Comma */
#endif
	kMT_F11, /* XK_F11 MKC_F11 */
	kMT_F12, /* XK_F12 MKC_F12 */
	/* nothing */
	/* XK_Katakana */
	/* XK_Hiragana */
	/* XK_Henkan */
	/* XK_Hiragana_Katakana */
	/* XK_Muhenkan */
	/* nothing */
	kMT_KP_Enter, /* XK_KP_Enter MKC_formac_Enter */
	kMT_Control_R, /* XK_Control_R MKC_formac_RControl */
	kMT_KP_Divide, /* XK_KP_Divide MKC_KPDevide */
#ifdef XK_Print
	kMT_Print, /* XK_Print MKC_Print */
#endif
	kMT_Alt_R, /* XK_Alt_R MKC_formac_RCommand */
	/* XK_Linefeed */
#ifdef XK_Home
	kMT_Home, /* XK_Home MKC_formac_Home */
#endif
	kMT_Up, /* XK_Up MKC_Up */

#ifdef XK_Page_Up
	kMT_Page_Up, /* XK_Page_Up MKC_formac_PageUp */
#else
#ifdef XK_Prior
	kMT_Prior, /* XK_Prior MKC_formac_PageUp */
#endif
#endif

	kMT_Left, /* XK_Left MKC_Left */
	kMT_Right, /* XK_Right MKC_Right */
#ifdef XK_End
	kMT_End, /* XK_End MKC_formac_End */
#endif
	kMT_Down, /* XK_Down MKC_Down */

#ifdef XK_Page_Down
	kMT_Page_Down, /* XK_Page_Down MKC_formac_PageDown */
#else
#ifdef XK_Next
	kMT_Next, /* XK_Next MKC_formac_PageDown */
#endif
#endif

#ifdef XK_Insert
	kMT_Insert, /* XK_Insert MKC_formac_Help */
#endif
#ifdef XK_Delete
	kMT_Delete, /* XK_Delete MKC_formac_ForwardDel */
#endif
	/* nothing */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	kMT_KP_Equal, /* XK_KP_Equal MKC_KPEqual */
	/* XK_plusminus */
#ifdef XK_Pause
	kMT_Pause, /* XK_Pause MKC_Pause */
#endif
#ifdef XK_F15
	kMT_F15, /* XK_F15 MKC_Pause */
#endif
	/* ? */
	kMT_KP_Decimal, /* XK_KP_Decimal MKC_Decimal */
	/* XK_Hangul */
	/* XK_Hangul_Hanja */
	/* nothing */
	kMT_Super_L, /* XK_Super_L MKC_formac_Option */
	kMT_Super_R, /* XK_Super_R MKC_formac_ROption */
	kMT_Menu, /* XK_Menu MKC_formac_Option */
	/* XK_Cancel */
	/* XK_Redo */
	/* ? */
	/* XK_Undo */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	/* XK_Find */
	/* ? */
#ifdef XK_Help
	kMT_Help, /* XK_Help MKC_formac_Help */
#endif
	/* ? */
	/* ? */
	/* nothing */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	/* nothing */

	/* XK_parenleft */
	/* XK_parenright */

	/* XK_Mode_switch */



	kMT_Meta_L, /* XK_Meta_L MKC_formac_Command */
	kMT_Meta_R, /* XK_Meta_R MKC_formac_RCommand */

	kMT_Mode_switch, /* XK_Mode_switch MKC_formac_Option */
	kMT_Hyper_L, /* XK_Hyper_L MKC_formac_Option */
	kMT_Hyper_R, /* XK_Hyper_R MKC_formac_ROption */

	kMT_F13, /* XK_F13 MKC_formac_Option */
		/*
			seen being used in Mandrake Linux 9.2
			for windows key
		*/

	kNumMTs
};

/*
	MT2KeySym was generated by a script from
	enum and comments above.
*/
static const KeySym MT2KeySym[kNumMTs + 1] = {
	XK_Escape, /* kMT_Escape */
	XK_1, /* kMT_1 */
	XK_2, /* kMT_2 */
	XK_3, /* kMT_3 */
	XK_4, /* kMT_4 */
	XK_5, /* kMT_5 */
	XK_6, /* kMT_6 */
	XK_7, /* kMT_7 */
	XK_8, /* kMT_8 */
	XK_9, /* kMT_9 */
	XK_0, /* kMT_0 */
	XK_minus, /* kMT_minus */
	XK_underscore, /* kMT_underscore */
	XK_equal, /* kMT_equal */
	XK_plus, /* kMT_plus */
	XK_BackSpace, /* kMT_BackSpace */
	XK_Tab, /* kMT_Tab */
	XK_q, /* kMT_q */
	XK_Q, /* kMT_Q */
	XK_w, /* kMT_w */
	XK_W, /* kMT_W */
	XK_e, /* kMT_e */
	XK_E, /* kMT_E */
	XK_r, /* kMT_r */
	XK_R, /* kMT_R */
	XK_t, /* kMT_t */
	XK_T, /* kMT_T */
	XK_y, /* kMT_y */
	XK_Y, /* kMT_Y */
	XK_u, /* kMT_u */
	XK_U, /* kMT_U */
	XK_i, /* kMT_i */
	XK_I, /* kMT_I */
	XK_o, /* kMT_o */
	XK_O, /* kMT_O */
	XK_p, /* kMT_p */
	XK_P, /* kMT_P */
	XK_bracketleft, /* kMT_bracketleft */
	XK_braceleft, /* kMT_braceleft */
	XK_bracketright, /* kMT_bracketright */
	XK_braceright, /* kMT_braceright */
	XK_Return, /* kMT_Return */
	XK_Control_L, /* kMT_Control_L */
	XK_a, /* kMT_a */
	XK_A, /* kMT_A */
	XK_s, /* kMT_s */
	XK_S, /* kMT_S */
	XK_d, /* kMT_d */
	XK_D, /* kMT_D */
	XK_f, /* kMT_f */
	XK_F, /* kMT_F */
	XK_g, /* kMT_g */
	XK_G, /* kMT_G */
	XK_h, /* kMT_h */
	XK_H, /* kMT_H */
	XK_j, /* kMT_j */
	XK_J, /* kMT_J */
	XK_k, /* kMT_k */
	XK_K, /* kMT_K */
	XK_l, /* kMT_l */
	XK_L, /* kMT_L */
	XK_semicolon, /* kMT_semicolon */
	XK_colon, /* kMT_colon */
	XK_apostrophe, /* kMT_apostrophe */
	XK_quotedbl, /* kMT_quotedbl */
	XK_grave, /* kMT_grave */
	XK_asciitilde, /* kMT_asciitilde */
	XK_Shift_L, /* kMT_Shift_L */
	XK_backslash, /* kMT_backslash */
	XK_bar, /* kMT_bar */
	XK_z, /* kMT_z */
	XK_Z, /* kMT_Z */
	XK_x, /* kMT_x */
	XK_X, /* kMT_X */
	XK_c, /* kMT_c */
	XK_C, /* kMT_C */
	XK_v, /* kMT_v */
	XK_V, /* kMT_V */
	XK_b, /* kMT_b */
	XK_B, /* kMT_B */
	XK_n, /* kMT_n */
	XK_N, /* kMT_N */
	XK_m, /* kMT_m */
	XK_M, /* kMT_M */
	XK_comma, /* kMT_comma */
	XK_period, /* kMT_period */
	XK_greater, /* kMT_greater */
	XK_slash, /* kMT_slash */
	XK_question, /* kMT_question */
	XK_Shift_R, /* kMT_Shift_R */
	XK_KP_Multiply, /* kMT_KP_Multiply */
	XK_Alt_L, /* kMT_Alt_L */
	XK_space, /* kMT_space */
	XK_Caps_Lock, /* kMT_Caps_Lock */
	XK_F1, /* kMT_F1 */
	XK_F2, /* kMT_F2 */
	XK_F3, /* kMT_F3 */
	XK_F4, /* kMT_F4 */
	XK_F5, /* kMT_F5 */
	XK_F6, /* kMT_F6 */
	XK_F7, /* kMT_F7 */
	XK_F8, /* kMT_F8 */
	XK_F9, /* kMT_F9 */
	XK_F10, /* kMT_F10 */
	XK_Num_Lock, /* kMT_Num_Lock */
#ifdef XK_Scroll_Lock
	XK_Scroll_Lock, /* kMT_Scroll_Lock */
#endif
#ifdef XK_F14
	XK_F14, /* kMT_F14 */
#endif

	XK_KP_7, /* kMT_KP_7 */
#ifdef XK_KP_Home
	XK_KP_Home, /* kMT_KP_Home */
#endif

	XK_KP_8, /* kMT_KP_8 */
#ifdef XK_KP_Up
	XK_KP_Up, /* kMT_KP_Up */
#endif

	XK_KP_9, /* kMT_KP_9 */
#ifdef XK_KP_Page_Up
	XK_KP_Page_Up, /* kMT_KP_Page_Up */
#else
#ifdef XK_KP_Prior
	XK_KP_Prior, /* kMT_KP_Prior */
#endif
#endif

	XK_KP_Subtract, /* kMT_KP_Subtract */

	XK_KP_4, /* kMT_KP_4 */
#ifdef XK_KP_Left
	XK_KP_Left, /* kMT_KP_Left */
#endif

	XK_KP_5, /* kMT_KP_5 */
#ifdef XK_KP_Begin
	XK_KP_Begin, /* kMT_KP_Begin */
#endif

	XK_KP_6, /* kMT_KP_6 */
#ifdef XK_KP_Right
	XK_KP_Right, /* kMT_KP_Right */
#endif

	XK_KP_Add, /* kMT_KP_Add */

	XK_KP_1, /* kMT_KP_1 */
#ifdef XK_KP_End
	XK_KP_End, /* kMT_KP_End */
#endif

	XK_KP_2, /* kMT_KP_2 */
#ifdef XK_KP_Down
	XK_KP_Down, /* kMT_KP_Down */
#endif

	XK_KP_3, /* kMT_KP_3 */
#ifdef XK_Page_Down
	XK_KP_Page_Down, /* kMT_KP_Page_Down */
#else
#ifdef XK_KP_Next
	XK_KP_Next, /* kMT_KP_Next */
#endif
#endif

	XK_KP_0, /* kMT_KP_0 */
#ifdef XK_KP_Insert
	XK_KP_Insert, /* kMT_KP_Insert */
#endif
#ifdef XK_KP_Delete
	XK_KP_Delete, /* kMT_KP_Delete */
#endif

	/* XK_ISO_Level3_Shift */
	/* nothing */
#ifdef XK_less
	XK_less, /* kMT_less */
#endif
	XK_F11, /* kMT_F11 */
	XK_F12, /* kMT_F12 */
	/* nothing */
	/* XK_Katakana */
	/* XK_Hiragana */
	/* XK_Henkan */
	/* XK_Hiragana_Katakana */
	/* XK_Muhenkan */
	/* nothing */
	XK_KP_Enter, /* kMT_KP_Enter */
	XK_Control_R, /* kMT_Control_R */
	XK_KP_Divide, /* kMT_KP_Divide */
#ifdef XK_Print
	XK_Print, /* kMT_Print */
#endif
	XK_Alt_R, /* kMT_Alt_R */
	/* XK_Linefeed */
#ifdef XK_Home
	XK_Home, /* kMT_Home */
#endif
	XK_Up, /* kMT_Up */

#ifdef XK_Page_Up
	XK_Page_Up, /* kMT_Page_Up */
#else
#ifdef XK_Prior
	XK_Prior, /* kMT_Prior */
#endif
#endif

	XK_Left, /* kMT_Left */
	XK_Right, /* kMT_Right */
#ifdef XK_End
	XK_End, /* kMT_End */
#endif
	XK_Down, /* kMT_Down */

#ifdef XK_Page_Down
	XK_Page_Down, /* kMT_Page_Down */
#else
#ifdef XK_Next
	XK_Next, /* kMT_Next */
#endif
#endif

#ifdef XK_Insert
	XK_Insert, /* kMT_Insert */
#endif
#ifdef XK_Delete
	XK_Delete, /* kMT_Delete */
#endif
	/* nothing */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	XK_KP_Equal, /* kMT_KP_Equal */
	/* XK_plusminus */
#ifdef XK_Pause
	XK_Pause, /* kMT_Pause */
#endif
#ifdef XK_F15
	XK_F15, /* kMT_F15 */
#endif
	/* ? */
	XK_KP_Decimal, /* kMT_KP_Decimal */
	/* XK_Hangul */
	/* XK_Hangul_Hanja */
	/* nothing */
	XK_Super_L, /* kMT_Super_L */
	XK_Super_R, /* kMT_Super_R */
	XK_Menu, /* kMT_Menu */
	/* XK_Cancel */
	/* XK_Redo */
	/* ? */
	/* XK_Undo */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	/* XK_Find */
	/* ? */
#ifdef XK_Help
	XK_Help, /* kMT_Help */
#endif
	/* ? */
	/* ? */
	/* nothing */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	/* nothing */

	/* XK_parenleft */
	/* XK_parenright */

	/* XK_Mode_switch */



	XK_Meta_L, /* kMT_Meta_L */
	XK_Meta_R, /* kMT_Meta_R */

	XK_Mode_switch, /* kMT_Mode_switch */
	XK_Hyper_L, /* kMT_Hyper_L */
	XK_Hyper_R, /* kMT_Hyper_R */

	XK_F13, /* kMT_F13 */
		/*
			seen being used in Mandrake Linux 9.2
			for windows key
		*/

	0 /* just so last above line can end in ',' */
};

/*
	MT2MKC was generated by a script from
	enum and comments above.
*/
static const uint8_t MT2MKC[kNumMTs + 1] = {
	MKC_formac_Escape, /* kMT_Escape */
	MKC_1, /* kMT_1 */
	MKC_2, /* kMT_2 */
	MKC_3, /* kMT_3 */
	MKC_4, /* kMT_4 */
	MKC_5, /* kMT_5 */
	MKC_6, /* kMT_6 */
	MKC_7, /* kMT_7 */
	MKC_8, /* kMT_8 */
	MKC_9, /* kMT_9 */
	MKC_0, /* kMT_0 */
	MKC_Minus, /* kMT_minus */
	MKC_Minus, /* kMT_underscore */
	MKC_Equal, /* kMT_equal */
	MKC_Equal, /* kMT_plus */
	MKC_BackSpace, /* kMT_BackSpace */
	MKC_Tab, /* kMT_Tab */
	MKC_Q, /* kMT_q */
	MKC_Q, /* kMT_Q */
	MKC_W, /* kMT_w */
	MKC_W, /* kMT_W */
	MKC_E, /* kMT_e */
	MKC_E, /* kMT_E */
	MKC_R, /* kMT_r */
	MKC_R, /* kMT_R */
	MKC_T, /* kMT_t */
	MKC_T, /* kMT_T */
	MKC_Y, /* kMT_y */
	MKC_Y, /* kMT_Y */
	MKC_U, /* kMT_u */
	MKC_U, /* kMT_U */
	MKC_I, /* kMT_i */
	MKC_I, /* kMT_I */
	MKC_O, /* kMT_o */
	MKC_O, /* kMT_O */
	MKC_P, /* kMT_p */
	MKC_P, /* kMT_P */
	MKC_LeftBracket, /* kMT_bracketleft */
	MKC_LeftBracket, /* kMT_braceleft */
	MKC_RightBracket, /* kMT_bracketright */
	MKC_RightBracket, /* kMT_braceright */
	MKC_Return, /* kMT_Return */
	MKC_formac_Control, /* kMT_Control_L */
	MKC_A, /* kMT_a */
	MKC_A, /* kMT_A */
	MKC_S, /* kMT_s */
	MKC_S, /* kMT_S */
	MKC_D, /* kMT_d */
	MKC_D, /* kMT_D */
	MKC_F, /* kMT_f */
	MKC_F, /* kMT_F */
	MKC_G, /* kMT_g */
	MKC_G, /* kMT_G */
	MKC_H, /* kMT_h */
	MKC_H, /* kMT_H */
	MKC_J, /* kMT_j */
	MKC_J, /* kMT_J */
	MKC_K, /* kMT_k */
	MKC_K, /* kMT_K */
	MKC_L, /* kMT_l */
	MKC_L, /* kMT_L */
	MKC_SemiColon, /* kMT_semicolon */
	MKC_SemiColon, /* kMT_colon */
	MKC_SingleQuote, /* kMT_apostrophe */
	MKC_SingleQuote, /* kMT_quotedbl */
	MKC_formac_Grave, /* kMT_grave */
	MKC_formac_Grave, /* kMT_asciitilde */
	MKC_formac_Shift, /* kMT_Shift_L */
	MKC_formac_BackSlash, /* kMT_backslash */
	MKC_formac_BackSlash, /* kMT_bar */
	MKC_Z, /* kMT_z */
	MKC_Z, /* kMT_Z */
	MKC_X, /* kMT_x */
	MKC_X, /* kMT_X */
	MKC_C, /* kMT_c */
	MKC_C, /* kMT_C */
	MKC_V, /* kMT_v */
	MKC_V, /* kMT_V */
	MKC_B, /* kMT_b */
	MKC_B, /* kMT_B */
	MKC_N, /* kMT_n */
	MKC_N, /* kMT_N */
	MKC_M, /* kMT_m */
	MKC_M, /* kMT_M */
	MKC_Comma, /* kMT_comma */
	MKC_Period, /* kMT_period */
	MKC_Period, /* kMT_greater */
	MKC_formac_Slash, /* kMT_slash */
	MKC_formac_Slash, /* kMT_question */
	MKC_formac_RShift, /* kMT_Shift_R */
	MKC_KPMultiply, /* kMT_KP_Multiply */
	MKC_formac_Command, /* kMT_Alt_L */
	MKC_Space, /* kMT_space */
	MKC_real_CapsLock, /* kMT_Caps_Lock */
	MKC_formac_F1, /* kMT_F1 */
	MKC_formac_F2, /* kMT_F2 */
	MKC_formac_F3, /* kMT_F3 */
	MKC_formac_F4, /* kMT_F4 */
	MKC_formac_F5, /* kMT_F5 */
	MKC_F6, /* kMT_F6 */
	MKC_F7, /* kMT_F7 */
	MKC_F8, /* kMT_F8 */
	MKC_F9, /* kMT_F9 */
	MKC_F10, /* kMT_F10 */
	MKC_Clear, /* kMT_Num_Lock */
#ifdef XK_Scroll_Lock
	MKC_ScrollLock, /* kMT_Scroll_Lock */
#endif
#ifdef XK_F14
	MKC_ScrollLock, /* kMT_F14  */
#endif

	MKC_KP7, /* kMT_KP_7 */
#ifdef XK_KP_Home
	MKC_KP7, /* kMT_KP_Home */
#endif

	MKC_KP8, /* kMT_KP_8 */
#ifdef XK_KP_Up
	MKC_KP8, /* kMT_KP_Up */
#endif

	MKC_KP9, /* kMT_KP_9 */
#ifdef XK_KP_Page_Up
	MKC_KP9, /* kMT_KP_Page_Up */
#else
#ifdef XK_KP_Prior
	MKC_KP9, /* kMT_KP_Prior */
#endif
#endif

	MKC_KPSubtract, /* kMT_KP_Subtract */

	MKC_KP4, /* kMT_KP_4 */
#ifdef XK_KP_Left
	MKC_KP4, /* kMT_KP_Left */
#endif

	MKC_KP5, /* kMT_KP_5 */
#ifdef XK_KP_Begin
	MKC_KP5, /* kMT_KP_Begin */
#endif

	MKC_KP6, /* kMT_KP_6 */
#ifdef XK_KP_Right
	MKC_KP6, /* kMT_KP_Right */
#endif

	MKC_KPAdd, /* kMT_KP_Add */

	MKC_KP1, /* kMT_KP_1 */
#ifdef XK_KP_End
	MKC_KP1, /* kMT_KP_End */
#endif

	MKC_KP2, /* kMT_KP_2 */
#ifdef XK_KP_Down
	MKC_KP2, /* kMT_KP_Down */
#endif

	MKC_KP3, /* kMT_KP_3 */
#ifdef XK_Page_Down
	MKC_KP3, /* kMT_KP_Page_Down */
#else
#ifdef XK_KP_Next
	MKC_KP3, /* kMT_KP_Next */
#endif
#endif

	MKC_KP0, /* kMT_KP_0 */
#ifdef XK_KP_Insert
	MKC_KP0, /* kMT_KP_Insert */
#endif
#ifdef XK_KP_Delete
	MKC_Decimal, /* kMT_KP_Delete */
#endif

	/* XK_ISO_Level3_Shift */
	/* nothing */
#ifdef XK_less
	MKC_Comma, /* kMT_less */
#endif
	MKC_F11, /* kMT_F11 */
	MKC_F12, /* kMT_F12 */
	/* nothing */
	/* XK_Katakana */
	/* XK_Hiragana */
	/* XK_Henkan */
	/* XK_Hiragana_Katakana */
	/* XK_Muhenkan */
	/* nothing */
	MKC_formac_Enter, /* kMT_KP_Enter */
	MKC_formac_RControl, /* kMT_Control_R */
	MKC_KPDevide, /* kMT_KP_Divide */
#ifdef XK_Print
	MKC_Print, /* kMT_Print */
#endif
	MKC_formac_RCommand, /* kMT_Alt_R */
	/* XK_Linefeed */
#ifdef XK_Home
	MKC_formac_Home, /* kMT_Home */
#endif
	MKC_Up, /* kMT_Up */

#ifdef XK_Page_Up
	MKC_formac_PageUp, /* kMT_Page_Up */
#else
#ifdef XK_Prior
	MKC_formac_PageUp, /* kMT_Prior */
#endif
#endif

	MKC_Left, /* kMT_Left */
	MKC_Right, /* kMT_Right */
#ifdef XK_End
	MKC_formac_End, /* kMT_End */
#endif
	MKC_Down, /* kMT_Down */

#ifdef XK_Page_Down
	MKC_formac_PageDown, /* kMT_Page_Down */
#else
#ifdef XK_Next
	MKC_formac_PageDown, /* kMT_Next */
#endif
#endif

#ifdef XK_Insert
	MKC_formac_Help, /* kMT_Insert */
#endif
#ifdef XK_Delete
	MKC_formac_ForwardDel, /* kMT_Delete */
#endif
	/* nothing */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	MKC_KPEqual, /* kMT_KP_Equal */
	/* XK_plusminus */
#ifdef XK_Pause
	MKC_Pause, /* kMT_Pause */
#endif
#ifdef XK_F15
	MKC_Pause, /* kMT_F15  */
#endif
	/* ? */
	MKC_Decimal, /* kMT_KP_Decimal */
	/* XK_Hangul */
	/* XK_Hangul_Hanja */
	/* nothing */
	MKC_formac_Option, /* kMT_Super_L */
	MKC_formac_ROption, /* kMT_Super_R */
	MKC_formac_Option, /* kMT_Menu */
	/* XK_Cancel */
	/* XK_Redo */
	/* ? */
	/* XK_Undo */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	/* XK_Find */
	/* ? */
#ifdef XK_Help
	MKC_formac_Help, /* kMT_Help */
#endif
	/* ? */
	/* ? */
	/* nothing */
	/* ? */
	/* ? */
	/* ? */
	/* ? */
	/* nothing */

	/* XK_parenleft */
	/* XK_parenright */

	/* XK_Mode_switch */



	MKC_formac_Command, /* kMT_Meta_L */
	MKC_formac_RCommand, /* kMT_Meta_R */

	MKC_formac_Option, /* kMT_Mode_switch */
	MKC_formac_Option, /* kMT_Hyper_L */
	MKC_formac_ROption, /* kMT_Hyper_R */

	MKC_formac_Option, /* kMT_F13 */
		/*
			seen being used in Mandrake Linux 9.2
			for windows key
		*/

	0 /* just so last above line can end in ',' */
};

static uint8_t KC2MKC[256];
	/*
		translate X11 key code to Macintosh key code
	*/

#define KMInit_dolog (dbglog_HAVE && 0)

static bool KC2MKCInit(void)
{
	int i;
	int j;
	int last_j;
	int first_keycode;
	int last_keycode;
	int keysyms_per_keycode;
	KeySym *KeyMap;
	KeySym MaxUsedKeySym;

	/*
		In Linux, observe that most KeySyms not
		found in our translation table are large.
		So saves time to find largest KeySym we
		are interested in.
	*/
	MaxUsedKeySym = 0;
	for (j = 0; j < kNumMTs; j++) {
		KeySym x = MT2KeySym[j];
		if (x > MaxUsedKeySym) {
			MaxUsedKeySym = x;
		}
	}

#if KMInit_dolog
	dbglog_writelnHex("MaxUsedKeySym", MaxUsedKeySym);
#endif

	for (i = 0; i < 256; ++i) {
		KC2MKC[i] = MKC_None;
	}

	XDisplayKeycodes(x_display, &first_keycode, &last_keycode);
	KeyMap = XGetKeyboardMapping(x_display,
		first_keycode,
		last_keycode - first_keycode + 1,
		&keysyms_per_keycode);

	last_j = kNumMTs - 1;

	for (i = first_keycode; i <= last_keycode; i++) {
		KeySym ks = KeyMap[(i - first_keycode) * keysyms_per_keycode];

#if KMInit_dolog
		dbglog_writeNum(i);
		dbglog_writeSpace();
		dbglog_writeHex(ks);
		dbglog_writeSpace();
#endif
		if (0 == ks) {
#if KMInit_dolog
			dbglog_writeCStr("zero");
#endif
		} else
		if (ks > MaxUsedKeySym) {
#if KMInit_dolog
			dbglog_writeCStr("too large");
#endif
		} else
		{
			/*
				look up in the translation table, and try to be more
				efficient if the order of this table is similar
				to the order of the X11 KeyboardMapping.
			*/
			j = last_j;
label_retry:
			++j;
			if (j >= kNumMTs) {
				j = 0;
			}

			if (j == last_j) {
				/* back where we started */
#if KMInit_dolog
				dbglog_writeCStr("not found");
#endif
			} else
			if (ks != MT2KeySym[j]) {
#if KMInit_dolog && 1
				dbglog_writeCStr("*");
#endif
				goto label_retry; /* try the next one */
			} else
			{
#if KMInit_dolog
				dbglog_writeCStr("match");
				dbglog_writeSpace();
				dbglog_writeHex(MT2MKC[j]);
#endif
				KC2MKC[i] = MT2MKC[j];
				last_j = j;
			}
		}
#if KMInit_dolog
		dbglog_writeReturn();
#endif
	}

	XFree(KeyMap);

	InitKeyCodes();

	return true;
}

static void CheckTheCapsLock(void)
{
	int NewMousePosh;
	int NewMousePosv;
	int root_x_return;
	int root_y_return;
	Window root_return;
	Window child_return;
	unsigned int mask_return;

	XQueryPointer(x_display, my_main_wind,
		&root_return, &child_return,
		&root_x_return, &root_y_return,
		&NewMousePosh, &NewMousePosv,
		&mask_return);

	Keyboard_UpdateKeyMap2(MKC_formac_CapsLock,
		(mask_return & LockMask) != 0);
}

static void DoKeyCode0(int i, bool down)
{
	uint8_t key = KC2MKC[i];

	if (MKC_None == key) {
		/* ignore */
	} else
	if (MKC_real_CapsLock == key) {
		/* also ignore */
	} else
	{
		Keyboard_UpdateKeyMap2(key, down);
	}
}

static void DoKeyCode(int i, bool down)
{
	if ((i >= 0) && (i < 256)) {
		uint8_t key = KC2MKC[i];

		if (MKC_None == key) {
			/* ignore */
		} else
		if (MKC_real_CapsLock == key) {
			CheckTheCapsLock();
		} else
		{
			Keyboard_UpdateKeyMap2(key, down);
		}
	}
}

#if MayFullScreen && GrabKeysFullScreen
static bool KeyboardIsGrabbed = false;
#endif

#if MayFullScreen && GrabKeysFullScreen
static void MyGrabKeyboard(void)
{
	if (! KeyboardIsGrabbed) {
		(void) XGrabKeyboard(x_display, my_main_wind,
			False, GrabModeAsync, GrabModeAsync,
			CurrentTime);
		KeyboardIsGrabbed = true;
	}
}
#endif

#if MayFullScreen && GrabKeysFullScreen
static void MyUnGrabKeyboard(void)
{
	if (KeyboardIsGrabbed && my_main_wind) {
		XUngrabKeyboard(x_display, CurrentTime);
		KeyboardIsGrabbed = false;
	}
}
#endif

static bool NoKeyRepeat = false;
static int SaveKeyRepeat;

static void DisableKeyRepeat(void)
{
	XKeyboardState r;
	XKeyboardControl k;

	if ((! NoKeyRepeat) && (x_display != NULL)) {
		NoKeyRepeat = true;

		XGetKeyboardControl(x_display, &r);
		SaveKeyRepeat = r.global_auto_repeat;

		k.auto_repeat_mode = AutoRepeatModeOff;
		XChangeKeyboardControl(x_display, KBAutoRepeatMode, &k);
	}
}

static void RestoreKeyRepeat(void)
{
	XKeyboardControl k;

	if (NoKeyRepeat && (x_display != NULL)) {
		NoKeyRepeat = false;

		k.auto_repeat_mode = SaveKeyRepeat;
		XChangeKeyboardControl(x_display, KBAutoRepeatMode, &k);
	}
}

static bool WantCmdOptOnReconnect = false;

static void GetTheDownKeys(void)
{
	char keys_return[32];
	int i;
	int v;
	int j;

	XQueryKeymap(x_display, keys_return);

	for (i = 0; i < 32; ++i) {
		v = keys_return[i];
		for (j = 0; j < 8; ++j) {
			if (0 != ((1 << j) & v)) {
				int k = i * 8 + j;

				DoKeyCode0(k, true);
			}
		}
	}
}

static void ReconnectKeyCodes3(void)
{
	CheckTheCapsLock();

	if (WantCmdOptOnReconnect) {
		WantCmdOptOnReconnect = false;

		GetTheDownKeys();
	}
}

static void DisconnectKeyCodes3(void)
{
	DisconnectKeyCodes2();
	MyMouseButtonSet(false);
}

/* --- time, date, location --- */

#define dbglog_TimeStuff (0 && dbglog_HAVE)

static uint32_t TrueEmulatedTime = 0;

#include "platform/common/date_to_sec.h"

#define TicksPerSecond 1000000

static bool HaveTimeDelta = false;
static uint32_t TimeDelta;

static uint32_t NewMacDateInSeconds;

static uint32_t LastTimeSec;
static uint32_t LastTimeUsec;

static void GetCurrentTicks(void)
{
	struct timeval t;

	gettimeofday(&t, NULL);
	if (! HaveTimeDelta) {
		time_t Current_Time;
		struct tm *s;

		(void) time(&Current_Time);
		s = localtime(&Current_Time);
		TimeDelta = Date2MacSeconds(s->tm_sec, s->tm_min, s->tm_hour,
			s->tm_mday, 1 + s->tm_mon, 1900 + s->tm_year) - t.tv_sec;
#if 0 && AutoTimeZone /* how portable is this ? */
		CurMacDelta = ((uint32_t)(s->tm_gmtoff) & 0x00FFFFFF)
			| ((s->tm_isdst ? 0x80 : 0) << 24);
#endif
		HaveTimeDelta = true;
	}

	NewMacDateInSeconds = t.tv_sec + TimeDelta;
	LastTimeSec = (uint32_t)t.tv_sec;
	LastTimeUsec = (uint32_t)t.tv_usec;
}

#define MyInvTimeStep 16626 /* TicksPerSecond / 60.14742 */

static uint32_t NextTimeSec;
static uint32_t NextTimeUsec;

static void IncrNextTime(void)
{
	NextTimeUsec += MyInvTimeStep;
	if (NextTimeUsec >= TicksPerSecond) {
		NextTimeUsec -= TicksPerSecond;
		NextTimeSec += 1;
	}
}

static void InitNextTime(void)
{
	NextTimeSec = LastTimeSec;
	NextTimeUsec = LastTimeUsec;
	IncrNextTime();
}

static void StartUpTimeAdjust(void)
{
	GetCurrentTicks();
	InitNextTime();
}

static int32_t GetTimeDiff(void)
{
	return ((int32_t)(LastTimeSec - NextTimeSec)) * TicksPerSecond
		+ ((int32_t)(LastTimeUsec - NextTimeUsec));
}

static void UpdateTrueEmulatedTime(void)
{
	int32_t TimeDiff;

	GetCurrentTicks();

	TimeDiff = GetTimeDiff();
	if (TimeDiff >= 0) {
		if (TimeDiff > 16 * MyInvTimeStep) {
			/* emulation interrupted, forget it */
			++TrueEmulatedTime;
			InitNextTime();

#if dbglog_TimeStuff
			dbglog_writelnNum("emulation interrupted",
				TrueEmulatedTime);
#endif
		} else {
			do {
				++TrueEmulatedTime;
				IncrNextTime();
				TimeDiff -= TicksPerSecond;
			} while (TimeDiff >= 0);
		}
	} else if (TimeDiff < - 16 * MyInvTimeStep) {
		/* clock goofed if ever get here, reset */
#if dbglog_TimeStuff
		dbglog_writeln("clock set back");
#endif

		InitNextTime();
	}
}

static bool CheckDateTime(void)
{
	if (CurMacDateInSeconds != NewMacDateInSeconds) {
		CurMacDateInSeconds = NewMacDateInSeconds;
		return true;
	} else {
		return false;
	}
}

static bool InitLocationDat(void)
{
	GetCurrentTicks();
	CurMacDateInSeconds = NewMacDateInSeconds;

	return true;
}

/* --- sound --- */

#if MySoundEnabled

#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)

#define DesiredMinFilledSoundBuffs 3
	/*
		if too big then sound lags behind emulation.
		if too small then sound will have pauses.
	*/

#define kLnOneBuffLen 9
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kLnOneBuffSz (kLnOneBuffLen + kLn2SoundSampSz - 3)
#define kLnAllBuffSz (kLnAllBuffLen + kLn2SoundSampSz - 3)
#define kOneBuffSz (1UL << kLnOneBuffSz)
#define kAllBuffSz (1UL << kLnAllBuffSz)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)
#define dbhBufferSize (kAllBuffSz + kOneBuffSz)

#define dbglog_SoundStuff (0 && dbglog_HAVE)
#define dbglog_SoundBuffStats (0 && dbglog_HAVE)

static tpSoundSamp TheSoundBuffer = nullptr;
static uint16_t ThePlayOffset;
static uint16_t TheFillOffset;
static uint16_t TheWriteOffset;
static uint16_t MinFilledSoundBuffs;

static void MySound_Start0(void)
{
	/* Reset variables */
	ThePlayOffset = 0;
	TheFillOffset = 0;
	TheWriteOffset = 0;
	MinFilledSoundBuffs = kSoundBuffers;
}

 tpSoundSamp MySound_BeginWrite(uint16_t n, uint16_t *actL)
{
	uint16_t ToFillLen = kAllBuffLen - (TheWriteOffset - ThePlayOffset);
	uint16_t WriteBuffContig =
		kOneBuffLen - (TheWriteOffset & kOneBuffMask);

	if (WriteBuffContig < n) {
		n = WriteBuffContig;
	}
	if (ToFillLen < n) {
		/* overwrite previous buffer */
#if dbglog_SoundStuff
		dbglog_writeln("sound buffer over flow");
#endif
		TheWriteOffset -= kOneBuffLen;
	}

	*actL = n;
	return TheSoundBuffer + (TheWriteOffset & kAllBuffMask);
}

static bool MySound_EndWrite0(uint16_t actL)
{
	bool v;

	TheWriteOffset += actL;

	if (0 != (TheWriteOffset & kOneBuffMask)) {
		v = false;
	} else {
		/* just finished a block */

		TheFillOffset = TheWriteOffset;

		v = true;
	}

	return v;
}

static void MySound_SecondNotify0(void)
{
	if (MinFilledSoundBuffs > DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too high");
#endif
		IncrNextTime();
	} else if (MinFilledSoundBuffs < DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too low");
#endif
		++TrueEmulatedTime;
	}
	MinFilledSoundBuffs = kSoundBuffers;
}

#define SOUND_SAMPLERATE 22255 /* = round(7833600 * 2 / 704) */

#include "SOUNDGLU.h"

#endif

/* --- basic dialogs --- */

static void CheckSavedMacMsg(void)
{
	if (nullptr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

		fprintf(stderr, "%s\n", briefMsg0);
		fprintf(stderr, "%s\n", longMsg0);

		SavedBriefMsg = nullptr;
	}
}

/* --- clipboard --- */

#if IncludeHostTextClipExchange
static uint8_t * MyClipBuffer = NULL;
#endif

#if IncludeHostTextClipExchange
static void FreeMyClipBuffer(void)
{
	if (MyClipBuffer != NULL) {
		free(MyClipBuffer);
		MyClipBuffer = NULL;
	}
}
#endif

#if IncludeHostTextClipExchange
 tMacErr HTCEexport(tPbuf i)
{
	tMacErr err = mnvm_miscErr;

	FreeMyClipBuffer();
	if (MacRomanTextToNativePtr(i, false,
		&MyClipBuffer))
	{
		XSetSelectionOwner(x_display, MyXA_CLIPBOARD,
			my_main_wind, CurrentTime);
		err = mnvm_noErr;
	}

	PbufDispose(i);

	return err;
}
#endif

#if IncludeHostTextClipExchange
static bool WaitForClipboardSelection(XEvent *xevent)
{
	struct timespec rqt;
	struct timespec rmt;
	int i;

	for (i = 100; --i >= 0; ) {
		while (XCheckTypedWindowEvent(x_display, my_main_wind,
			SelectionNotify, xevent))
		{
			if (xevent->xselection.selection != MyXA_CLIPBOARD) {
				/*
					not what we were looking for. lose it.
					(and hope it wasn't too important).
				*/
				WriteExtraErr("Discarding unwanted SelectionNotify");
			} else {
				/* this is our event */
				return true;
			}
		}

		rqt.tv_sec = 0;
		rqt.tv_nsec = 10000000;
		(void) nanosleep(&rqt, &rmt);
	}
	return false;
}
#endif

#if IncludeHostTextClipExchange
static void HTCEimport_do(void)
{
	Window w = XGetSelectionOwner(x_display, MyXA_CLIPBOARD);

	if (w == my_main_wind) {
		/* We own the clipboard, already have MyClipBuffer */
	} else {
		FreeMyClipBuffer();
		if (w != None) {
			XEvent xevent;

			XDeleteProperty(x_display, my_main_wind,
				MyXA_MinivMac_Clip);
			XConvertSelection(x_display, MyXA_CLIPBOARD, XA_STRING,
				MyXA_MinivMac_Clip, my_main_wind, CurrentTime);

			if (WaitForClipboardSelection(&xevent)) {
				if (None == xevent.xselection.property) {
					/* oops, target not supported */
				} else {
					if (xevent.xselection.property
						!= MyXA_MinivMac_Clip)
					{
						/* not where we expected it */
					} else {
						Atom ret_type;
						int ret_format;
						unsigned long ret_item;
						unsigned long remain_byte;
						unsigned char *s = NULL;

						if ((Success != XGetWindowProperty(
							x_display, my_main_wind, MyXA_MinivMac_Clip,
							0, 65535, False, AnyPropertyType, &ret_type,
							&ret_format, &ret_item, &remain_byte, &s))
							|| (ret_type != XA_STRING)
							|| (ret_format != 8)
							|| (NULL == s))
						{
							WriteExtraErr(
								"XGetWindowProperty failed"
								" in HTCEimport_do");
						} else {
							MyClipBuffer = (uint8_t *)malloc(ret_item + 1);
							if (NULL == MyClipBuffer) {
								MacMsg(kStrOutOfMemTitle,
									kStrOutOfMemMessage, false);
							} else {
								MyMoveBytes((uint8_t *)s, (uint8_t *)MyClipBuffer,
									ret_item);
								MyClipBuffer[ret_item] = 0;
							}
							XFree(s);
						}
					}
					XDeleteProperty(x_display, my_main_wind,
						MyXA_MinivMac_Clip);
				}
			}
		}
	}
}
#endif

#if IncludeHostTextClipExchange
 tMacErr HTCEimport(tPbuf *r)
{
	HTCEimport_do();

	return NativeTextToMacRomanPbuf((char *)MyClipBuffer, r);
}
#endif

#if IncludeHostTextClipExchange
static bool HandleSelectionRequestClipboard(XEvent *theEvent)
{
	bool RequestFilled = false;

#if MyDbgEvents
	dbglog_writeln("Requested MyXA_CLIPBOARD");
#endif

	if (NULL == MyClipBuffer) {
		/* our clipboard is empty */
	} else if (theEvent->xselectionrequest.target == MyXA_TARGETS) {
		Atom a[2];

		a[0] = MyXA_TARGETS;
		a[1] = XA_STRING;

		XChangeProperty(x_display,
			theEvent->xselectionrequest.requestor,
			theEvent->xselectionrequest.property,
			MyXA_TARGETS,
			32,
				/*
					most, but not all, other programs I've
					look at seem to use 8 here, but that
					can't be right. can it?
				*/
			PropModeReplace,
			(unsigned char *)a,
			sizeof(a) / sizeof(Atom));

		RequestFilled = true;
	} else if (theEvent->xselectionrequest.target == XA_STRING) {
		XChangeProperty(x_display,
			theEvent->xselectionrequest.requestor,
			theEvent->xselectionrequest.property,
			XA_STRING,
			8,
			PropModeReplace,
			(unsigned char *)MyClipBuffer,
			strlen((char *)MyClipBuffer));

		RequestFilled = true;
	}

	return RequestFilled;
}
#endif

/* --- drag and drop --- */

#if EnableDragDrop
static void MyActivateWind(Time time)
{
	if (NetSupportedContains(MyXA_NetActiveWindow)) {
		XEvent xevent;
		Window rootwin = XRootWindow(x_display,
			DefaultScreen(x_display));

		memset(&xevent, 0, sizeof (xevent));

		xevent.xany.type = ClientMessage;
		xevent.xclient.send_event = True;
		xevent.xclient.window = my_main_wind;
		xevent.xclient.message_type = MyXA_NetActiveWindow;
		xevent.xclient.format = 32;
		xevent.xclient.data.l[0] = 1;
		xevent.xclient.data.l[1]= time;

		if (0 == XSendEvent(x_display, rootwin, 0,
			SubstructureRedirectMask | SubstructureNotifyMask,
			&xevent))
		{
			WriteExtraErr("XSendEvent failed in MyActivateWind");
		}
	}

	XRaiseWindow(x_display, my_main_wind);
		/*
			In RedHat 7.1, _NET_ACTIVE_WINDOW supported,
			but XSendEvent of _NET_ACTIVE_WINDOW
			doesn't raise the window. So just always
			call XRaiseWindow. Hopefully calling
			XRaiseWindow won't do any harm on window
			managers where it isn't needed.
			(Such as in Ubuntu 5.10)
		*/
	XSetInputFocus(x_display, my_main_wind,
		RevertToPointerRoot, time);
		/* And call this always too, just in case */
}
#endif

#if EnableDragDrop
static void ParseOneUri(char *s)
{
	/* printf("ParseOneUri %s\n", s); */
	if (('f' == s[0]) && ('i' == s[1]) && ('l' == s[2])
		&& ('e' == s[3]) && (':' == s[4]))
	{
		s += 5;
		if (('/' == s[0]) && ('/' == s[1])) {
			/* skip hostname */
			char c;

			s += 2;
			while ((c = *s) != '/') {
				if (0 == c) {
					return;
				}
				++s;
			}
		}
		(void) Sony_Insert1a(s, false);
	}
}
#endif

#if EnableDragDrop
static int HexChar2Nib(char x)
{
	if ((x >= '0') && (x <= '9')) {
		return x - '0';
	} else if ((x >= 'A') && (x <= 'F')) {
		return x - 'A' + 10;
	} else if ((x >= 'a') && (x <= 'f')) {
		return x - 'a' + 10;
	} else {
		return -1;
	}
}
#endif

#if EnableDragDrop
static void ParseUriList(char *s)
{
	char *p1 = s;
	char *p0 = s;
	char *p = s;
	char c;

	/* printf("ParseUriList %s\n", s); */
	while ((c = *p++) != 0) {
		if ('%' == c) {
			int a;
			int b;

			if (((a = HexChar2Nib(p[0])) >= 0) &&
				((b = HexChar2Nib(p[1])) >= 0))
			{
				p += 2;
				*p1++ = (a << 4) + b;
			} else {
				*p1++ = c;
			}
		} else if (('\n' == c) || ('\r' == c)) {
			*p1++ = 0;
			ParseOneUri(p0);
			p0 = p1;
		} else {
			*p1++ = c;
		}
	}
	*p1++ = 0;
	ParseOneUri(p0);
}
#endif

#if EnableDragDrop
static Window PendingDragWindow = None;
#endif

#if EnableDragDrop
static void HandleSelectionNotifyDnd(XEvent *theEvent)
{
	bool DropOk = false;

#if MyDbgEvents
	dbglog_writeln("Got MyXA_DndSelection");
#endif

	if ((theEvent->xselection.property == MyXA_MinivMac_DndXchng)
		&& (theEvent->xselection.target == MyXA_UriList))
	{
		Atom ret_type;
		int ret_format;
		unsigned long ret_item;
		unsigned long remain_byte;
		unsigned char *s = NULL;

		if ((Success != XGetWindowProperty(x_display, my_main_wind,
			MyXA_MinivMac_DndXchng,
			0, 65535, False, MyXA_UriList, &ret_type, &ret_format,
			&ret_item, &remain_byte, &s))
			|| (NULL == s))
		{
			WriteExtraErr(
				"XGetWindowProperty failed in SelectionNotify");
		} else {
			ParseUriList((char *)s);
			DropOk = true;
			XFree(s);
		}
	} else {
		WriteExtraErr("Got Unknown SelectionNotify");
	}

	XDeleteProperty(x_display, my_main_wind,
		MyXA_MinivMac_DndXchng);

	if (PendingDragWindow != None) {
		XEvent xevent;

		memset(&xevent, 0, sizeof(xevent));

		xevent.xany.type = ClientMessage;
		xevent.xany.display = x_display;
		xevent.xclient.window = PendingDragWindow;
		xevent.xclient.message_type = MyXA_DndFinished;
		xevent.xclient.format = 32;

		xevent.xclient.data.l[0] = my_main_wind;
		if (DropOk) {
			xevent.xclient.data.l[1] = 1;
		}
		xevent.xclient.data.l[2] = MyXA_DndActionPrivate;

		if (0 == XSendEvent(x_display,
			PendingDragWindow, 0, 0, &xevent))
		{
			WriteExtraErr("XSendEvent failed in SelectionNotify");
		}
	}
	if (DropOk && gTrueBackgroundFlag) {
		MyActivateWind(theEvent->xselection.time);

		WantCmdOptOnReconnect = true;
	}
}
#endif

#if EnableDragDrop
static void HandleClientMessageDndPosition(XEvent *theEvent)
{
	XEvent xevent;
	int xr;
	int yr;
	unsigned int dr;
	unsigned int wr;
	unsigned int hr;
	unsigned int bwr;
	Window rr;
	Window srcwin = theEvent->xclient.data.l[0];

#if MyDbgEvents
	dbglog_writeln("Got XdndPosition");
#endif

	XGetGeometry(x_display, my_main_wind,
		&rr, &xr, &yr, &wr, &hr, &bwr, &dr);
	memset (&xevent, 0, sizeof(xevent));
	xevent.xany.type = ClientMessage;
	xevent.xany.display = x_display;
	xevent.xclient.window = srcwin;
	xevent.xclient.message_type = MyXA_DndStatus;
	xevent.xclient.format = 32;

	xevent.xclient.data.l[0] = theEvent->xclient.window;
		/* Target Window */
	xevent.xclient.data.l[1] = 1; /* Accept */
	xevent.xclient.data.l[2] = ((xr) << 16) | ((yr) & 0xFFFFUL);
	xevent.xclient.data.l[3] = ((wr) << 16) | ((hr) & 0xFFFFUL);
	xevent.xclient.data.l[4] = MyXA_DndActionPrivate; /* Action */

	if (0 == XSendEvent(x_display, srcwin, 0, 0, &xevent)) {
		WriteExtraErr(
			"XSendEvent failed in HandleClientMessageDndPosition");
	}
}
#endif

#if EnableDragDrop
static void HandleClientMessageDndDrop(XEvent *theEvent)
{
	Time timestamp = theEvent->xclient.data.l[2];
	PendingDragWindow = (Window) theEvent->xclient.data.l[0];

#if MyDbgEvents
	dbglog_writeln("Got XdndDrop");
#endif

	XConvertSelection(x_display, MyXA_DndSelection, MyXA_UriList,
		MyXA_MinivMac_DndXchng, my_main_wind, timestamp);
}
#endif


#if EmLocalTalk

struct xqpr {
		int NewMousePosh;
		int NewMousePosv;
		int root_x_return;
		int root_y_return;
		Window root_return;
		Window child_return;
		unsigned int mask_return;
};
typedef struct xqpr xqpr;


static bool EntropyGather(void)
{
	/*
		gather some entropy from several places, just in case
		/dev/urandom is not available.
	*/

	{
		struct timeval t;

		gettimeofday(&t, NULL);

		EntropyPoolAddPtr((uint8_t *)&t, sizeof(t) / sizeof(uint8_t));
	}

	{
		xqpr t;

		XQueryPointer(x_display, my_main_wind,
			&t.root_return, &t.child_return,
			&t.root_x_return, &t.root_y_return,
			&t.NewMousePosh, &t.NewMousePosv,
			&t.mask_return);

		EntropyPoolAddPtr((uint8_t *)&t, sizeof(t) / sizeof(uint8_t));
	}

#if 0
	/*
		Another possible source of entropy. But if available,
		almost certainly /dev/urandom is also available.
	*/
	/* #include <sys/sysinfo.h> */
	{
		struct sysinfo t;

		if (0 != sysinfo(&t)) {
#if dbglog_HAVE
			dbglog_writeln("sysinfo fails");
#endif
		}

		/*
			continue even if error, it doesn't hurt anything
				if t is garbage.
		*/
		EntropyPoolAddPtr((uint8_t *)&t, sizeof(t) / sizeof(uint8_t));
	}
#endif

	{
		pid_t t = getpid();

		EntropyPoolAddPtr((uint8_t *)&t, sizeof(t) / sizeof(uint8_t));
	}

	{
		uint32_t dat[2];
		int fd;

		if (-1 == (fd = open("/dev/urandom", O_RDONLY))) {
#if dbglog_HAVE
			dbglog_writeCStr("open /dev/urandom fails");
			dbglog_writeNum(errno);
			dbglog_writeCStr(" (");
			dbglog_writeCStr(strerror(errno));
			dbglog_writeCStr(")");
			dbglog_writeReturn();
#endif
		} else {

			if (read(fd, &dat, sizeof(dat)) < 0) {
#if dbglog_HAVE
				dbglog_writeCStr("open /dev/urandom fails");
				dbglog_writeNum(errno);
				dbglog_writeCStr(" (");
				dbglog_writeCStr(strerror(errno));
				dbglog_writeCStr(")");
				dbglog_writeReturn();
#endif
			} else {

#if dbglog_HAVE
				dbglog_writeCStr("dat: ");
				dbglog_writeHex(dat[0]);
				dbglog_writeCStr(" ");
				dbglog_writeHex(dat[1]);
				dbglog_writeReturn();
#endif

				e_p[0] ^= dat[0];
				e_p[1] ^= dat[1];
					/*
						if "/dev/urandom" is working correctly,
						this should make the previous contents of e_p
						irrelevant. if it is completely broken, like
						returning 0, this will not make e_p any less
						random.
					*/

#if dbglog_HAVE
				dbglog_writeCStr("ep: ");
				dbglog_writeHex(e_p[0]);
				dbglog_writeCStr(" ");
				dbglog_writeHex(e_p[1]);
				dbglog_writeReturn();
#endif
			}

			close(fd);
		}
	}

	return true;
}
#endif

#if EmLocalTalk

#include "platform/localtalk.h"

#endif


#define UseMotionEvents 1

#if UseMotionEvents
static bool CaughtMouse = false;
#endif

#if MayNotFullScreen
static int SavedTransH;
static int SavedTransV;
#endif

/* --- event handling for main window --- */

static void HandleTheEvent(XEvent *theEvent)
{
	if (theEvent->xany.display != x_display) {
		WriteExtraErr("Got event for some other display");
	} else switch(theEvent->type) {
		case KeyPress:
			if (theEvent->xkey.window != my_main_wind) {
				WriteExtraErr("Got KeyPress for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - KeyPress");
#endif

				MousePositionNotify(theEvent->xkey.x, theEvent->xkey.y);
				DoKeyCode(theEvent->xkey.keycode, true);
			}
			break;
		case KeyRelease:
			if (theEvent->xkey.window != my_main_wind) {
				WriteExtraErr("Got KeyRelease for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - KeyRelease");
#endif

				MousePositionNotify(theEvent->xkey.x, theEvent->xkey.y);
				DoKeyCode(theEvent->xkey.keycode, false);
			}
			break;
		case ButtonPress:
			/* any mouse button, we don't care which */
			if (theEvent->xbutton.window != my_main_wind) {
				WriteExtraErr("Got ButtonPress for some other window");
			} else {
				/*
					could check some modifiers, but don't bother for now
					Keyboard_UpdateKeyMap2(MKC_formac_CapsLock,
						(theEvent->xbutton.state & LockMask) != 0);
				*/
				MousePositionNotify(
					theEvent->xbutton.x, theEvent->xbutton.y);
				MyMouseButtonSet(true);
			}
			break;
		case ButtonRelease:
			/* any mouse button, we don't care which */
			if (theEvent->xbutton.window != my_main_wind) {
				WriteExtraErr(
					"Got ButtonRelease for some other window");
			} else {
				MousePositionNotify(
					theEvent->xbutton.x, theEvent->xbutton.y);
				MyMouseButtonSet(false);
			}
			break;
#if UseMotionEvents
		case MotionNotify:
			if (theEvent->xmotion.window != my_main_wind) {
				WriteExtraErr("Got MotionNotify for some other window");
			} else {
				MousePositionNotify(
					theEvent->xmotion.x, theEvent->xmotion.y);
			}
			break;
		case EnterNotify:
			if (theEvent->xcrossing.window != my_main_wind) {
				WriteExtraErr("Got EnterNotify for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - EnterNotify");
#endif

				CaughtMouse = true;
				MousePositionNotify(
					theEvent->xcrossing.x, theEvent->xcrossing.y);
			}
			break;
		case LeaveNotify:
			if (theEvent->xcrossing.window != my_main_wind) {
				WriteExtraErr("Got LeaveNotify for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - LeaveNotify");
#endif

				MousePositionNotify(
					theEvent->xcrossing.x, theEvent->xcrossing.y);
				CaughtMouse = false;
			}
			break;
#endif
		case Expose:
			if (theEvent->xexpose.window != my_main_wind) {
				WriteExtraErr(
					"Got SelectionRequest for some other window");
			} else {
				int x0 = theEvent->xexpose.x;
				int y0 = theEvent->xexpose.y;
				int x1 = x0 + theEvent->xexpose.width;
				int y1 = y0 + theEvent->xexpose.height;

#if 0 && MyDbgEvents
				dbglog_writeln("- event - Expose");
#endif

#if VarFullScreen
				if (UseFullScreen)
#endif
#if MayFullScreen
				{
					x0 -= hOffset;
					y0 -= vOffset;
					x1 -= hOffset;
					y1 -= vOffset;
				}
#endif

#if EnableMagnify
				if (UseMagnify) {
					x0 /= MyWindowScale;
					y0 /= MyWindowScale;
					x1 = (x1 + (MyWindowScale - 1)) / MyWindowScale;
					y1 = (y1 + (MyWindowScale - 1)) / MyWindowScale;
				}
#endif

#if VarFullScreen
				if (UseFullScreen)
#endif
#if MayFullScreen
				{
					x0 += ViewHStart;
					y0 += ViewVStart;
					x1 += ViewHStart;
					y1 += ViewVStart;
				}
#endif

				if (x0 < 0) {
					x0 = 0;
				}
				if (x1 > vMacScreenWidth) {
					x1 = vMacScreenWidth;
				}
				if (y0 < 0) {
					y0 = 0;
				}
				if (y1 > vMacScreenHeight) {
					y1 = vMacScreenHeight;
				}
				if ((x0 < x1) && (y0 < y1)) {
					HaveChangedScreenBuff(y0, x0, y1, x1);
				}

				NeedFinishOpen1 = false;
			}
			break;
#if IncludeHostTextClipExchange
		case SelectionRequest:
			if (theEvent->xselectionrequest.owner != my_main_wind) {
				WriteExtraErr(
					"Got SelectionRequest for some other window");
			} else {
				XEvent xevent;
				bool RequestFilled = false;

#if MyDbgEvents
				dbglog_writeln("- event - SelectionRequest");
				WriteDbgAtom("selection",
					theEvent->xselectionrequest.selection);
				WriteDbgAtom("target",
					theEvent->xselectionrequest.target);
				WriteDbgAtom("property",
					theEvent->xselectionrequest.property);
#endif

				if (theEvent->xselectionrequest.selection ==
					MyXA_CLIPBOARD)
				{
					RequestFilled =
						HandleSelectionRequestClipboard(theEvent);
				}


				memset(&xevent, 0, sizeof(xevent));
				xevent.xselection.type = SelectionNotify;
				xevent.xselection.display = x_display;
				xevent.xselection.requestor =
					theEvent->xselectionrequest.requestor;
				xevent.xselection.selection =
					theEvent->xselectionrequest.selection;
				xevent.xselection.target =
					theEvent->xselectionrequest.target;
				xevent.xselection.property = (! RequestFilled) ? None
					: theEvent->xselectionrequest.property ;
				xevent.xselection.time =
					theEvent->xselectionrequest.time;

				if (0 == XSendEvent(x_display,
					xevent.xselection.requestor, False, 0, &xevent))
				{
					WriteExtraErr(
						"XSendEvent failed in SelectionRequest");
				}
			}
			break;
		case SelectionClear:
			if (theEvent->xselectionclear.window != my_main_wind) {
				WriteExtraErr(
					"Got SelectionClear for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - SelectionClear");
				WriteDbgAtom("selection",
					theEvent->xselectionclear.selection);
#endif

				if (theEvent->xselectionclear.selection ==
					MyXA_CLIPBOARD)
				{
					FreeMyClipBuffer();
				}
			}
			break;
#endif
#if EnableDragDrop
		case SelectionNotify:
			if (theEvent->xselection.requestor != my_main_wind) {
				WriteExtraErr(
					"Got SelectionNotify for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - SelectionNotify");
				WriteDbgAtom("selection",
					theEvent->xselection.selection);
				WriteDbgAtom("target", theEvent->xselection.target);
				WriteDbgAtom("property", theEvent->xselection.property);
#endif

				if (theEvent->xselection.selection == MyXA_DndSelection)
				{
					HandleSelectionNotifyDnd(theEvent);
				} else {
					WriteExtraErr(
						"Got Unknown selection in SelectionNotify");
				}
			}
			break;
#endif
		case ClientMessage:
			if (theEvent->xclient.window != my_main_wind) {
				WriteExtraErr(
					"Got ClientMessage for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - ClientMessage");
				WriteDbgAtom("message_type",
					theEvent->xclient.message_type);
#endif

#if EnableDragDrop
				if (theEvent->xclient.message_type == MyXA_DndEnter) {
					/* printf("Got XdndEnter\n"); */
				} else if (theEvent->xclient.message_type ==
					MyXA_DndLeave)
				{
					/* printf("Got XdndLeave\n"); */
				} else if (theEvent->xclient.message_type ==
					MyXA_DndPosition)
				{
					HandleClientMessageDndPosition(theEvent);
				} else if (theEvent->xclient.message_type ==
					MyXA_DndDrop)
				{
					HandleClientMessageDndDrop(theEvent);
				} else
#endif
				{
					if ((32 == theEvent->xclient.format) &&
						(theEvent->xclient.data.l[0] == MyXA_DeleteW))
					{
						/*
							I would think that should test that
								WM_PROTOCOLS == message_type
							but none of the other programs I looked
							at did.
						*/
						RequestMacOff = true;
					}
				}
			}
			break;
		case FocusIn:
			if (theEvent->xfocus.window != my_main_wind) {
				WriteExtraErr("Got FocusIn for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - FocusIn");
#endif

				gTrueBackgroundFlag = false;
#if UseMotionEvents
				CheckMouseState();
					/*
						Doesn't help on x11 for OS X,
						can't get new mouse position
						in any fashion until mouse moves.
					*/
#endif
			}
			break;
		case FocusOut:
			if (theEvent->xfocus.window != my_main_wind) {
				WriteExtraErr("Got FocusOut for some other window");
			} else {
#if MyDbgEvents
				dbglog_writeln("- event - FocusOut");
#endif

				gTrueBackgroundFlag = true;
			}
			break;
		default:
			break;
	}
}

/* --- main window creation and disposal --- */

static int my_argc;
static char **my_argv;

static char *display_name = NULL;

static bool Screen_Init(void)
{
	Window rootwin;
	int screen;
	Colormap Xcmap;
	Visual *Xvisual;

	x_display = XOpenDisplay(display_name);
	if (NULL == x_display) {
		fprintf(stderr, "Cannot connect to X server.\n");
		return false;
	}

	screen = DefaultScreen(x_display);

	rootwin = XRootWindow(x_display, screen);

	Xcmap = DefaultColormap(x_display, screen);

	Xvisual = DefaultVisual(x_display, screen);

	LoadMyXA();

	XParseColor(x_display, Xcmap, "#000000", &x_black);
	if (! XAllocColor(x_display, Xcmap, &x_black)) {
		WriteExtraErr("XParseColor black fails");
	}
	XParseColor(x_display, Xcmap, "#ffffff", &x_white);
	if (! XAllocColor(x_display, Xcmap, &x_white)) {
		WriteExtraErr("XParseColor white fails");
	}

	if (! CreateMyBlankCursor(rootwin)) {
		return false;
	}

#if ! UseColorImage
	my_image = XCreateImage(x_display, Xvisual, 1, XYBitmap, 0,
		NULL /* (char *)image_Mem1 */,
		vMacScreenWidth, vMacScreenHeight, 32,
		vMacScreenMonoByteWidth);
	if (NULL == my_image) {
		fprintf(stderr, "XCreateImage failed.\n");
		return false;
	}

#if 0
	fprintf(stderr, "bitmap_bit_order = %d\n",
		(int)my_image->bitmap_bit_order);
	fprintf(stderr, "byte_order = %d\n", (int)my_image->byte_order);
#endif

	my_image->bitmap_bit_order = MSBFirst;
	my_image->byte_order = MSBFirst;
#endif

#if UseColorImage
	my_image = XCreateImage(x_display, Xvisual, 24, ZPixmap, 0,
		NULL /* (char *)image_Mem1 */,
		vMacScreenWidth, vMacScreenHeight, 32,
			4 * (uint32_t)vMacScreenWidth);
	if (NULL == my_image) {
		fprintf(stderr, "XCreateImage Color failed.\n");
		return false;
	}

#if 0
	fprintf(stderr, "DefaultDepth = %d\n",
		(int)DefaultDepth(x_display, screen));

	fprintf(stderr, "MSBFirst = %d\n", (int)MSBFirst);
	fprintf(stderr, "LSBFirst = %d\n", (int)LSBFirst);

	fprintf(stderr, "bitmap_bit_order = %d\n",
		(int)my_image->bitmap_bit_order);
	fprintf(stderr, "byte_order = %d\n",
		(int)my_image->byte_order);
	fprintf(stderr, "bitmap_unit = %d\n",
		(int)my_image->bitmap_unit);
	fprintf(stderr, "bits_per_pixel = %d\n",
		(int)my_image->bits_per_pixel);
	fprintf(stderr, "red_mask = %d\n",
		(int)my_image->red_mask);
	fprintf(stderr, "green_mask = %d\n",
		(int)my_image->green_mask);
	fprintf(stderr, "blue_mask = %d\n",
		(int)my_image->blue_mask);
#endif

#endif /* UseColorImage */

#if EnableMagnify && (! UseColorImage)
	my_Scaled_image = XCreateImage(x_display, Xvisual,
		1, XYBitmap, 0,
		NULL /* (char *)image_Mem1 */,
		vMacScreenWidth * MyWindowScale,
		vMacScreenHeight * MyWindowScale,
		32, vMacScreenMonoByteWidth * MyWindowScale);
	if (NULL == my_Scaled_image) {
		fprintf(stderr, "XCreateImage failed.\n");
		return false;
	}

	my_Scaled_image->bitmap_bit_order = MSBFirst;
	my_Scaled_image->byte_order = MSBFirst;
#endif

#if EnableMagnify && UseColorImage
	my_Scaled_image = XCreateImage(x_display, Xvisual,
		24, ZPixmap, 0,
		NULL /* (char *)image_Mem1 */,
		vMacScreenWidth * MyWindowScale,
		vMacScreenHeight * MyWindowScale,
		32, 4 * (uint32_t)vMacScreenWidth * MyWindowScale);
	if (NULL == my_Scaled_image) {
		fprintf(stderr, "XCreateImage Scaled failed.\n");
		return false;
	}
#endif

#if 0 != vMacScreenDepth
	ColorModeWorks = true;
#endif

	DisableKeyRepeat();

	return true;
}

static void CloseMainWindow(void)
{
	if (my_gc != NULL) {
		XFreeGC(x_display, my_gc);
		my_gc = NULL;
	}
	if (my_main_wind) {
		XDestroyWindow(x_display, my_main_wind);
		my_main_wind = 0;
	}
}

enum {
	kMagStateNormal,
#if EnableMagnify
	kMagStateMagnifgy,
#endif
	kNumMagStates
};

#define kMagStateAuto kNumMagStates

#if MayNotFullScreen
static int CurWinIndx;
static bool HavePositionWins[kNumMagStates];
static int WinPositionWinsH[kNumMagStates];
static int WinPositionWinsV[kNumMagStates];
#endif

#if EnableRecreateW
static void ZapMyWState(void)
{
	my_main_wind = 0;
	my_gc = NULL;
}
#endif

static bool CreateMainWindow(void)
{
	Window rootwin;
	int screen;
	int xr;
	int yr;
	unsigned int dr;
	unsigned int wr;
	unsigned int hr;
	unsigned int bwr;
	Window rr;
	int leftPos;
	int topPos;
#if MayNotFullScreen
	int WinIndx;
#endif
#if EnableDragDrop
	long int xdnd_version = 5;
#endif
	int NewWindowHeight = vMacScreenHeight;
	int NewWindowWidth = vMacScreenWidth;

	/* Get connection to X Server */
	screen = DefaultScreen(x_display);

	rootwin = XRootWindow(x_display, screen);

	XGetGeometry(x_display, rootwin,
		&rr, &xr, &yr, &wr, &hr, &bwr, &dr);

#if EnableMagnify
	if (UseMagnify) {
		NewWindowHeight *= MyWindowScale;
		NewWindowWidth *= MyWindowScale;
	}
#endif

	if (wr > NewWindowWidth) {
		leftPos = (wr - NewWindowWidth) / 2;
	} else {
		leftPos = 0;
	}
	if (hr > NewWindowHeight) {
		topPos = (hr - NewWindowHeight) / 2;
	} else {
		topPos = 0;
	}

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		ViewHSize = wr;
		ViewVSize = hr;
#if EnableMagnify
		if (UseMagnify) {
			ViewHSize /= MyWindowScale;
			ViewVSize /= MyWindowScale;
		}
#endif
		if (ViewHSize >= vMacScreenWidth) {
			ViewHStart = 0;
			ViewHSize = vMacScreenWidth;
		} else {
			ViewHSize &= ~ 1;
		}
		if (ViewVSize >= vMacScreenHeight) {
			ViewVStart = 0;
			ViewVSize = vMacScreenHeight;
		} else {
			ViewVSize &= ~ 1;
		}
	}
#endif

#if VarFullScreen
	if (! UseFullScreen)
#endif
#if MayNotFullScreen
	{
#if EnableMagnify
		if (UseMagnify) {
			WinIndx = kMagStateMagnifgy;
		} else
#endif
		{
			WinIndx = kMagStateNormal;
		}

		if (! HavePositionWins[WinIndx]) {
			WinPositionWinsH[WinIndx] = leftPos;
			WinPositionWinsV[WinIndx] = topPos;
			HavePositionWins[WinIndx] = true;
		} else {
			leftPos = WinPositionWinsH[WinIndx];
			topPos = WinPositionWinsV[WinIndx];
		}
	}
#endif

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		XSetWindowAttributes xattr;
		xattr.override_redirect = True;
		xattr.background_pixel = x_black.pixel;
		xattr.border_pixel = x_white.pixel;

		my_main_wind = XCreateWindow(x_display, rr,
			0, 0, wr, hr, 0,
			CopyFromParent, /* depth */
			InputOutput, /* class */
			CopyFromParent, /* visual */
			CWOverrideRedirect | CWBackPixel | CWBorderPixel,
				/* valuemask */
			&xattr /* attributes */);
	}
#endif
#if VarFullScreen
	else
#endif
#if MayNotFullScreen
	{
		my_main_wind = XCreateSimpleWindow(x_display, rootwin,
			leftPos,
			topPos,
			NewWindowWidth, NewWindowHeight, 4,
			x_white.pixel,
			x_black.pixel);
	}
#endif

	if (! my_main_wind) {
		WriteExtraErr("XCreateSimpleWindow failed.");
		return false;
	} else {
		char *win_name =
			(NULL != n_arg) ? n_arg : (
#if CanGetAppPath
			(NULL != app_name) ? app_name :
#endif
			kStrAppName);
		XSelectInput(x_display, my_main_wind,
			ExposureMask | KeyPressMask | KeyReleaseMask
			| ButtonPressMask | ButtonReleaseMask
#if UseMotionEvents
			| PointerMotionMask | EnterWindowMask | LeaveWindowMask
#endif
			| FocusChangeMask);

		XStoreName(x_display, my_main_wind, win_name);
		XSetIconName(x_display, my_main_wind, win_name);

		{
			XClassHint *hints = XAllocClassHint();
			if (hints) {
				hints->res_name = "minivmac";
				hints->res_class = "minivmac";
				XSetClassHint(x_display, my_main_wind, hints);
				XFree(hints);
			}
		}

		{
			XWMHints *hints = XAllocWMHints();
			if (hints) {
				hints->input = True;
				hints->initial_state = NormalState;
				hints->flags = InputHint | StateHint;
				XSetWMHints(x_display, my_main_wind, hints);
				XFree(hints);
			}

		}

		XSetCommand(x_display, my_main_wind, my_argv, my_argc);

		/* let us handle a click on the close box */
		XSetWMProtocols(x_display, my_main_wind, &MyXA_DeleteW, 1);

#if EnableDragDrop
		XChangeProperty (x_display, my_main_wind, MyXA_DndAware,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *) &xdnd_version, 1);
#endif

		my_gc = XCreateGC(x_display, my_main_wind, 0, NULL);
		if (NULL == my_gc) {
			WriteExtraErr("XCreateGC failed.");
			return false;
		}
		XSetState(x_display, my_gc, x_black.pixel, x_white.pixel,
			GXcopy, AllPlanes);

#if VarFullScreen
		if (! UseFullScreen)
#endif
#if MayNotFullScreen
		{
			XSizeHints *hints = XAllocSizeHints();
			if (hints) {
				hints->min_width = NewWindowWidth;
				hints->max_width = NewWindowWidth;
				hints->min_height = NewWindowHeight;
				hints->max_height = NewWindowHeight;

				/*
					Try again to say where the window ought to go.
					I've seen this described as obsolete, but it
					seems to work on all x implementations tried
					so far, and nothing else does.
				*/
				hints->x = leftPos;
				hints->y = topPos;
				hints->width = NewWindowWidth;
				hints->height = NewWindowHeight;

				hints->flags = PMinSize | PMaxSize | PPosition | PSize;
				XSetWMNormalHints(x_display, my_main_wind, hints);
				XFree(hints);
			}
		}
#endif

#if VarFullScreen
		if (UseFullScreen)
#endif
#if MayFullScreen
		{
			hOffset = leftPos;
			vOffset = topPos;
		}
#endif

		DisconnectKeyCodes3();
			/* since will lose keystrokes to old window */

#if MayNotFullScreen
		CurWinIndx = WinIndx;
#endif

		XMapRaised(x_display, my_main_wind);

#if 0
		XSync(x_display, 0);
#endif

#if 0
		/*
			This helps in Red Hat 9 to get the new window
			activated, and I've seen other programs
			do similar things.
		*/
		/*
			In current scheme, haven't closed old window
			yet. If old window full screen, never receive
			expose event for new one.
		*/
		{
			XEvent event;

			do {
				XNextEvent(x_display, &event);
				HandleTheEvent(&event);
			} while (! ((Expose == event.type)
				&& (event.xexpose.window == my_main_wind)));
		}
#endif

		NeedFinishOpen1 = true;
		NeedFinishOpen2 = true;

		return true;
	}
}

#if MayFullScreen
static bool GrabMachine = false;
#endif

#if MayFullScreen
static void GrabTheMachine(void)
{
#if EnableFSMouseMotion
	StartSaveMouseMotion();
#endif
#if GrabKeysFullScreen
	MyGrabKeyboard();
#endif
}
#endif

#if MayFullScreen
static void UngrabMachine(void)
{
#if EnableFSMouseMotion
	StopSaveMouseMotion();
#endif
#if GrabKeysFullScreen
	MyUnGrabKeyboard();
#endif
}
#endif

#if EnableRecreateW
struct MyWState {
	Window f_my_main_wind;
	GC f_my_gc;
#if MayFullScreen
	short f_hOffset;
	short f_vOffset;
	uint16_t f_ViewHSize;
	uint16_t f_ViewVSize;
	uint16_t f_ViewHStart;
	uint16_t f_ViewVStart;
#endif
#if VarFullScreen
	bool f_UseFullScreen;
#endif
#if EnableMagnify
	bool f_UseMagnify;
#endif
};
typedef struct MyWState MyWState;
#endif

#if EnableRecreateW
static void GetMyWState(MyWState *r)
{
	r->f_my_main_wind = my_main_wind;
	r->f_my_gc = my_gc;
#if MayFullScreen
	r->f_hOffset = hOffset;
	r->f_vOffset = vOffset;
	r->f_ViewHSize = ViewHSize;
	r->f_ViewVSize = ViewVSize;
	r->f_ViewHStart = ViewHStart;
	r->f_ViewVStart = ViewVStart;
#endif
#if VarFullScreen
	r->f_UseFullScreen = UseFullScreen;
#endif
#if EnableMagnify
	r->f_UseMagnify = UseMagnify;
#endif
}
#endif

#if EnableRecreateW
static void SetMyWState(MyWState *r)
{
	my_main_wind = r->f_my_main_wind;
	my_gc = r->f_my_gc;
#if MayFullScreen
	hOffset = r->f_hOffset;
	vOffset = r->f_vOffset;
	ViewHSize = r->f_ViewHSize;
	ViewVSize = r->f_ViewVSize;
	ViewHStart = r->f_ViewHStart;
	ViewVStart = r->f_ViewVStart;
#endif
#if VarFullScreen
	UseFullScreen = r->f_UseFullScreen;
#endif
#if EnableMagnify
	UseMagnify = r->f_UseMagnify;
#endif
}
#endif

#if EnableRecreateW
static bool WantRestoreCursPos = false;
static uint16_t RestoreMouseH;
static uint16_t RestoreMouseV;
#endif

#if EnableRecreateW
static bool ReCreateMainWindow(void)
{
	MyWState old_state;
	MyWState new_state;
#if IncludeHostTextClipExchange
	bool OwnClipboard = false;
#endif

	if (HaveCursorHidden) {
		WantRestoreCursPos = true;
		RestoreMouseH = CurMouseH;
		RestoreMouseV = CurMouseV;
	}

	ForceShowCursor(); /* hide/show cursor api is per window */

#if MayNotFullScreen
#if VarFullScreen
	if (! UseFullScreen)
#endif
	if (my_main_wind)
	if (! NeedFinishOpen2)
	{
		/* save old position */
		int xr;
		int yr;
		unsigned int dr;
		unsigned int wr;
		unsigned int hr;
		unsigned int bwr;
		Window rr;
		Window rr2;

		/* Get connection to X Server */
		int screen = DefaultScreen(x_display);

		Window rootwin = XRootWindow(x_display, screen);

		XGetGeometry(x_display, rootwin,
			&rr, &xr, &yr, &wr, &hr, &bwr, &dr);

		/*
			Couldn't reliably find out where window
			is now, due to what seem to be some
			broken X implementations, and so instead
			track how far window has moved.
		*/
		XSync(x_display, 0);
		if (XTranslateCoordinates(x_display, my_main_wind, rootwin,
			0, 0, &xr, &yr, &rr2))
		{
			int newposh =
				WinPositionWinsH[CurWinIndx] + (xr - SavedTransH);
			int newposv =
				WinPositionWinsV[CurWinIndx] + (yr - SavedTransV);
			if ((newposv > 0) && (newposv < hr) && (newposh < wr)) {
				WinPositionWinsH[CurWinIndx] = newposh;
				WinPositionWinsV[CurWinIndx] = newposv;
				SavedTransH = xr;
				SavedTransV = yr;
			}
		}
	}
#endif

#if MayFullScreen
	if (GrabMachine) {
		GrabMachine = false;
		UngrabMachine();
	}
#endif

	GetMyWState(&old_state);
	ZapMyWState();

#if EnableMagnify
	UseMagnify = WantMagnify;
#endif
#if VarFullScreen
	UseFullScreen = WantFullScreen;
#endif

	ColorTransValid = false;

	if (! CreateMainWindow()) {
		CloseMainWindow();
		SetMyWState(&old_state);

		/* avoid retry */
#if VarFullScreen
		WantFullScreen = UseFullScreen;
#endif
#if EnableMagnify
		WantMagnify = UseMagnify;
#endif

		return false;
	} else {
		GetMyWState(&new_state);
		SetMyWState(&old_state);

#if IncludeHostTextClipExchange
		if (my_main_wind) {
			if (XGetSelectionOwner(x_display, MyXA_CLIPBOARD) ==
				my_main_wind)
			{
				OwnClipboard = true;
			}
		}
#endif

		CloseMainWindow();

		SetMyWState(&new_state);

#if IncludeHostTextClipExchange
		if (OwnClipboard) {
			XSetSelectionOwner(x_display, MyXA_CLIPBOARD,
				my_main_wind, CurrentTime);
		}
#endif
	}

	return true;
}
#endif

#if VarFullScreen && EnableMagnify
enum {
	kWinStateWindowed,
#if EnableMagnify
	kWinStateFullScreen,
#endif
	kNumWinStates
};
#endif

#if VarFullScreen && EnableMagnify
static int WinMagStates[kNumWinStates];
#endif

static void ZapWinStateVars(void)
{
#if MayNotFullScreen
	{
		int i;

		for (i = 0; i < kNumMagStates; ++i) {
			HavePositionWins[i] = false;
		}
	}
#endif
#if VarFullScreen && EnableMagnify
	{
		int i;

		for (i = 0; i < kNumWinStates; ++i) {
			WinMagStates[i] = kMagStateAuto;
		}
	}
#endif
}

#if VarFullScreen
static void ToggleWantFullScreen(void)
{
	WantFullScreen = ! WantFullScreen;

#if EnableMagnify
	{
		int OldWinState =
			UseFullScreen ? kWinStateFullScreen : kWinStateWindowed;
		int OldMagState =
			UseMagnify ? kMagStateMagnifgy : kMagStateNormal;
		int NewWinState =
			WantFullScreen ? kWinStateFullScreen : kWinStateWindowed;
		int NewMagState = WinMagStates[NewWinState];

		WinMagStates[OldWinState] = OldMagState;
		if (kMagStateAuto != NewMagState) {
			WantMagnify = (kMagStateMagnifgy == NewMagState);
		} else {
			WantMagnify = false;
			if (WantFullScreen) {
				Window rootwin;
				int xr;
				int yr;
				unsigned int dr;
				unsigned int wr;
				unsigned int hr;
				unsigned int bwr;
				Window rr;

				rootwin =
					XRootWindow(x_display, DefaultScreen(x_display));
				XGetGeometry(x_display, rootwin,
					&rr, &xr, &yr, &wr, &hr, &bwr, &dr);
				if ((wr >= vMacScreenWidth * MyWindowScale)
					&& (hr >= vMacScreenHeight * MyWindowScale)
					)
				{
					WantMagnify = true;
				}
			}
		}
	}
#endif
}
#endif

/* --- SavedTasks --- */

static void LeaveBackground(void)
{
	ReconnectKeyCodes3();
	DisableKeyRepeat();
}

static void EnterBackground(void)
{
	RestoreKeyRepeat();
	DisconnectKeyCodes3();

	ForceShowCursor();
}

static void LeaveSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Start();
#endif

	StartUpTimeAdjust();
}

static void EnterSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Stop();
#endif
}

static void CheckForSavedTasks(void)
{
	if (MyEvtQNeedRecover) {
		MyEvtQNeedRecover = false;

		/* attempt cleanup, MyEvtQNeedRecover may get set again */
		MyEvtQTryRecoverFromFull();
	}

	if (NeedFinishOpen2 && ! NeedFinishOpen1) {
		NeedFinishOpen2 = false;

#if VarFullScreen
		if (UseFullScreen)
#endif
#if MayFullScreen
		{
			XSetInputFocus(x_display, my_main_wind,
				RevertToPointerRoot, CurrentTime);
		}
#endif
#if VarFullScreen
		else
#endif
#if MayNotFullScreen
		{
			Window rr;
			int screen = DefaultScreen(x_display);
			Window rootwin = XRootWindow(x_display, screen);
#if 0
			/*
				This doesn't work right in Red Hat 6, and may not
				be needed anymore, now that using PPosition hint.
			*/
			XMoveWindow(x_display, my_main_wind,
				leftPos, topPos);
				/*
					Needed after XMapRaised, because some window
					managers will apparently ignore where the
					window was asked to be put.
				*/
#endif

			XSync(x_display, 0);
				/*
					apparently, XTranslateCoordinates can be inaccurate
					without this
				*/
			XTranslateCoordinates(x_display, my_main_wind, rootwin,
				0, 0, &SavedTransH, &SavedTransV, &rr);
		}
#endif

#if EnableRecreateW
		if (WantRestoreCursPos) {
#if EnableFSMouseMotion
			if (! HaveMouseMotion)
#endif
			{
				(void) MyMoveMouse(RestoreMouseH, RestoreMouseV);
				WantCursorHidden = true;
			}
			WantRestoreCursPos = false;
		}
#endif
	}

#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		MyMouseConstrain();
	}
#endif

	if (RequestMacOff) {
		RequestMacOff = false;
		if (AnyDiskInserted()) {
			MacMsgOverride(kStrQuitWarningTitle,
				kStrQuitWarningMessage);
		} else {
			ForceMacOff = true;
		}
	}

	if (ForceMacOff) {
		return;
	}

	if (gTrueBackgroundFlag != gBackgroundFlag) {
		gBackgroundFlag = gTrueBackgroundFlag;
		if (gTrueBackgroundFlag) {
			EnterBackground();
		} else {
			LeaveBackground();
		}
	}

	if (CurSpeedStopped != (SpeedStopped ||
		(gBackgroundFlag && ! RunInBackground
#if EnableAutoSlow && 0
			&& (QuietSubTicks >= 4092)
#endif
		)))
	{
		CurSpeedStopped = ! CurSpeedStopped;
		if (CurSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

#if MayFullScreen
	if (gTrueBackgroundFlag
#if VarFullScreen
		&& WantFullScreen
#endif
		)
	{
		/*
			Since often get here on Ubuntu Linux 5.10
			running on a slow machine (emulated) when
			attempt to enter full screen, don't abort
			full screen, but try to fix it.
		*/
#if 0
		ToggleWantFullScreen();
#else
		XRaiseWindow(x_display, my_main_wind);
		XSetInputFocus(x_display, my_main_wind,
			RevertToPointerRoot, CurrentTime);
#endif
	}
#endif

#if EnableRecreateW
	if (0
#if EnableMagnify
		|| (UseMagnify != WantMagnify)
#endif
#if VarFullScreen
		|| (UseFullScreen != WantFullScreen)
#endif
		)
	{
		(void) ReCreateMainWindow();
	}
#endif


#if MayFullScreen
	if (GrabMachine != (
#if VarFullScreen
		UseFullScreen &&
#endif
		! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		GrabMachine = ! GrabMachine;
		if (GrabMachine) {
			GrabTheMachine();
		} else {
			UngrabMachine();
		}
	}
#endif

#if IncludeSonyNew
	if (vSonyNewDiskWanted) {
#if IncludeSonyNameNew
		if (vSonyNewDiskName != NotAPbuf) {
			uint8_t * NewDiskNameDat;
			if (MacRomanTextToNativePtr(vSonyNewDiskName, true,
				&NewDiskNameDat))
			{
				MakeNewDisk(vSonyNewDiskSize, (char *)NewDiskNameDat);
				free(NewDiskNameDat);
			}
			PbufDispose(vSonyNewDiskName);
			vSonyNewDiskName = NotAPbuf;
		} else
#endif
		{
			MakeNewDiskAtDefault(vSonyNewDiskSize);
		}
		vSonyNewDiskWanted = false;
			/* must be done after may have gotten disk */
	}
#endif

	if ((nullptr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

	if (NeedWholeScreenDraw) {
		NeedWholeScreenDraw = false;
		ScreenChangedAll();
	}

#if NeedRequestIthDisk
	if (0 != RequestIthDisk) {
		Sony_InsertIth(RequestIthDisk);
		RequestIthDisk = 0;
	}
#endif

	if (HaveCursorHidden != (WantCursorHidden
		&& ! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		HaveCursorHidden = ! HaveCursorHidden;
		if (HaveCursorHidden) {
			XDefineCursor(x_display, my_main_wind, blankCursor);
		} else {
			XUndefineCursor(x_display, my_main_wind);
		}
	}
}

/* --- command line parsing --- */

static bool ScanCommandLine(void)
{
	char *pa;
	int i = 1;

label_retry:
	if (i < my_argc) {
		pa = my_argv[i++];
		if ('-' == pa[0]) {
			if ((0 == strcmp(pa, "--display"))
				|| (0 == strcmp(pa, "-display")))
			{
				if (i < my_argc) {
					display_name = my_argv[i++];
					goto label_retry;
				}
			} else
			if ((0 == strcmp(pa, "--rom"))
				|| (0 == strcmp(pa, "-r")))
			{
				if (i < my_argc) {
					rom_path = my_argv[i++];
					goto label_retry;
				}
			} else
			if (0 == strcmp(pa, "-n"))
			{
				if (i < my_argc) {
					n_arg = my_argv[i++];
					goto label_retry;
				}
			} else
			if (0 == strcmp(pa, "-d"))
			{
				if (i < my_argc) {
					d_arg = my_argv[i++];
					goto label_retry;
				}
			} else
#ifndef UsingAlsa
#define UsingAlsa 0
#endif

#if UsingAlsa
			if ((0 == strcmp(pa, "--alsadev"))
				|| (0 == strcmp(pa, "-alsadev")))
			{
				if (i < my_argc) {
					alsadev_name = my_argv[i++];
					goto label_retry;
				}
			} else
#endif
#if 0
			if (0 == strcmp(pa, "-l")) {
				SpeedValue = 0;
				goto label_retry;
			} else
#endif
			{
				MacMsg(kStrBadArgTitle, kStrBadArgMessage, false);
			}
		} else {
			(void) Sony_Insert1(pa, false);
			goto label_retry;
		}
	}

	return true;
}

/* --- main program flow --- */

void DoneWithDrawingForTick(void)
{
#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		AutoScrollScreen();
	}
#endif
	MyDrawChangesAndClear();
	XFlush(x_display);
}

 bool ExtraTimeNotOver(void)
{
	UpdateTrueEmulatedTime();
	return TrueEmulatedTime == OnTrueTime;
}

static void WaitForTheNextEvent(void)
{
	XEvent event;

	XNextEvent(x_display, &event);
	HandleTheEvent(&event);
}

static void CheckForSystemEvents(void)
{
	int i = 10;

	while ((XEventsQueued(x_display, QueuedAfterReading) > 0)
		&& (--i >= 0))
	{
		WaitForTheNextEvent();
	}
}

void WaitForNextTick(void)
{
label_retry:
	CheckForSystemEvents();
	CheckForSavedTasks();
	if (ForceMacOff) {
		return;
	}

	if (CurSpeedStopped) {
		DoneWithDrawingForTick();
		WaitForTheNextEvent();
		goto label_retry;
	}

	if (ExtraTimeNotOver()) {
		struct timespec rqt;
		struct timespec rmt;

		int32_t TimeDiff = GetTimeDiff();
		if (TimeDiff < 0) {
			rqt.tv_sec = 0;
			rqt.tv_nsec = (- TimeDiff) * 1000;
			(void) nanosleep(&rqt, &rmt);
		}
		goto label_retry;
	}

	if (CheckDateTime()) {
#if MySoundEnabled
		MySound_SecondNotify();
#endif
#if EnableDemoMsg
		DemoModeSecondNotify();
#endif
	}

	if ((! gBackgroundFlag)
#if UseMotionEvents
		&& (! CaughtMouse)
#endif
		)
	{
		CheckMouseState();
	}

	OnTrueTime = TrueEmulatedTime;

#if dbglog_TimeStuff
	dbglog_writelnNum("WaitForNextTick, OnTrueTime", OnTrueTime);
#endif
}

/* --- platform independent code can be thought of as going here --- */

#include "core/main.h"

static void ZapOSGLUVars(void)
{
	InitDrives();
	ZapWinStateVars();
}

static void ReserveAllocAll(void)
{
#if dbglog_HAVE
	dbglog_ReserveAlloc();
#endif
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, false);

	ReserveAllocOneBlock(&screencomparebuff,
		vMacScreenNumBytes, 5, true);
#if UseControlKeys
	ReserveAllocOneBlock(&CntrlDisplayBuff,
		vMacScreenNumBytes, 5, false);
#endif
#if WantScalingBuff
	ReserveAllocOneBlock(&ScalingBuff,
		ScalingBuffsz, 5, false);
#endif
#if WantScalingTabl
	ReserveAllocOneBlock(&ScalingTabl,
		ScalingTablsz, 5, false);
#endif

#if MySoundEnabled
	ReserveAllocOneBlock((uint8_t * *)&TheSoundBuffer,
		dbhBufferSize, 5, false);
#endif

	EmulationReserveAlloc();
}

static bool AllocMyMemory(void)
{
	uint32_t n;
	bool IsOk = false;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullptr;
	ReserveAllocAll();
	n = ReserveAllocOffset;
	ReserveAllocBigBlock = (uint8_t *)calloc(1, n);
	if (NULL == ReserveAllocBigBlock) {
		MacMsg(kStrOutOfMemTitle, kStrOutOfMemMessage, true);
	} else {
		ReserveAllocOffset = 0;
		ReserveAllocAll();
		if (n != ReserveAllocOffset) {
			/* oops, program error */
		} else {
			IsOk = true;
		}
	}

	return IsOk;
}

static void UnallocMyMemory(void)
{
	if (nullptr != ReserveAllocBigBlock) {
		free((char *)ReserveAllocBigBlock);
	}
}

#if HaveAppPathLink
static bool ReadLink_Alloc(char *path, char **r)
{
	/*
		This should work to find size:

		struct stat r;

		if (lstat(path, &r) != -1) {
			r = r.st_size;
			IsOk = true;
		}

		But observed to return 0 in Ubuntu 10.04 x86-64
	*/

	char *s;
	int sz;
	char *p;
	bool IsOk = false;
	size_t s_alloc = 256;

label_retry:
	s = (char *)malloc(s_alloc);
	if (NULL == s) {
		fprintf(stderr, "malloc failed.\n");
	} else {
		sz = readlink(path, s, s_alloc);
		if ((sz < 0) || (sz >= s_alloc)) {
			free(s);
			if (sz == s_alloc) {
				s_alloc <<= 1;
				goto label_retry;
			} else {
				fprintf(stderr, "readlink failed.\n");
			}
		} else {
			/* ok */
			p = (char *)malloc(sz + 1);
			if (NULL == p) {
				fprintf(stderr, "malloc failed.\n");
			} else {
				(void) memcpy(p, s, sz);
				p[sz] = 0;
				*r = p;
				IsOk = true;
			}
			free(s);
		}
	}

	return IsOk;
}
#endif

#if HaveSysctlPath
static bool ReadKernProcPathname(char **r)
{
	size_t s_alloc;
	char *s;
	int mib[] = {
		CTL_KERN,
		KERN_PROC,
		KERN_PROC_PATHNAME,
		-1
	};
	bool IsOk = false;

	if (0 != sysctl(mib, sizeof(mib) / sizeof(int),
		NULL, &s_alloc, NULL, 0))
	{
		fprintf(stderr, "sysctl failed.\n");
	} else {
		s = (char *)malloc(s_alloc);
		if (NULL == s) {
			fprintf(stderr, "malloc failed.\n");
		} else {
			if (0 != sysctl(mib, sizeof(mib) / sizeof(int),
				s, &s_alloc, NULL, 0))
			{
				fprintf(stderr, "sysctl 2 failed.\n");
			} else {
				*r = s;
				IsOk = true;
			}
			if (! IsOk) {
				free(s);
			}
		}
	}

	return IsOk;
}
#endif

#if CanGetAppPath
static bool Path2ParentAndName(char *path,
	char **parent, char **name)
{
	bool IsOk = false;

	char *t = strrchr(path, '/');
	if (NULL == t) {
		fprintf(stderr, "no directory.\n");
	} else {
		int par_sz = t - path;
		char *par = (char *)malloc(par_sz + 1);
		if (NULL == par) {
			fprintf(stderr, "malloc failed.\n");
		} else {
			(void) memcpy(par, path, par_sz);
			par[par_sz] = 0;
			{
				int s_sz = strlen(path);
				int child_sz = s_sz - par_sz - 1;
				char *child = (char *)malloc(child_sz + 1);
				if (NULL == child) {
					fprintf(stderr, "malloc failed.\n");
				} else {
					(void) memcpy(child, t + 1, child_sz);
					child[child_sz] = 0;

					*name = child;
					IsOk = true;
					/* free(child); */
				}
			}
			if (! IsOk) {
				free(par);
			} else {
				*parent = par;
			}
		}
	}

	return IsOk;
}
#endif

#if CanGetAppPath
static bool InitWhereAmI(void)
{
	char *s;

	if (!
#if HaveAppPathLink
		ReadLink_Alloc(TheAppPathLink, &s)
#endif
#if HaveSysctlPath
		ReadKernProcPathname(&s)
#endif
		)
	{
		fprintf(stderr, "InitWhereAmI fails.\n");
	} else {
		if (! Path2ParentAndName(s, &app_parent, &app_name)) {
			fprintf(stderr, "Path2ParentAndName fails.\n");
		} else {
			/* ok */
			/*
				fprintf(stderr, "parent = %s.\n", app_parent);
				fprintf(stderr, "name = %s.\n", app_name);
			*/
		}

		free(s);
	}

	return true; /* keep going regardless */
}
#endif

#if CanGetAppPath
static void UninitWhereAmI(void)
{
	MyMayFree(app_parent);
	MyMayFree(app_name);
}
#endif

static bool InitOSGLU(void)
{
	if (AllocMyMemory())
#if CanGetAppPath
	if (InitWhereAmI())
#endif
#if dbglog_HAVE
	if (dbglog_open())
#endif
	if (ScanCommandLine())
	if (LoadMacRom())
	if (LoadInitialImages())
#if UseActvCode
	if (ActvCodeInit())
#endif
	if (InitLocationDat())
#if MySoundEnabled
	if (MySound_Init())
#endif
	if (Screen_Init())
	if (CreateMainWindow())
	if (KC2MKCInit())
#if EmLocalTalk
	if (EntropyGather())
	if (InitLocalTalk())
#endif
	if (WaitForRom())
	{
		return true;
	}
	return false;
}

static void UnInitOSGLU(void)
{
	if (MacMsgDisplayed) {
		MacMsgDisplayOff();
	}

#if EmLocalTalk
	UnInitLocalTalk();
#endif

	RestoreKeyRepeat();
#if MayFullScreen
	UngrabMachine();
#endif
#if MySoundEnabled
	MySound_Stop();
#endif
#if MySoundEnabled
	MySound_UnInit();
#endif
#if IncludeHostTextClipExchange
	FreeMyClipBuffer();
#endif
#if IncludePbufs
	UnInitPbufs();
#endif
	UnInitDrives();

	ForceShowCursor();
	if (blankCursor != None) {
		XFreeCursor(x_display, blankCursor);
	}

	if (my_image != NULL) {
		XDestroyImage(my_image);
	}
#if EnableMagnify
	if (my_Scaled_image != NULL) {
		XDestroyImage(my_Scaled_image);
	}
#endif

	CloseMainWindow();
	if (x_display != NULL) {
		XCloseDisplay(x_display);
	}

#if dbglog_HAVE
	dbglog_close();
#endif

#if CanGetAppPath
	UninitWhereAmI();
#endif
	UnallocMyMemory();

	CheckSavedMacMsg();
}

int main(int argc, char **argv)
{
	my_argc = argc;
	my_argv = argv;

	ZapOSGLUVars();
	if (InitOSGLU()) {
		ProgramMain();
	}
	UnInitOSGLU();

	return 0;
}

#endif /* WantOSGLUXWN */
