/*
** Copyright 2009 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

#include	"courierauth.h"


static void escape_specials(const char *str,
			    char *bufptr,
			    size_t *sizeptr)
{
	static const char specials[]="*()\\";

	while (*str)
	{
		char buf[10];
		char *p;

		if (strchr(specials, *str))
		{
			sprintf(buf, "\\%02x", (int)(unsigned char)*str);
		}
		else
		{
			buf[0]=*str;
			buf[1]=0;
		}

		for (p=buf; *p; p++)
		{
			if (bufptr)
				*bufptr++=*p;
			if (sizeptr)
				++*sizeptr;
		}
		++str;
	}

	if (bufptr)
		*bufptr=0;
}

char *courier_auth_ldap_escape(const char *str)
{
	char *escaped;
	size_t escaped_cnt=1;

	escape_specials(str, NULL, &escaped_cnt);
	escaped=malloc(escaped_cnt);
	if (!escaped)
		return NULL;

	escape_specials(str, escaped, NULL);

	return escaped;
}
