#ifndef DBGLOG_PLATFORM_H
#define DBGLOG_PLATFORM_H

#include <cstdint>

#include "platform/common/osglu_ui.h"


bool dbglog_open0();
void dbglog_write0(char *s, uint32_t L);
void dbglog_close0();


#endif /* DBGLOG_PLATFORM_H */
