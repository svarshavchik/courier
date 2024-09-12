/*
** Copyright 1998 - 2018 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>

static const char *cdefaultsep;

const char *config_defaultsep()
{
	if (!cdefaultsep)
	{
		char	*f=config_localfilename("defaultsep");

		if ((cdefaultsep=config_read1l(f)) == 0)
			cdefaultsep="+";
		free(f);
	}
	return cdefaultsep;
}
