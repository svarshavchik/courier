/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>

static const char *helohost;

const char *config_esmtphelo()
{
	if (!helohost)
	{
	char	*f=config_localfilename("esmtphelo");

		if ((helohost=config_read1l(f)) == 0)
			helohost=config_me();
		free(f);
	}
	return (helohost);
}
