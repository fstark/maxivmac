/*
 * unsit_mac.c -- StuffIt archive extractor, Mac console (THINK C)
 *
 * Phase 2 baby step: uses stdio for I/O, creates files with
 * .rsrc and .finderinfo suffixes (same as Linux CLI output).
 * Purpose: verify decompressors work correctly on 68K.
 *
 * Ported from XADMaster (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"
#include <Files.h>
#include <Events.h>
#include <Memory.h>
#include <StandardFile.h>
#include <console.h>

#define noPROFILE
#ifdef PROFILE
#include "profile.h"
#endif

/* ????????????????????????????????????????????????????
   UFile -- stdio implementation (same as Linux)
   ???????????????????????????????????????????????????? */

int uf_open(UFile *uf, const char *path)
{
	uf->fp = fopen(path, "rb");
	if (!uf->fp) return -1;
	uf->base = 0;
	fseek(uf->fp, 0, SEEK_END);
	uf->length = (s32)ftell(uf->fp);
	fseek(uf->fp, 0, SEEK_SET);
	return 0;
}

void uf_close(UFile *uf)
{
	if (uf->fp)
	{
		fclose(uf->fp);
		uf->fp = NULL;
	}
}

int uf_seek(UFile *uf, s32 offset)
{
	return fseek(uf->fp, uf->base + offset, SEEK_SET);
}

int uf_read(UFile *uf, void *buf, s32 count)
{
	size_t n = fread(buf, 1, (size_t)count, uf->fp);
	return (s32)n == count ? 0 : -1;
}

s32 uf_tell(UFile *uf)
{
	return (s32)ftell(uf->fp) - uf->base;
}

u8 uf_read8(UFile *uf)
{
	int c = fgetc(uf->fp);
	return (u8)(c == EOF ? 0 : c);
}

u16 uf_read16(UFile *uf)
{
	u8 b[2];
	if (fread(b, 1, 2, uf->fp) != 2) return 0;
	return (u16)((u16)b[0] << 8 | b[1]);
}

u32 uf_read32(UFile *uf)
{
	u8 b[4];
	if (fread(b, 1, 4, uf->fp) != 4) return 0;
	return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3];
}

void uf_skip(UFile *uf, s32 count)
{
	fseek(uf->fp, count, SEEK_CUR);
}

/* ????????????????????????????????????????????????????
   Mac directory creation via Toolbox
   ???????????????????????????????????????????????????? */

/*
 * Create a nested directory path.  Components are separated by ':'.
 * Uses PBDirCreate relative to the app's working directory.
 * After return, mkd_vref/mkd_dirid hold the final directory.
 */
static short mkd_vref;
static long mkd_dirid;

static void mac_mkdirs(const char *path)
{
	char component[36];
	Str63 pname;
	CInfoPBRec cpb;
	HParamBlockRec hpb;
	const char *p = path;
	int i;
	OSErr err;

	/* Start from the app's working directory */
	HGetVol(NULL, &mkd_vref, &mkd_dirid);

	if (*p == ':') p++;

	while (*p)
	{
		i = 0;
		while (*p && *p != ':' && i < 31)
			component[i++] = *p++;
		if (*p == ':') p++;
		if (i == 0) continue;
		component[i] = '\0';

		/* Pascal string */
		pname[0] = (unsigned char)i;
		memcpy(pname + 1, component, i);

		/* Try to create */
		memset(&hpb, 0, sizeof(hpb));
		hpb.fileParam.ioNamePtr = pname;
		hpb.fileParam.ioVRefNum = mkd_vref;
		hpb.fileParam.ioDirID = mkd_dirid;
		err = PBDirCreate((HParmBlkPtr)&hpb, false);

		if (err == noErr)
		{
			mkd_dirid = hpb.fileParam.ioDirID;
		}
		else
		{
			/* Already exists -- look up its dirID */
			memset(&cpb, 0, sizeof(cpb));
			cpb.dirInfo.ioNamePtr = pname;
			cpb.dirInfo.ioVRefNum = mkd_vref;
			cpb.dirInfo.ioDrDirID = mkd_dirid;
			cpb.dirInfo.ioFDirIndex = 0;
			if (PBGetCatInfo(&cpb, false) == noErr)
				mkd_dirid = cpb.dirInfo.ioDrDirID;
			else
				break;
		}
	}
}

/* Convert '/' to ':' in-place */
static void to_mac_sep(char *s)
{
	while (*s)
	{
		if (*s == '/') *s = ':';
		s++;
	}
}

