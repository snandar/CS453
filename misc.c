
#include <ctype.h>
#include "misc.h"

int split(char *s, char *argv[], int maxargs)
{
	int i = 0;

	while (*s && i < maxargs)
	{
		while (*s && isspace(*s))
			*s++ = 0;

		if (!*s)
			break;

		argv[i++] = s;

		while (*s && !isspace(*s))
			s++;
	}

	return i;
}

