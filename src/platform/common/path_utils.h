#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <cstdint>
#include "platform/platform.h"

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

tMacErr ChildPath(char *x, char *y, char **r);

#endif /* PATH_UTILS_H */
