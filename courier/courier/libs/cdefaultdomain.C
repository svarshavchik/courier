/*
** Copyright 1998 - 2018 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<idn2.h>
#include	<string>

static std::string init_defaultdomain()
{
	char	*f=config_localfilename("defaultdomain");
	const char  *defaultdomain;

	if ((defaultdomain=config_read1l(f)) == 0)
		defaultdomain=config_me();
	free(f);
	return defaultdomain;
}

const char *config_defaultdomain()
{
	static std::string defaultdomain{init_defaultdomain()};

	return defaultdomain.c_str();
}

static std::string init_defaultdomain_ace()
{
	char	*p=0;
	const char *default_domain=config_defaultdomain();
	std::string defaultdomain_ace;

	if (idna_to_ascii_8z(default_domain, &p, 0) == IDNA_SUCCESS)
		defaultdomain_ace=p;
	else
	{
		defaultdomain_ace=default_domain;
	}

	if (p)
		free(p);
	return defaultdomain_ace;
}

const char *config_defaultdomain_ace()
{
	static std::string defaultdomain_ace=init_defaultdomain_ace();

	return defaultdomain_ace.c_str();
}
