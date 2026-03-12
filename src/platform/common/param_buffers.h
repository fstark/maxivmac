/*
	param_buffers.h

	Copyright (C) 2018 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	Parameter BUFfers — declarations
*/

#pragma once

#include "platform/common/osglu_ui.h"

#if IncludePbufs

extern void *PbufDat[NumPbufs];

#define PbufHaveLock 1

tMacErr PbufNewFromPtr(void *p, uint32_t count, tPbuf *r);
void PbufKillToPtr(void **p, uint32_t *count, tPbuf r);
tMacErr PbufNew(uint32_t count, tPbuf *r);
void PbufDispose(tPbuf i);
void UnInitPbufs(void);
uint8_t * PbufLock(tPbuf i);
#define PbufUnlock(i)
void PbufTransfer(uint8_t * Buffer,
	tPbuf i, uint32_t offset, uint32_t count, bool IsWrite);

#endif
