#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <cstdint>
#include <filesystem>
#include <string>
#include "platform/platform.h"

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

/* Portable UTF-8 c-string from fs::path (path::c_str() returns wchar_t* on Windows). */
inline std::string path_str(const std::filesystem::path &p)
{
	return p.string();
}

tMacErr ChildPath(char *x, char *y, char **r);

#endif /* PATH_UTILS_H */
