/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>

static const char *defaultdomain;

const char *config_defaultdomain()
{
	if (!defaultdomain)
	{
	char	*f=config_localfilename("defaultdomain");

		if ((defaultdomain=config_read1l(f)) == 0)
			defaultdomain=config_me();
		free(f);
	}
	return (defaultdomain);
}