/* Make a Pascal string from a C string (max 31 chars for HFS) */
static void c2pstr_hfs(Str63 pstr, const char *cstr)
{
	int len = strlen(cstr);
	if (len > 31) len = 31;
	pstr[0] = (unsigned char)len;
	memcpy(pstr + 1, cstr, len);
}

/* ====================================================
   Output -- native Mac File Manager
   ==================================================== */

static char output_dir[256];
static int error_count;

/* ====================================================
   Progress display callback
   ==================================================== */

static s32 prog_last_dot; /* bytes at which last dot was printed */
static s32 prog_dot_interval;

static void mac_progress_tick(s32 bytes_so_far, s32 total)
{
	s32 dots_now, dots_prev, i;
	(void)total;
	dots_now = bytes_so_far / prog_dot_interval;
	dots_prev = prog_last_dot / prog_dot_interval;
	for (i = dots_prev; i < dots_now; i++)
		putchar(progress_char);
	if (dots_now > dots_prev) fflush(stdout);
	prog_last_dot = bytes_so_far;
}

/* ====================================================
   Fork extraction helper
   ==================================================== */

static int extract_fork(UFile *archive, short vRef, long dirID, const unsigned char *pname,
						int is_rsrc, s32 offset, s32 length, s32 comp_length, int method,
						u16 expected_crc)
{
	u8 *buf;
	OSErr err;
	const char *label = is_rsrc ? "rsrc" : "data";
	unsigned long t0, elapsed_ms;

	if (length <= 0) return 0;

	printf("  %s: [%-8s] %ld bytes: ", label, method_name(method), (long)length);
	fflush(stdout);

	/* Check available memory -- leave 512K headroom for decompressor buffers + stack */
	if ((long)FreeMem() < length + 524288L)
	{
		printf("SKIP (need %ld, have %ld free)\n", (long)length + 524288L, (long)FreeMem());
		fflush(stdout);
		error_count++;
		return -1;
	}

	buf = (u8 *)malloc((size_t)length);
	if (!buf)
	{
		printf("FAIL (malloc)\n");
		fflush(stdout);
		error_count++;
		return -1;
	}

	/* Set up progress state -- aim for ~50 dots max */
	prog_dot_interval = length / 50;
	if (prog_dot_interval < 1024) prog_dot_interval = 1024;
	prog_last_dot = 0;
	progress_tick = mac_progress_tick;

	t0 = TickCount();
	uf_seek(archive, offset);
	if (decompress(archive, method, buf, length, comp_length) != 0)
	{
		progress_tick = NULL;
		printf(" FAIL\n");
		fflush(stdout);
		free(buf);
		error_count++;
		return -1;
	}

	progress_tick = NULL;
	elapsed_ms = (TickCount() - t0) * 50 / 3;

	/* CRC-16 verification (arsenic has its own CRC-32 check) */
	if (expected_crc != 0 && method != 15)
	{
		u16 actual_crc = crc16(buf, length);
		if (actual_crc != expected_crc)
		{
			printf(" CRC MISMATCH (expected %04x, got %04x)\n", (unsigned)expected_crc,
				   (unsigned)actual_crc);
			fflush(stdout);
			error_count++;
		}
	}

	/* Write to fork */
	{
		short refNum;
		long count = length;
		if (is_rsrc)
			err = HOpenRF(vRef, dirID, pname, fsWrPerm, &refNum);
		else
			err = HOpen(vRef, dirID, pname, fsWrPerm, &refNum);
		if (err == noErr)
		{
			FSWrite(refNum, &count, buf);
			FSClose(refNum);
		}
	}

	printf(" done (%lu ms)\n", elapsed_ms);
	fflush(stdout);
	free(buf);
	return 0;
}

/* ====================================================
   Entry callback
   ==================================================== */

