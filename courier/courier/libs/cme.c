/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>

static const char *me;

const char *config_me()
{
	if (!me)
	{
	char	*f=config_localfilename("me");

		if ((me=config_read1l(f)) == 0)
			me=config_gethostname();
		free(f);
	}
	return (me);
}
