/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>
#include	<idn2.h>

static const char *esmtpidstring;

const char *config_esmtpgreeting()
{
	if (!esmtpidstring)
	{
	char	*f=config_localfilename("esmtpgreeting");

		if ((esmtpidstring=readfile(f, 0)) == 0)
		{
			const char *me=config_me();
			char *me_ace;

			if (idna_to_ascii_8z(me, &me_ace, 0) == IDNA_SUCCESS)
			{
				me=me_ace;
			}
			else
			{
				me_ace=0;
			}

			esmtpidstring=strcat(strcpy(courier_malloc(
				strlen(me)+sizeof(" ESMTP")), me), " ESMTP");

			if (me_ace)
				free(me_ace);
		}
		free(f);
	}
	return (esmtpidstring);
}
