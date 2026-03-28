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

/* Allocate a Pbuf descriptor for an existing memory pointer. */
tMacErr PbufNewFromPtr(void *p, uint32_t count, tPbuf *r)
{
	tPbuf i;
	tMacErr err;

	if (! FirstFreePbuf(&i)) {
		free(p);
		err = tMacErr::miscErr;
	} else {
		*r = i;
		PbufDat[i] = p;
		PbufNewNotify(i, count);
		err = tMacErr::noErr;
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
	tMacErr err = tMacErr::miscErr;

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
	return static_cast<uint8_t *>(PbufDat[i]);
}

/* Copy data between a Pbuf and host memory at the given offset. */
void PbufTransfer(uint8_t * Buffer,
	tPbuf i, uint32_t offset, uint32_t count, bool IsWrite)
{
	void *p = static_cast<uint8_t *>(PbufDat[i]) + offset;
	if (IsWrite) {
		(void) memcpy(p, Buffer, count);
	} else {
		(void) memcpy(Buffer, p, count);
	}
}
