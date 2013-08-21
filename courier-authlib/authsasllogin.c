
/*
** Copyright 1998 - 2005 Double Precision, Inc.  See COPYING for
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
                      
int authsasl_login(const char *method, const char *initresponse,
		   char *(*getresp)(const char *, void *),
		   void *callback_arg,
		   char **authtype,
		   char **authdata)
{
char	*uid;
char	*pw;
char	*p;
int	n;

	if (initresponse)
	{
		uid=malloc(strlen(initresponse)+1);
		if (!uid)
		{
			perror("malloc");
			return (AUTHSASL_ERROR);
		}
		strcpy(uid, initresponse);
	}
	else
	{
		p=authsasl_tobase64("Username:", -1);
		if (!p)
		{
			perror("malloc");
			return (AUTHSASL_ERROR);
		}
		uid=getresp(p, callback_arg);
		free(p);
		if (!uid)
		{
			perror("malloc");
			return (AUTHSASL_ERROR);
		}

		if (*uid == '*')
		{
			free(uid);
			return (AUTHSASL_ABORTED);
		}
	}

	p=authsasl_tobase64("Password:", -1);
	if (!p)
	{
		free(uid);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	pw=getresp(p, callback_arg);
	free(p);
	if (!pw)
	{
		free(uid);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	if (*pw == '*')
	{
		free(pw);
		free(uid);
		return (AUTHSASL_ABORTED);
	}

	if ((n=authsasl_frombase64(uid)) < 0 ||
		(uid[n]=0, n=authsasl_frombase64(pw)) < 0)
	{
		free(uid);
		free(pw);
		return (AUTHSASL_ABORTED);
	}
	pw[n]=0;

	if ( (*authtype=malloc(sizeof(AUTHTYPE_LOGIN))) == 0)
	{
		free(uid);
		free(pw);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	strcpy( *authtype, AUTHTYPE_LOGIN);

	if ( (*authdata=strdupdefdomain(uid,"\n",pw,"\n")) == 0)
	{
		free( *authtype );
		free(uid);
		free(pw);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	free(uid);
	free(pw);

	return (AUTHSASL_OK);
}
