/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"maxlongsize.h"
#include	<stdio.h>
#include	<stdlib.h>


static char *sizelimit_ptr=0;

unsigned long config_sizelimit()
{
char	*sizelimitfilename;

	if (sizelimit_ptr == 0)
	{
		sizelimit_ptr=getenv("SIZELIMIT");
		if (sizelimit_ptr == 0)
		{
			sizelimitfilename=config_localfilename("sizelimit");
			sizelimit_ptr=config_read1l(sizelimitfilename);
			free(sizelimitfilename);
			if (sizelimit_ptr == 0)
				sizelimit_ptr="10485760";
		}
	}
	return (atol(sizelimit_ptr));
}
