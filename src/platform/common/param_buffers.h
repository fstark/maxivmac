/*
	Parameter BUFfers — declarations
*/

#pragma once

#include "platform/common/osglu_ui.h"


extern void *PbufDat[NumPbufs];

#define PbufHaveLock 1

tMacErr PbufNewFromPtr(void *p, uint32_t count, tPbuf *r);
void PbufKillToPtr(void **p, uint32_t *count, tPbuf r);
tMacErr PbufNew(uint32_t count, tPbuf *r);
void PbufDispose(tPbuf i);
void UnInitPbufs();
uint8_t * PbufLock(tPbuf i);
#define PbufUnlock(i)
void PbufTransfer(uint8_t * Buffer,
	tPbuf i, uint32_t offset, uint32_t count, bool IsWrite);

