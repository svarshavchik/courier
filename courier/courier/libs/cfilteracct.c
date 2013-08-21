/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>

static const char *filteracct;

const char *config_filteracct()
{
	if (!filteracct)
	{
	char	*f=config_localfilename("aliasfilteracct");

		if ((filteracct=config_read1l(f)) == 0)
			filteracct="";
		free(f);
	}
	return (filteracct);
}
