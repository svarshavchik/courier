/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>

static const char *esmtpidstring;

const char *config_esmtpgreeting()
{
	if (!esmtpidstring)
	{
	char	*f=config_localfilename("esmtpgreeting");

		if ((esmtpidstring=readfile(f, 0)) == 0)
		{
		const char *me=config_me();

			esmtpidstring=strcat(strcpy(courier_malloc(
				strlen(me)+sizeof(" ESMTP")), me), " ESMTP");
		}
		free(f);
	}
	return (esmtpidstring);
}
