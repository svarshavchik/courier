/*
** Copyright 1998 - 2018 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rfc822/rfc2047.h"
#include	"rfc2045/rfc2045charset.h"
#include	<stdlib.h>
#include	<string.h>

char *config_dsnfrom()
{
char	*f=config_localfilename("dsnfrom");
char	*p=config_read1l(f);
static const char defaultdsnfrom[]="Courier mail server at %s <@>";

	free(f);
	if (!p)
	{
		const char *me=config_me();
		char *me_encoded=rfc2047_encode_str(me,
						    RFC2045CHARSET,
						    rfc2047_qp_allow_word);

		if (me_encoded)
			me=me_encoded;
		p=courier_malloc(sizeof(defaultdsnfrom)+strlen(me));
		sprintf(p, defaultdsnfrom, me);
		if (me_encoded)
			free(me_encoded);
	}
	return (p);
}

size_t config_dsnlimit()
{
char	*f=config_localfilename("dsnlimit");
char	*p=config_read1l(f);
size_t	l=32768;

	if (p)
	{
		l=atol(p);
		free(p);
	}
	return (l);
}
