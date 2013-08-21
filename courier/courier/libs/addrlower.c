/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<ctype.h>
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

/* Convert address to consistent lowercase */

void domainlower(char *c)
{
	if ((c=strchr(c, '@')) != 0)
	{
		while (*c)
		{
			*c=tolower((int)(unsigned char)*c);
			++c;
		}
	}
}

void locallower(char *c)
{
static int islocallower=0;

	if (islocallower == 0)
	{
	char	*s=config_localfilename("locallowercase");

		islocallower = access(s, 0) == 0 ? 1:-1;
		free(s);
	}

	if ( islocallower > 0 || (
#if     HAVE_STRNCASECMP
		strncasecmp(c, "postmaster", 10)
#else
		strnicmp(c, "postmaster", 10)
#endif
			== 0 && (c[10] == '\0' || c[10] == '@')))
	{
		while (*c)
		{
			*c=tolower((int)(unsigned char)*c);
			if (*c++ == '@')	break;
		}
	}
}
