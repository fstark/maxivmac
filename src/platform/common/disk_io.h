#pragma once

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/param_buffers.h"
#include "platform/common/mac_roman.h"
#include "lang/localization.h"

#include <sys/stat.h>

/* Disk image I/O functions. */

extern FILE *Drives[];
extern char *DriveNames[];

void InitDrives();
void UnInitDrives();

tMacErr vSonyTransfer(bool IsWrite, uint8_t * Buffer,
	tDrive Drive_No, uint32_t Sony_Start, uint32_t Sony_Count,
	uint32_t *Sony_ActCount);
tMacErr vSonyGetSize(tDrive Drive_No, uint32_t *Sony_Count);
tMacErr vSonyEject(tDrive Drive_No);
tMacErr vSonyEjectDelete(tDrive Drive_No);
tMacErr vSonyGetName(tDrive Drive_No, tPbuf *r);

bool Sony_Insert0(FILE * refnum, bool locked, char *drivepath);
bool Sony_Insert1(char *drivepath, bool silentfail);
void MakeNewDisk(uint32_t L, char *drivename);
