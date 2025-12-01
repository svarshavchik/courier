/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<ctype.h>
#include	<stdlib.h>
#include	<string.h>
#include	<courier-unicode.h>
#include	<idn2.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

/* Convert address to consistent lowercase */

char *udomainlower(const char *c)
{
	const char *d;
	char *ud;
	char *s;

	d=strrchr(c, '@');

	if (!d)
		return courier_strdup(c);

	ud=unicode_convert_tocase(++d, "utf-8", unicode_lc, unicode_lc);

	if (!ud)
		return courier_strdup(c);

	s=(char *)courier_malloc(strlen(ud)+1+(d-c));

	memcpy(s, c, d-c);
	strcpy(s+(d-c), ud);
	free(ud);
	return s;
}

static char *udomainutf82(const char *c)
{
	const char *d;
	char *ud;
	char *s;

	d=strrchr(c, '@');

	if (!d)
		return courier_strdup(c);

	++d;
	s=0;
	if (idna_to_unicode_8z8z(d, &s, 0) != IDNA_SUCCESS)
	{
		if (s)
			free(s);
		return courier_strdup(c);
	}

	ud=(char *)courier_malloc((d-c+1)+strlen(s));

	memcpy(ud, c, d-c);
	strcpy(ud+(d-c), s);
	free(s);
	return ud;
}

char *udomainutf8(const char *c)
{
	char *p=udomainutf82(c);
	char *q=udomainlower(p);
	free(p);
	return q;
}

static char *udomainace2(const char *c)
{
	const char *d;
	char *ud;
	char *s=0;

	d=strrchr(c, '@');

	if (!d)
		return courier_strdup(c);

	++d;
	if (idna_to_ascii_8z(d, &s, 0) != IDNA_SUCCESS)
	{
		if (s)
			free(s);
		return courier_strdup(c);
	}

	ud=(char *)courier_malloc((d-c+1)+strlen(s));

	memcpy(ud, c, d-c);
	strcpy(ud+(d-c), s);
	free(s);
	return ud;
}

char *udomainace(const char *c)
{
	char *p=udomainace2(c);
	char *q=udomainlower(p);
	free(p);
	return q;
}

char *ulocallower(const char *c)
{
	static int islocallower=0;

	if (islocallower == 0)
	{
		char	*s=config_localfilename("locallowercase");

		islocallower = access(s, 0) == 0 ? 1:-1;
		free(s);
	}

	if ( islocallower > 0 || (
		strncasecmp(c, "postmaster", 10)
		== 0 && (c[10] == '\0' || c[10] == '@')))
	{
		char *c_copy=courier_strdup(c);
		char *at=strrchr(c_copy, '@');
		char save;
		char *lower_username;

		if (!at)
			at=c_copy+strlen(c_copy);

		save=*at;
		*at=0;

		lower_username=	unicode_convert_tocase(c_copy, "utf-8",
						       unicode_lc, unicode_lc);
		*at=save;

		if (lower_username)
		{
			char *buf=(char *)courier_malloc(strlen(lower_username)+
							 strlen(at)+1);

			strcat(strcpy(buf, lower_username), at);
			free(c_copy);
			free(lower_username);
			return buf;
		}
		free(c_copy);
	}

	return courier_strdup(c);
}

char *ualllower(const char *c)
{
	char *u=unicode_convert_tocase(c, "utf-8", unicode_lc, unicode_lc);

	if (u)
		return u;
	return courier_strdup(c);
}
