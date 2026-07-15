/*
 * unsit.c — StuffIt archive extractor, Linux CLI
 *
 * Platform-specific: stdio-based UFile, Unix file output, main().
 * Portable code lives in core.c and the decompressor files.
 *
 * Ported from XADMaster (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

/* ════════════════════════════════════════════════════
   UFile — I/O abstraction (Phase 1: stdio)
   ════════════════════════════════════════════════════ */

int uf_open(UFile *uf, const char *path)
{
    uf->fp = fopen(path, "rb");
    if (!uf->fp)
        return -1;
    uf->base = 0;
    fseek(uf->fp, 0, SEEK_END);
    uf->length = (s32)ftell(uf->fp);
    fseek(uf->fp, 0, SEEK_SET);
    return 0;
}

void uf_close(UFile *uf)
{
    if (uf->fp) {
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
    if (fread(b, 1, 2, uf->fp) != 2)
        return 0;
    return (u16)((u16)b[0] << 8 | b[1]);
}

u32 uf_read32(UFile *uf)
{
    u8 b[4];
    if (fread(b, 1, 4, uf->fp) != 4)
        return 0;
    return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3];
}

void uf_skip(UFile *uf, s32 count)
{
    fseek(uf->fp, count, SEEK_CUR);
}

/* ════════════════════════════════════════════════════
   Output writer
   ════════════════════════════════════════════════════ */

#ifndef __THINK__
#include <sys/stat.h>
#endif

static char output_dir[512];

#ifndef __THINK__
static int mkdirs(const char *path)
{
    char tmp[1024];
    char *p;
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
    return 0;
}

/* Create parent directories for a file path */
static int mkdirs_for_file(const char *filepath)
{
    char tmp[1024];
    char *slash;
    strncpy(tmp, filepath, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    slash = strrchr(tmp, '/');
    if (slash) {
        *slash = '\0';
        return mkdirs(tmp);
    }
    return 0;
}
#endif

static void write_be32(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static void write_be16(u8 *p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static int write_file(const char *path, const u8 *data, s32 length)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "unsit: cannot create '%s'\n", path);
        return -1;
    }
    if (length > 0)
        fwrite(data, 1, (size_t)length, fp);
    fclose(fp);
    return 0;
}

static int extract_entry(const char *path, const SitEntry *entry,
                         UFile *archive, void *ctx)
{
    static char out_path[512];
    static char base_path[512];
    (void)ctx;

    /* Build base output directory for this entry */
    if (path[0] != '\0')
        snprintf(base_path, sizeof(base_path), "%s/%s", output_dir, path);
    else
        snprintf(base_path, sizeof(base_path), "%s", output_dir);

    if (entry->is_dir) {
        mkdirs(base_path);
        printf("  %s/\n", path);
        return 0;
    }

    /* ── Data fork ──────────────────────────── */
    snprintf(out_path, sizeof(out_path), "%s/%s", base_path, entry->name);
    mkdirs_for_file(out_path);

    if (entry->data_length > 0) {
        u8 *buf;

        buf = (u8 *)malloc((size_t)entry->data_length);
        if (!buf) {
            fprintf(stderr, "unsit: out of memory (%ld bytes)\n",
                    (long)entry->data_length);
            return -1;
        }
        uf_seek(archive, entry->data_offset);
        if (decompress(archive, entry->data_method, buf,
                       entry->data_length, entry->data_comp_length) != 0) {
            fprintf(stderr, "unsit: failed to decompress data fork of '%s'\n",
                    entry->name);
        } else {
            write_file(out_path, buf, entry->data_length);
        }
        free(buf);
    } else if (entry->rsrc_length == 0) {
        /* Create empty data fork only if no rsrc fork either */
        write_file(out_path, NULL, 0);
    }

    if (path[0] != '\0')
        printf("  %s/%s\n", path, entry->name);
    else
        printf("  %s\n", entry->name);

    /* ── Resource fork ──────────────────────── */
    if (entry->rsrc_length > 0) {
        u8 *buf;
        snprintf(out_path, sizeof(out_path), "%s/%s.rsrc",
                 base_path, entry->name);

        buf = (u8 *)malloc((size_t)entry->rsrc_length);
        if (!buf) {
            fprintf(stderr, "unsit: out of memory (%ld bytes)\n",
                    (long)entry->rsrc_length);
            return -1;
        }
        uf_seek(archive, entry->rsrc_offset);
        if (decompress(archive, entry->rsrc_method, buf,
                       entry->rsrc_length, entry->rsrc_comp_length) != 0) {
            fprintf(stderr, "unsit: failed to decompress rsrc fork of '%s' (method %d)\n",
                    entry->name, entry->rsrc_method);
            free(buf);
            /* continue — still write finderinfo */
        } else {
            write_file(out_path, buf, entry->rsrc_length);
            free(buf);
        }
    }

    /* ── Finder info (24 bytes, big-endian) ── */
    {
        u8 fi[24];
        memset(fi, 0, sizeof(fi));
        write_be32(fi + 0, entry->info.type);
        write_be32(fi + 4, entry->info.creator);
        write_be16(fi + 8, entry->info.flags);
        write_be16(fi + 10, entry->info.location_v);
        write_be16(fi + 12, entry->info.location_h);
        write_be16(fi + 14, entry->info.folder);
        write_be32(fi + 16, entry->info.creation);
        write_be32(fi + 20, entry->info.modification);

        snprintf(out_path, sizeof(out_path), "%s/%s.finderinfo",
                 base_path, entry->name);
        write_file(out_path, fi, 24);
    }

    return 0;
}

/* ════════════════════════════════════════════════════
   Main
   ════════════════════════════════════════════════════ */

static void strip_extension(char *dest, const char *src, size_t dest_size)
{
    const char *base;
    char *dot;

    /* Find the last path component (basename) */
    base = strrchr(src, '/');
    base = base ? base + 1 : src;

    strncpy(dest, base, dest_size - 1);
    dest[dest_size - 1] = '\0';

    /* Remove .sit extension */
    dot = strrchr(dest, '.');
    if (dot && (strcmp(dot, ".sit") == 0 || strcmp(dot, ".SIT") == 0))
        *dot = '\0';
}

int main(int argc, char **argv)
{
    UFile uf;
    int fmt, rc;

    if (argc < 2) {
        fprintf(stderr, "usage: unsit <archive.sit>\n");
        return 1;
    }

    if (uf_open(&uf, argv[1]) != 0) {
        fprintf(stderr, "unsit: cannot open '%s'\n", argv[1]);
        return 1;
    }

    /* Detect and skip MacBinary header */
    detect_macbinary(&uf);

    fmt = detect_format(&uf);

    /* Create output directory from archive name */
    strip_extension(output_dir, argv[1], sizeof(output_dir));
    mkdirs(output_dir);

    switch (fmt) {
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
