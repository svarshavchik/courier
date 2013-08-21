/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdio.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

static char *maildrop;

const char *config_maildropmda()
{
	if (!maildrop)
	{
	char	*filename=config_localfilename("maildrop");

		maildrop=config_read1l(filename);
		free(filename);

		if (!maildrop)
			maildrop=MAILDROP;
		if (access(maildrop, 0))
			maildrop="";
	}
	return (*maildrop ? maildrop:0);
}
