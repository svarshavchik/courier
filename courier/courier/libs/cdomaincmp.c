/*
** Copyright 1998 - 2018 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	<string.h>
#include	<stdlib.h>
#include	<idn2.h>

/* Compare domains */

static int config_domaincmp_utf8(const char *address, const char *domain)
{
	size_t domainl=strlen(domain);
	size_t l;

	if (!domainl)	return (1);
	l=strlen(address);

	if (*domain == '.')	/* Subdomain wildcard */
	{
		if (l >= domainl)
			return strncmp(address+(l-domainl), domain, domainl);

		return (1);
	}

	if (l != domainl)	return (1);

	return (strncmp(address, domain, domainl));
}

int config_domaincmp(const char *lookup_address,
		     const char *domain, unsigned domainl)
{
	char *lookup_domain=courier_malloc(domainl+1);
	char *lookup_domain_utf8;
	int rc;

	char *lookup_address_utf8;
	char *p;

	memcpy(lookup_domain, domain, domainl);
	lookup_domain[domainl]=0;

	if (idna_to_unicode_8z8z(lookup_domain, &lookup_domain_utf8, 0)
	    == IDNA_SUCCESS)
	{
		free(lookup_domain);
		lookup_domain=lookup_domain_utf8;
	}

	lookup_domain_utf8=ualllower(lookup_domain);
	free(lookup_domain);

	if (idna_to_unicode_8z8z(lookup_address, &lookup_address_utf8, 0)
	    != IDNA_SUCCESS)
		lookup_address_utf8=courier_strdup(lookup_address);
	p=ualllower(lookup_address_utf8);
	free(lookup_address_utf8);

	rc=config_domaincmp_utf8(p, lookup_domain_utf8);
	free(p);
	free(lookup_domain_utf8);
	return rc;
}
