/*
** Copyright 1998 - 2018 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<idna.h>

static const char *defaultdomain;
static const char *defaultdomain_ace;

const char *config_defaultdomain()
{
	if (!defaultdomain)
	{
		char	*f=config_localfilename("defaultdomain");
		char	*p;

		if ((defaultdomain=config_read1l(f)) == 0)
			defaultdomain=config_me();
		free(f);

		if (idna_to_ascii_8z(defaultdomain, &p, 0) == IDNA_SUCCESS)
			defaultdomain_ace=p;
		else
			defaultdomain_ace=defaultdomain;

	}
	return (defaultdomain);
}

const char *config_defaultdomain_ace()
{
	(void)config_defaultdomain();
	return defaultdomain_ace;
}
