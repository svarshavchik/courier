/*
** Copyright 1998 - 2018 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<idna.h>

static const char *me;
static const char *me_ace;

const char *config_me()
{
	if (!me)
	{
		char *ace;
		char	*f=config_localfilename("me");

		if ((me=config_read1l(f)) == 0)
			me=config_gethostname();
		free(f);


		if (idna_to_ascii_8z(me, &ace, 0) == IDNA_SUCCESS)
			me_ace=ace;
		else
			me_ace=me;
	}
	return (me);
}

const char *config_me_ace()
{
	(void)config_me();
	return me_ace;
}
