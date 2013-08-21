/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>

char *config_dsnfrom()
{
char	*f=config_localfilename("dsnfrom");
char	*p=config_read1l(f);
static const char defaultdsnfrom[]="\"Courier mail server at %s\" <@>";

	free(f);
	if (!p)
	{
	const char *me=config_me();

		p=courier_malloc(sizeof(defaultdsnfrom)+strlen(me));
		sprintf(p, defaultdsnfrom, me);
	}
	return (p);
}

size_t config_dsnlimit()
{
char	*f=config_localfilename("dsnlimit");
char	*p=config_read1l(f);
size_t	l=32768;

	if (p)
	{
		l=atol(p);
		free(p);
	}
	return (l);
}
