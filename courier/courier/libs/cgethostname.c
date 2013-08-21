/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_lib_config.h"
#endif
#include	"courier.h"
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#if     HAS_GETHOSTNAME
#else
int gethostname(const char *, size_t);
#endif

static char *hostname_buf=0;

const char *config_gethostname()
{
char	buf[BUFSIZ];

	if (!hostname_buf)
	{
		buf[sizeof(buf)-1]=0;
		buf[0]=0;
		gethostname(buf, sizeof(buf)-1);
		if (buf[0])
		{
			hostname_buf=courier_malloc(strlen(buf)+1);
			strcpy(hostname_buf, buf);
		}
	}
	return (hostname_buf ? hostname_buf:"");
}
		
