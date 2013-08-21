/*
** Copyright 2002-2009 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<pwd.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

#include	"auth.h"



int auth_getoptionenvint(const char *keyword)
{
	char *p = auth_getoptionenv(keyword);
	int i;

	if (!p) return 0;

	i = atoi(p);

	if (i == 0 && strchr("tTyY", *p))
		i=1;   /* Convert 'true', 'TRUE', 'yes', 'YES' to 1 */
	free(p);
	return i;
}

char *auth_getoptionenv(const char *keyword)
{
	return auth_getoption(getenv("OPTIONS"), keyword);
}

char *auth_getoption(const char *options, const char *keyword)
{
	size_t keyword_l=strlen(keyword);
	char *p;

	while (options)
	{
		if (strncmp(options, keyword, keyword_l) == 0)
		{
			if (options[keyword_l] == 0 ||
			    options[keyword_l] == ',')
				return strdup("");

			if (options[keyword_l] == '=')
			{
				options += keyword_l;
				++options;

				for (keyword_l=0;
				     options[keyword_l] &&
					     options[keyword_l] != ',';
				     ++keyword_l)
					;

				if (!(p=malloc(keyword_l+1)))
					return NULL;
				memcpy(p, options, keyword_l);
				p[keyword_l]=0;
				return p;
			}
		}

		options=strchr(options, ',');
		if (options)
			++options;
	}
	errno=ENOENT;
	return NULL;
}
