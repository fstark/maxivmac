/*
	lomem_globals.h — Macintosh low-memory global variable descriptors

	Static table of ~160 well-known low-memory globals ($0100–$0DFF)
	with type, category, and formatting helpers for the ImGui viewer.
*/

#pragma once

#include <cstdint>

enum LMType
{
	LM_BYTE,	/*  1 byte  */
	LM_WORD,	/*  2 bytes */
	LM_LONG,	/*  4 bytes */
	LM_POINTER, /*  4 bytes, displayed as address */
	LM_HANDLE,	/*  4 bytes, displayed as handle */
	LM_PSTRING, /* length-prefixed Pascal string */
	LM_BYTES,	/* raw byte array */
	LM_RECT,	/*  8 bytes: top, left, bottom, right */
	LM_PATTERN, /*  8 bytes pattern */
	LM_OSType,	/*  4 bytes four-char code */
};

enum LMCategory
{
	LM_CAT_SYSTEM,
	LM_CAT_HARDWARE,
	LM_CAT_TIMING,
	LM_CAT_INTERRUPT,
	LM_CAT_IO,
	LM_CAT_EVENTS,
	LM_CAT_WINDOW,
	LM_CAT_MENU,
	LM_CAT_TEXTEDIT,
	LM_CAT_RESOURCE,
	LM_CAT_SCRAP,
	LM_CAT_PRINTING,
	LM_CAT_SOUND,
	LM_CAT_FONT,
	LM_CAT_APP,
	LM_CAT_MISC,
	LM_CAT_COUNT
};

struct LMGlobal
{
	const char *name;
	uint32_t addr;
	uint16_t size;
	LMType type;
	LMCategory category;
	const char *brief;
};

/* Full table (sorted by address). */
extern const LMGlobal kLowMemGlobals[];
extern const int kLowMemCount;

/* Human-readable category labels (LM_CAT_COUNT entries). */
extern const char *const kLMCategoryLabels[];

/* Take a 4 KB snapshot of low memory ($000–$FFF) from g_ram. */
void Lomem_SnapshotTake(uint8_t *snapshot);

/* Returns true if live RAM at [addr, addr+size) differs from snapshot. */
bool lomem_snapshot_changed(const uint8_t *snapshot, uint32_t addr, uint16_t size);

/* Format a low-memory global's value into buf (null-terminated).
   Returns buf for convenience.  bufSize should be >= 64. */
char *lomem_format_value(const LMGlobal *g, char *buf, int bufSize);

/* Short type label for display column. */
const char *lomem_type_label(LMType t);
