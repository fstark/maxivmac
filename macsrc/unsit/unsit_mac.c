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
#include <console.h>

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

static int extract_entry(const char *path, const SitEntry *entry, UFile *archive, void *ctx)
{
	static char dir_path[256];
	Str63 pname;
	short vRef;
	long dirID;
	OSErr err;
	(void)ctx;

	printf("[%s%s%s dm=%d rm=%d]\n", path[0] ? path : "", path[0] ? ":" : "", entry->name,
		   entry->data_method, entry->rsrc_method);
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

	/* -- Data fork -------------------------- */
	if (entry->data_length > 0)
	{
		u8 *buf = (u8 *)malloc((size_t)entry->data_length);
		if (!buf)
		{
			printf("  data malloc fail %ld\n", (long)entry->data_length);
			fflush(stdout);
		}
		else
		{
			uf_seek(archive, entry->data_offset);
			if (decompress(archive, entry->data_method, buf, entry->data_length,
						   entry->data_comp_length) == 0)
			{
				short refNum;
				long count = entry->data_length;
				err = HOpen(vRef, dirID, pname, fsWrPerm, &refNum);
				if (err == noErr)
				{
					FSWrite(refNum, &count, buf);
					FSClose(refNum);
				}
			}
			else
			{
				printf("  data decomp fail\n");
				fflush(stdout);
			}
			free(buf);
		}
	}

	/* -- Resource fork ---------------------- */
	if (entry->rsrc_length > 0)
	{
		u8 *buf = (u8 *)malloc((size_t)entry->rsrc_length);
		if (!buf)
		{
			printf("  rsrc malloc fail %ld\n", (long)entry->rsrc_length);
			fflush(stdout);
		}
		else
		{
			uf_seek(archive, entry->rsrc_offset);
			if (decompress(archive, entry->rsrc_method, buf, entry->rsrc_length,
						   entry->rsrc_comp_length) == 0)
			{
				short refNum;
				long count = entry->rsrc_length;
				err = HOpenRF(vRef, dirID, pname, fsWrPerm, &refNum);
				if (err == noErr)
				{
					FSWrite(refNum, &count, buf);
					FSClose(refNum);
				}
			}
			else
			{
				printf("  rsrc decomp fail\n");
				fflush(stdout);
			}
			free(buf);
		}
	}

	/* -- Finder info ------------------------ */
	{
		FInfo fi;
		memset(&fi, 0, sizeof(fi));
		fi.fdType = (OSType)entry->info.type;
		fi.fdCreator = (OSType)entry->info.creator;
		fi.fdFlags = entry->info.flags;
		HSetFInfo(vRef, dirID, pname, &fi);
	}

	printf("  ok\n");
	fflush(stdout);
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
	int argc;
	char **argv;
	UFile uf;
	int fmt, rc;
	static char *fake_argv[] = {"unsit", "x.sit", NULL};

	//    argc = ccommand(&argv);
	argc = 2;
	argv = fake_argv;

	fprintf(stderr, "Starting unsit\n");

	if (argc < 2)
	{
		fprintf(stderr, "usage: unsit <archive.sit>\n");
		return 1;
	}

	if (uf_open(&uf, argv[1]) != 0)
	{
		fprintf(stderr, "unsit: cannot open '%s'\n", argv[1]);
		return 1;
	}

	detect_macbinary(&uf);
	fmt = detect_format(&uf);

	strip_extension(output_dir, argv[1], sizeof(output_dir));
	mac_mkdirs(output_dir);

	switch (fmt)
	{
		case FMT_SIT5:
			printf("StuffIt 5 archive: %s\n", argv[1]);
			rc = sit5_parse(&uf, extract_entry, NULL);
			break;
		case FMT_CLASSIC:
			printf("Classic StuffIt archive: %s\n", argv[1]);
			rc = sit1_parse(&uf, extract_entry, NULL);
			break;
		default:
			fprintf(stderr, "unsit: not a StuffIt archive: %s\n", argv[1]);
			uf_close(&uf);
			return 1;
	}

	uf_close(&uf);
	return rc != 0 ? 1 : 0;
}
