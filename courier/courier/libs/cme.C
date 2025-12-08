/*
** Copyright 1998 - 2025 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>
#include	<idn2.h>
#include	<string>


std::string config_me_init()
{
	char *me;
	char	*f=config_localfilename("me");

	if ((me=config_read1l(f)) == 0)
	{
		free(f);
		return config_gethostname();
	}
	free(f);

	std::string ret{me};
	free(me);

	if (strncmp(ret.c_str(), "*.", 2) == 0)
	{
		ret=config_gethostname() + ret.substr(1);
	}
	return ret;
}

const char *config_me()
{
	static std::string me=config_me_init();

	return me.c_str();
}

static std::string config_me_ace_init()
{
	auto me=config_me();
	char *ace=0;

	std::string me_ace;

	if (idna_to_ascii_8z(me, &ace, 0) == IDNA_SUCCESS)
	{
		me_ace=ace;
		free(ace);
	}
	else
	{
		if (ace)
			free(ace);
		me_ace=me;
	}
	return me_ace;
}

const char *config_me_ace()
{
	static std::string me_ace=config_me_ace_init();

	return me_ace.c_str();
}
