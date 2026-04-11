#include "path_utils.h"

#include <cstdlib>
#include <cstring>

// Build a full path from directory x and filename y, adding separator.
tMacErr ChildPath(char *x, char *y, char **r)
{
	tMacErr err = tMacErr::miscErr;
	int nx = strlen(x);
	int ny = strlen(y);
	{
		if ((nx > 0) && (MyPathSep == x[nx - 1]))
		{
			--nx;
		}
		{
			int nr = nx + 1 + ny;
			char *p = static_cast<char *>(malloc(nr + 1));
			if (p != nullptr)
			{
				char *p2 = p;
				(void)memcpy(p2, x, nx);
				p2 += nx;
				*p2++ = MyPathSep;
				(void)memcpy(p2, y, ny);
				p2 += ny;
				*p2 = 0;
				*r = p;
				err = tMacErr::noErr;
			}
		}
	}

	return err;
}
