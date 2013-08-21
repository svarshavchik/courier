
/*
** Copyright 2000-2005 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	"auth.h"
#include	"random128/random128.h"
#include	"courierauthsasl.h"
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<ctype.h>
#include	<stdio.h>
#include	<errno.h>

extern char *strdupdefdomain(const char *userid, const char *s1,
			     const char *s2, const char *s3);

int authsasl_plain(const char *method, const char *initresponse,
		   char *(*getresp)(const char *, void *),
		   void *callback_arg,
		   char **authtype,
		   char **authdata)
{
char	*uid;
char	*pw;
char	*p;
int	n;
int	i;

	if (initresponse)
	{
		p=malloc(strlen(initresponse)+1);
		if (!p)
		{
			perror("malloc");
			return (AUTHSASL_ERROR);
		}
		strcpy(p, initresponse);
	}
	else
	{
		p=authsasl_tobase64("", -1);
		if (!p)
		{
			perror("malloc");
			return (AUTHSASL_ERROR);
		}
		uid=getresp(p, callback_arg);
		free(p);
		p=uid;
		if (!p)
		{
			perror("malloc");
			return (AUTHSASL_ERROR);
		}

		if (*p == '*')
		{
			free(p);
			return (AUTHSASL_ABORTED);
		}
	}

	if ((n=authsasl_frombase64(p)) < 0)
	{
		free(p);
		return (AUTHSASL_ABORTED);
	}
	p[n]=0;

	uid=pw=0;

	for (i=0; i<n; i++)
	{
		if (p[i] == 0)
		{
			++i;
			for (uid=p+i; i<n; i++)
				if (p[i] == 0)
				{
					pw=p+i+1;
					break;
				}
		}
	}

	if (pw == 0)
	{
		free(p);
		return (AUTHSASL_ABORTED);	/* Bad message */
	}

	if ( (*authtype=malloc(sizeof(AUTHTYPE_LOGIN))) == 0)
	{
		free(p);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	strcpy( *authtype, AUTHTYPE_LOGIN);

	if ( (*authdata=strdupdefdomain(uid, "\n", pw, "\n")) == 0)
	{
		free( *authtype );
		free(p);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	free(p);
	return (AUTHSASL_OK);
}
