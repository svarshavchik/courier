/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"libs/courier_lib_config.h"
#endif
#include	"courier.h"
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<idn2.h>

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
			char *address_utf8;

			if (idna_to_unicode_8z8z(buf, &address_utf8, 0)
			    != IDNA_SUCCESS)
				address_utf8=courier_strdup(buf);

			hostname_buf=ualllower(address_utf8);
			free(address_utf8);
		}
	}
	return (hostname_buf ? hostname_buf:"");
}

/*
** If addr is, or start with, hostname, return a '*'.
*/

char *config_is_gethostname(const char *addr)
{
	const char *h=config_gethostname();
	size_t l=strlen(h);

	if (l > 0 && strncmp(addr, h, l) == 0)
	{
		if (addr[l] == '\0')
			return courier_strdup("*");

		if (addr[l] == '.')
		{
			char *p=courier_strdup(addr+l-1);

			if (p)
			{
				*p='*';
				return p;
			}
		}
	}
	return 0;
}
