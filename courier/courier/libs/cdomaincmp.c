/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier.h"
#endif
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<string.h>
#include	<stdlib.h>

/* Compare domains */

int config_domaincmp(const char *address, const char *domain, unsigned domainl)
{
unsigned l;

	if (!domainl)	return (1);
	l=strlen(address);

	if (*domain == '.')	/* Subdomain wildcard */
	{
		if (l >= domainl)
			return (
#if	HAVE_STRNCASECMP
				strncasecmp(address+(l-domainl), domain,
					domainl)
#else
				strnicmp(address+(l-domainl), domain,
					domainl)
#endif
				);
		return (1);
	}

	if (l != domainl)	return (1);

	return (
#if	HAVE_STRNCASECMP
			strncasecmp(address, domain, domainl)
#else
			strnicmp(address, domain, domainl)
#endif
		);
}
