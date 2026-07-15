/*
 * sit1.c — Classic StuffIt (SIT!) container parser
 *
 * Ported from XADMaster XADStuffItParser.m (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

/* ── Header field offsets ────────────────────────── */

#define SITFH_COMPRMETHOD    0   /* u8  rsrc compression method */
#define SITFH_COMPDMETHOD    1   /* u8  data compression method */
#define SITFH_FNAMESIZE      2   /* u8  filename length (max 31) */
#define SITFH_FNAME          3   /* 31 bytes filename */
#define SITFH_FTYPE         66   /* u32 file type */
#define SITFH_CREATOR       70   /* u32 creator */
#define SITFH_FNDRFLAGS     74   /* u16 Finder flags */
#define SITFH_CREATIONDATE  76   /* u32 creation date */
#define SITFH_MODDATE       80   /* u32 modification date */
#define SITFH_RSRCLENGTH    84   /* u32 rsrc decompressed length */
#define SITFH_DATALENGTH    88   /* u32 data decompressed length */
#define SITFH_COMPRLENGTH   92   /* u32 rsrc compressed length */
#define SITFH_COMPDLENGTH   96   /* u32 data compressed length */
#define SITFH_RSRCCRC      100   /* u16 rsrc CRC-16 */
#define SITFH_DATACRC      102   /* u16 data CRC-16 */
#define SITFH_HDRCRC       110   /* u16 header CRC-16 */
#define SIT_FILEHDRSIZE    112

#define StuffItEncryptedFlag       0x80
#define StuffItStartFolder         0x20
#define StuffItEndFolder           0x21
#define StuffItFolderContainsEnc   0x10
#define StuffItMethodMask          (~StuffItEncryptedFlag)
#define StuffItFolderMask          (~(StuffItEncryptedFlag | StuffItFolderContainsEnc))

/* ── Helpers ─────────────────────────────────────── */

static u32 read_be32(const u8 *p) {
    return (u32)p[0]<<24 | (u32)p[1]<<16 | (u32)p[2]<<8 | p[3];
}
static u16 read_be16(const u8 *p) {
    return (u16)((u16)p[0]<<8 | p[1]);
}

/* ── Directory stack ─────────────────────────────── */

#define MAX_DEPTH 32

static char path_stack[MAX_DEPTH][256];
static int  path_depth;

static void path_init(void)
{
    path_depth = 0;
}

static void path_push(const char *name)
{
    if (path_depth < MAX_DEPTH) {
        snprintf(path_stack[path_depth], sizeof(path_stack[0]),
                 "%s", name);
        path_depth++;
    }
}

static void path_pop(void)
{
    if (path_depth > 0)
        path_depth--;
}

static void path_build(char *buf, int bufsz)
{
    int i, pos = 0;
    buf[0] = '\0';
    for (i = 0; i < path_depth; i++) {
        int n = snprintf(buf + pos, bufsz - pos, "%s%s",
                         (i > 0) ? "/" : "", path_stack[i]);
        pos += n;
        if (pos >= bufsz) break;
    }
}

/* ── Parser ──────────────────────────────────────── */

int sit1_parse(UFile *uf, EntryCallback cb, void *ctx)
{
    u8 archdr[22];
    s32 totalsize;

    path_init();

    /* Read and verify archive header */
    uf_seek(uf, 0);
    if (uf_read(uf, archdr, 22) != 0)
        return -1;

    if (read_be32(archdr) != 0x53495421UL ||     /* SIT! */
        read_be32(archdr + 10) != 0x724C6175UL)  /* rLau */
        return -1;

    totalsize = (s32)read_be32(archdr + 6);

    /* Entries start at offset 22 */
    uf_seek(uf, 22);

    while (uf_tell(uf) + SIT_FILEHDRSIZE <= totalsize) {
        u8 hdr[SIT_FILEHDRSIZE];
        SitEntry entry;
        s32 start;
        int rsrc_method, data_method;
        int namelen;
        char path_buf[1024];
        u16 hdr_crc, calc_crc;

        if (uf_read(uf, hdr, SIT_FILEHDRSIZE) != 0)
            break;

        /* Validate header CRC */
        hdr_crc = read_be16(hdr + SITFH_HDRCRC);
        /* Zero out CRC field for calculation */
        hdr[110] = 0;
        hdr[111] = 0;
        calc_crc = crc16(hdr, 110);
        if (hdr_crc != calc_crc) {
            fprintf(stderr, "unsit: classic: header CRC mismatch "
                    "(expected %04x, got %04x)\n",
                    (unsigned)hdr_crc, (unsigned)calc_crc);
            break;
        }

        start = uf_tell(uf);

        rsrc_method = hdr[SITFH_COMPRMETHOD];
        data_method = hdr[SITFH_COMPDMETHOD];

        memset(&entry, 0, sizeof(entry));

        /* Filename */
        namelen = hdr[SITFH_FNAMESIZE];
        if (namelen > 31) namelen = 31;
        memcpy(entry.name, hdr + SITFH_FNAME, namelen);
        entry.name[namelen] = '\0';

        /* Finder info */
        entry.info.type         = read_be32(hdr + SITFH_FTYPE);
        entry.info.creator      = read_be32(hdr + SITFH_CREATOR);
        entry.info.flags        = read_be16(hdr + SITFH_FNDRFLAGS);
        entry.info.creation     = read_be32(hdr + SITFH_CREATIONDATE);
        entry.info.modification = read_be32(hdr + SITFH_MODDATE);

        /* Sizes */
        entry.rsrc_length      = (s32)read_be32(hdr + SITFH_RSRCLENGTH);
        entry.data_length      = (s32)read_be32(hdr + SITFH_DATALENGTH);
        entry.rsrc_comp_length = (s32)read_be32(hdr + SITFH_COMPRLENGTH);
        entry.data_comp_length = (s32)read_be32(hdr + SITFH_COMPDLENGTH);
        entry.rsrc_crc         = read_be16(hdr + SITFH_RSRCCRC);
        entry.data_crc         = read_be16(hdr + SITFH_DATACRC);

        entry.rsrc_method = rsrc_method & 0x0F;
        entry.data_method = data_method & 0x0F;

        /* ── Folder handling ─────────────────── */
        if ((data_method & StuffItFolderMask) == StuffItStartFolder ||
            (rsrc_method & StuffItFolderMask) == StuffItStartFolder) {

            entry.is_dir = 1;
            path_push(entry.name);
            path_build(path_buf, sizeof(path_buf));

            if (cb(path_buf, &entry, uf, ctx) != 0)
                return -1;

            /* No data follows a folder start marker */
            uf_seek(uf, start);
            continue;
        }

        if ((data_method & StuffItFolderMask) == StuffItEndFolder ||
            (rsrc_method & StuffItFolderMask) == StuffItEndFolder) {

            path_pop();
            /* No data follows a folder end marker */
            uf_seek(uf, start);
            continue;
        }

        /* ── Regular file ────────────────────── */
        /* Data layout after header: rsrc compressed, then data compressed */
        entry.rsrc_offset = start;
        entry.data_offset = start + entry.rsrc_comp_length;

        path_build(path_buf, sizeof(path_buf));

        if (cb(path_buf, &entry, uf, ctx) != 0)
            return -1;

        /* Seek past both compressed forks */
        uf_seek(uf, start + entry.rsrc_comp_length + entry.data_comp_length);
    }

    return 0;
}
