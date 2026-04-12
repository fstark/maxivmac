/*
	Parameter BUFfers implemented with STanDard C library
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/param_buffers.h"

#include <cstdlib>
#include <cstring>

void *g_pbufDat[NumPbufs];

/* Allocate a Pbuf descriptor for an existing memory pointer. */
tMacErr PbufNewFromPtr(void *p, uint32_t count, PbufIndex *r)
{
	PbufIndex i;
	tMacErr err;

	if (!FirstFreePbuf(&i))
	{
		free(p);
		err = tMacErr::miscErr;
	}
	else
	{
		*r = i;
		g_pbufDat[i] = p;
		PbufNewNotify(i, count);
		err = tMacErr::noErr;
	}

	return err;
}

void PbufKillToPtr(void **p, uint32_t *count, PbufIndex r)
{
	*p = g_pbufDat[r];
	*count = g_pbufSize[r];

	PbufDisposeNotify(r);
}

tMacErr PbufNew(uint32_t count, PbufIndex *r)
{
	tMacErr err = tMacErr::miscErr;

	void *p = calloc(1, count);
	if (nullptr != p)
	{
		err = PbufNewFromPtr(p, count, r);
	}

	return err;
}

void PbufDispose(PbufIndex i)
{
	void *p;
	uint32_t count;

	PbufKillToPtr(&p, &count, i);

	free(p);
}

void UnInitPbufs()
{
	PbufIndex i;

	for (i = 0; i < NumPbufs; ++i)
	{
		if (PbufIsAllocated(i))
		{
			PbufDispose(i);
		}
	}
}

uint8_t *PbufLock(PbufIndex i)
{
	return static_cast<uint8_t *>(g_pbufDat[i]);
}

/* Copy data between a Pbuf and host memory at the given offset. */
void PbufTransfer(uint8_t *buffer, PbufIndex i, uint32_t offset, uint32_t count, bool isWrite)
{
	void *p = static_cast<uint8_t *>(g_pbufDat[i]) + offset;
	if (isWrite)
	{
		(void)memcpy(p, buffer, count);
	}
	else
	{
		(void)memcpy(buffer, p, count);
	}
}
