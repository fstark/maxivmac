/*
 * sit5.c -- StuffIt 5 container parser
 *
 * Ported from XADMaster XADStuffIt5Parser.m (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

/* -- Directory path tracking ----------------------- */

#define MAX_DIRS 256

typedef struct
{
	s32 offset;		/* absolute offset of directory entry */
	char path[256]; /* accumulated path prefix */
} DirEntry;

static DirEntry *dir_table;
static int dir_count;

static void dir_init(void)
{
	dir_count = 0;
	dir_table = (DirEntry *)calloc(MAX_DIRS, sizeof(DirEntry));
}

static void dir_add(s32 offset, const char *path)
{
	if (dir_count < MAX_DIRS)
	{
		dir_table[dir_count].offset = offset;
		snprintf(dir_table[dir_count].path, sizeof(dir_table[0].path), "%s", path);
		dir_count++;
	}
}

static const char *dir_lookup(s32 offset)
{
	int i;
	for (i = 0; i < dir_count; i++)
	{
		if (dir_table[i].offset == offset) return dir_table[i].path;
	}
	return "";
}

/* -- Parser ---------------------------------------- */

int sit5_parse(UFile *uf, EntryCallback cb, void *ctx)
{
	s32 pos;
	u32 entry_id;

	dir_init();

	/* Skip archive header (0x72 = 114 bytes) */
	pos = 0x72;

	for (;;)
	{
		SitEntry entry;
		int version;
		int header_size;
		int flags;
		s32 prev_off, next_off, parent_off;
		int name_len;
		int data_method;
		int sb_flags;
		int padding_size;
		s32 entry_offset;
		static char parent_path[256];
		static char full_path[512];

		/* Read entry ID */
		uf_seek(uf, pos);
		entry_id = uf_read32(uf);
		if (entry_id != 0xA5A5A5A5UL) break;

		entry_offset = pos;
		memset(&entry, 0, sizeof(entry));

		/* -- Entry header ----------------------- */
		version = uf_read8(uf);		 /* +4 */
		uf_skip(uf, 1);				 /* +5 unknown */
		header_size = uf_read16(uf); /* +6 */
		uf_skip(uf, 1);				 /* +8 system ID */
		flags = uf_read8(uf);		 /* +9 */

		entry.info.creation = uf_read32(uf);	 /* +10 */
		entry.info.modification = uf_read32(uf); /* +14 */

		prev_off = (s32)uf_read32(uf);	 /* +18 */
		next_off = (s32)uf_read32(uf);	 /* +22 */
		parent_off = (s32)uf_read32(uf); /* +26 */

		name_len = uf_read16(uf); /* +30 */
		uf_skip(uf, 2);			  /* +32 header CRC */

		entry.data_length = (s32)uf_read32(uf);		 /* +34 */
		entry.data_comp_length = (s32)uf_read32(uf); /* +38 */
		entry.data_crc = uf_read16(uf);				 /* +42 */
		uf_skip(uf, 2);								 /* +44 unknown */

		entry.is_dir = (flags & 0x40) != 0;

		/* +46: method, +47: password (only for non-dir) */
		data_method = uf_read8(uf); /* +46 */
		uf_skip(uf, 1);				/* +47 password data len */

		if (!entry.is_dir) entry.data_method = data_method;

		/* +48: filename */
		if (name_len > MAX_FILENAME - 1) name_len = MAX_FILENAME - 1;
		uf_read(uf, entry.name, name_len);
		entry.name[name_len] = '\0';

		(void)prev_off;
		(void)next_off;

		/* -- Build path from parent chain ------- */
		if (parent_off == 0)
		{
			parent_path[0] = '\0';
		}
		else
		{
			const char *pp = dir_lookup(parent_off);
			strncpy(parent_path, pp, sizeof(parent_path) - 1);
			parent_path[sizeof(parent_path) - 1] = '\0';
		}

		if (entry.is_dir)
		{
			/* Build this directory's full path */
			if (parent_path[0] != '\0')
				snprintf(full_path, sizeof(full_path), "%s/%s", parent_path, entry.name);
			else
				snprintf(full_path, sizeof(full_path), "%s", entry.name);
			dir_add(entry_offset, full_path);
		}
		else
		{
			strncpy(full_path, parent_path, sizeof(full_path) - 1);
			full_path[sizeof(full_path) - 1] = '\0';
		}

		/* -- Folder close marker ---------------- */
		/* A directory entry with no name signals end-of-folder.
		   It has no second block and no data -- skip straight to
		   the next entry at pos + header_size. */
		if (entry.is_dir && name_len == 0)
		{
			pos = pos + header_size;
			if (pos <= 0 || pos >= uf->length) break;
			continue;
		}

		/* -- Second block ----------------------- */
		/* Seek past the entry header to the second block */
		uf_seek(uf, pos + header_size);

		sb_flags = uf_read16(uf);			/* +0 flags */
		uf_skip(uf, 2);						/* +2 unknown */
		entry.info.type = uf_read32(uf);	/* +4 type */
		entry.info.creator = uf_read32(uf); /* +8 creator */
		entry.info.flags = uf_read16(uf);	/* +12 Finder flags */

		/* +14: padding (22 bytes for version 1, 18 otherwise) */
		padding_size = (version == 1) ? 22 : 18;
		uf_skip(uf, padding_size);

		/* -- Resource fork info (if present) ---- */
		if ((sb_flags & 1) && !entry.is_dir)
		{
			entry.rsrc_length = (s32)uf_read32(uf);
			entry.rsrc_comp_length = (s32)uf_read32(uf);
			entry.rsrc_crc = uf_read16(uf);
			uf_skip(uf, 2); /* unknown */
			entry.rsrc_method = uf_read8(uf);
			uf_skip(uf, 1); /* rsrc password len */

			/* Resource fork data comes first */
			entry.rsrc_offset = uf_tell(uf);
			entry.data_offset = entry.rsrc_offset + entry.rsrc_comp_length;
		}
		else if (!entry.is_dir)
		{
			/* No resource fork -- data starts here */
			entry.data_offset = uf_tell(uf);
		}

		/* -- Invoke callback -------------------- */
		if (cb(full_path, &entry, uf, ctx) != 0)
		{
			free(dir_table);
			dir_table = NULL;
			return -1;
		}

		/* -- Advance to next entry -------------- */
		if (entry.is_dir)
		{
			/* Children follow immediately after second block */
			pos = uf_tell(uf);
		}
		else
		{
			/* Skip past compressed data */
			pos = entry.data_offset + entry.data_comp_length;
		}

		/* Safety: don't read past EOF */
		if (pos <= 0 || pos >= uf->length) break;
	}

	free(dir_table);
	dir_table = NULL;
	return 0;
}