static int extract_entry(const char *path, const SitEntry *entry, UFile *archive, void *ctx)
{
	static char dir_path[256];
	Str63 pname;
	short vRef;
	long dirID;
	OSErr err;
	(void)ctx;

	printf("[%s%s%s]\n", path[0] ? path : "", path[0] ? ":" : "", entry->name);
	fflush(stdout);

	/* Build Mac-style directory path (convert / to :) */
	if (path[0] != '\0')
		snprintf(dir_path, sizeof(dir_path), "%s:%s", output_dir, path);
	else
		snprintf(dir_path, sizeof(dir_path), "%s", output_dir);
	to_mac_sep(dir_path);

	if (entry->is_dir)
	{
		static char full_dir[256];
		snprintf(full_dir, sizeof(full_dir), "%s:%s", dir_path, entry->name);
		mac_mkdirs(full_dir);
		return 0;
	}

	/* Ensure parent directory exists; get its vRef+dirID */
	mac_mkdirs(dir_path);
	vRef = mkd_vref;
	dirID = mkd_dirid;

	c2pstr_hfs(pname, entry->name);

	/* Delete any existing file, then create with type/creator */
	HDelete(vRef, dirID, pname);
	err = HCreate(vRef, dirID, pname, (OSType)entry->info.creator, (OSType)entry->info.type);
	if (err != noErr)
	{
		printf("  HCreate err=%d\n", (int)err);
		fflush(stdout);
		return 0; /* skip file, don't abort archive */
	}

	/* Extract both forks */
	extract_fork(archive, vRef, dirID, pname, 0, entry->data_offset, entry->data_length,
				 entry->data_comp_length, entry->data_method, entry->data_crc);

	extract_fork(archive, vRef, dirID, pname, 1, entry->rsrc_offset, entry->rsrc_length,
				 entry->rsrc_comp_length, entry->rsrc_method, entry->rsrc_crc);

	/* -- Finder info ------------------------ */
	{
		FInfo fi;
		memset(&fi, 0, sizeof(fi));
		fi.fdType = (OSType)entry->info.type;
		fi.fdCreator = (OSType)entry->info.creator;
		fi.fdFlags = entry->info.flags;
		HSetFInfo(vRef, dirID, pname, &fi);
	}

	return 0;
}

/* ????????????????????????????????????????????????????
   Main
   ???????????????????????????????????????????????????? */

static void strip_extension(char *dest, const char *src, int dest_size)
{
	const char *base;
	char *dot;

	/* Find the last path component (after last / or :) */
	base = strrchr(src, '/');
	if (!base) base = strrchr(src, ':');
	base = base ? base + 1 : src;

	strncpy(dest, base, dest_size - 1);
	dest[dest_size - 1] = '\0';

	dot = strrchr(dest, '.');
	if (dot && (strcmp(dot, ".sit") == 0 || strcmp(dot, ".SIT") == 0)) *dot = '\0';
}

int main(void)
{
	UFile uf;
	int fmt, rc;
	SFReply reply;
	Point where;
	static char filename[256];

#ifdef PROFILE
	InitProfile(400, 50); // 400 functions, 50 call depth
	freopen("profiler.txt", "w", stdout);
#endif

	/* Reserve 64K for stack — must be before any heap growth.
	   Lowers the heap ceiling so malloc can't eat into stack space. */
	SetApplLimit(GetApplLimit() - 0x10000L);
	MaxApplZone();

	// We print this banner to make sure the ansi lib calls all the InitXXX
	// otherwise the SFGetFile call will crash the program.
	fprintf(stderr, "maxivmac Unsit\n");

	where.h = 80;
	where.v = 80;
	SFGetFile(where, "\p", NULL, -1, NULL, NULL, &reply);
	if (!reply.good) return 0;

	/* Convert Pascal filename to C string */
	{
		int len = reply.fName[0];
		memcpy(filename, reply.fName + 1, len);
		filename[len] = '\0';
	}

	/* Set working directory to the file's volume/directory */
	HSetVol(NULL, reply.vRefNum, 0);

	printf("Opening: %s\n", filename);

	if (uf_open(&uf, filename) != 0)
	{
		fprintf(stderr, "unsit: cannot open '%s'\n", filename);
		return 1;
	}

	detect_macbinary(&uf);
	fmt = detect_format(&uf);

	strip_extension(output_dir, filename, sizeof(output_dir));
	mac_mkdirs(output_dir);

	switch (fmt)
	{
		case FMT_SIT5:
			printf("StuffIt 5 archive: %s\n", filename);
			rc = sit5_parse(&uf, extract_entry, NULL);
			break;
		case FMT_CLASSIC:
			printf("Classic StuffIt archive: %s\n", filename);
			rc = sit1_parse(&uf, extract_entry, NULL);
			break;
		default:
			fprintf(stderr, "unsit: not a StuffIt archive: %s\n", filename);
			uf_close(&uf);
			return 1;
	}

	uf_close(&uf);
	if (error_count > 0) printf("\n%d file(s) with errors\n", error_count);
	fprintf(stderr, "Finished unsit\n");
	return rc != 0 ? 1 : 0;
}
