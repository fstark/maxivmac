#pragma once

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/param_buffers.h"
#include "platform/common/mac_roman.h"

#include <sys/stat.h>

/* Disk image I/O functions. */

extern FILE *g_drives[];
extern char *g_driveNames[];

void InitDrives();
void UnInitDrives();

tMacErr vSonyTransfer(bool isWrite, uint8_t *buffer, DriveIndex driveNo, uint32_t sonyStart,
					  uint32_t sonyCount, uint32_t *sonyActCount);
tMacErr vSonyGetSize(DriveIndex driveNo, uint32_t *sonyCount);
tMacErr vSonyEject(DriveIndex driveNo);
tMacErr vSonyEjectDelete(DriveIndex driveNo);
tMacErr vSonyGetName(DriveIndex driveNo, PbufIndex *r);

bool Sony_Insert0(FILE *refnum, bool locked, char *drivepath);
bool Sony_Insert1(char *drivepath, bool silentfail);
void MakeNewDisk(uint32_t L, char *drivename);
