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
	DriveIndex Drive_No, uint32_t Sony_Start, uint32_t Sony_Count,
	uint32_t *Sony_ActCount);
tMacErr vSonyGetSize(DriveIndex Drive_No, uint32_t *Sony_Count);
tMacErr vSonyEject(DriveIndex Drive_No);
tMacErr vSonyEjectDelete(DriveIndex Drive_No);
tMacErr vSonyGetName(DriveIndex Drive_No, PbufIndex *r);

bool Sony_Insert0(FILE * refnum, bool locked, char *drivepath);
bool Sony_Insert1(char *drivepath, bool silentfail);
void MakeNewDisk(uint32_t L, char *drivename);
