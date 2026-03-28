#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <cstdint>
#include "platform/platform.h"

#ifdef _WIN32
#define MyPathSep '\\'
#else
#define MyPathSep '/'
#endif

tMacErr ChildPath(char *x, char *y, char **r);

#endif /* PATH_UTILS_H */
