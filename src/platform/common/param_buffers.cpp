/*
	Parameter BUFfers implemented with STanDard C library
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/param_buffers.h"

#include <cstdlib>
#include <cstring>

void *PbufDat[NumPbufs];

tMacErr PbufNewFromPtr(void *p, uint32_t count, tPbuf *r)
{
	tPbuf i;
	tMacErr err;

	if (! FirstFreePbuf(&i)) {
		free(p);
		err = mnvm_miscErr;
	} else {
		*r = i;
		PbufDat[i] = p;
		PbufNewNotify(i, count);
		err = mnvm_noErr;
	}

	return err;
}

void PbufKillToPtr(void **p, uint32_t *count, tPbuf r)
{
	*p = PbufDat[r];
	*count = PbufSize[r];

	PbufDisposeNotify(r);
}

tMacErr PbufNew(uint32_t count, tPbuf *r)
{
	tMacErr err = mnvm_miscErr;

	void *p = calloc(1, count);
	if (nullptr != p) {
		err = PbufNewFromPtr(p, count, r);
	}

	return err;
}

void PbufDispose(tPbuf i)
{
	void *p;
	uint32_t count;

	PbufKillToPtr(&p, &count, i);

	free(p);
}

void UnInitPbufs()
{
	tPbuf i;

	for (i = 0; i < NumPbufs; ++i) {
		if (PbufIsAllocated(i)) {
			PbufDispose(i);
		}
	}
}

uint8_t * PbufLock(tPbuf i)
{
	return (uint8_t *)PbufDat[i];
}

void PbufTransfer(uint8_t * Buffer,
	tPbuf i, uint32_t offset, uint32_t count, bool IsWrite)
{
	void *p = ((uint8_t *)PbufDat[i]) + offset;
	if (IsWrite) {
		(void) memcpy(p, Buffer, count);
	} else {
		(void) memcpy(Buffer, p, count);
	}
}
