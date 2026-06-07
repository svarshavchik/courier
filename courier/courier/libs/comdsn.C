/*
** Copyright 1998 - 2018 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rfc822/rfc822.h"
#include	"rfc822/rfc2047.h"
#include	"rfc2045/rfc2045charset.h"
#include	<stdlib.h>
#include	<string.h>

char *config_dsnfrom()
{
char	*f=config_localfilename("dsnfrom");
char	*p=config_read1l(f);
static const char defaultdsnfrom[]="Courier mail server at ~ <@>";

	free(f);
	if (!p)
	{
		const char *me=config_me();

		auto s=rfc2047::encode(
			me,
			RFC2045CHARSET,
			rfc2047_qp_allow_word
		).first;

		std::string tt=defaultdsnfrom;

		size_t pos=tt.find('~');

		if (pos != tt.npos)
		{
			tt=tt.substr(0, pos)+s+tt.substr(pos+1);
		}
		p=(char *)courier_strdup(tt.c_str());
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
