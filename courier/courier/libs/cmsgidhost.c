/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>

static const char *msgidhost;

const char *config_msgidhost()
{
	if (!msgidhost)
	{
	char	*f=config_localfilename("msgidhost");

		if ((msgidhost=config_read1l(f)) == 0)
			msgidhost=config_me();
		free(f);
	}
	return (msgidhost);
}
