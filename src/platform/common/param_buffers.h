/*
	Parameter BUFfers — declarations
*/

#pragma once

#include "platform/common/osglu_ui.h"


extern void *g_pbufDat[NumPbufs];

tMacErr PbufNewFromPtr(void *p, uint32_t count, PbufIndex *r);
void PbufKillToPtr(void **p, uint32_t *count, PbufIndex r);
tMacErr PbufNew(uint32_t count, PbufIndex *r);
void PbufDispose(PbufIndex i);
void UnInitPbufs();
uint8_t *PbufLock(PbufIndex i);
#define PBUF_UNLOCK(i)
void PbufTransfer(uint8_t *buffer, PbufIndex i, uint32_t offset, uint32_t count, bool isWrite);
