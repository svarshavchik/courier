
/*
** Copyright 1998 - 2006 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
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

int authsasl_cram(const char *method, const char *initresponse,
		  char *(*getresp)(const char *, void *),
		  void *callback_arg,
		  char **authtype,
		  char **authdata)
{
const	char *randtoken;
char	hostnamebuf[256];
char	*challenge;
char	*challenge_base64;
char	*response;
char	*chrsp;
char	*q, *r, *s, *t;
int	plen;

	if (initresponse && *initresponse)
	{
		if (write(2, "authsasl_cram: invalid request.\n", 32) < 0)
			; /* ignore gcc warning */
		return (AUTHSASL_ERROR);
	}

	randtoken=random128();
	hostnamebuf[0]=0;
	if (gethostname(hostnamebuf, sizeof(hostnamebuf)-1))
		strcpy(hostnamebuf, "cram");

	challenge=malloc(strlen(randtoken)+strlen(hostnamebuf)
			+sizeof("<@>"));
	if (!challenge)
	{
		perror("malloc");
		return (AUTHSASL_ERROR);
	}
	strcat(strcat(strcat(strcat(strcpy(challenge, "<"),
		randtoken), "@"), hostnamebuf), ">");

	challenge_base64=authsasl_tobase64(challenge, -1);
	free(challenge);
	if (!challenge_base64)
	{
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	response=getresp(challenge_base64, callback_arg);
	if (!response)
	{
		free(challenge_base64);
		return (AUTHSASL_ERROR);
	}

	if (*response == '*')
	{
		free(challenge_base64);
		free(response);
		return (AUTHSASL_ABORTED);
	}

	/* If DEFDOMAIN is set, pick apart the response and reassemble
	 * it, potentially with a default domain appended to the username */
	q=getenv("DEFDOMAIN");
	if (q && q[0])
	{
		r = 0;
		if (	(plen = authsasl_frombase64(response)) > 0 &&
			(response[plen]=0, (s = strchr(response, ' ')) != 0) &&
			(*s++ = 0, (t = strdupdefdomain(response, " ", s, "")) != 0) )
		{
			r = authsasl_tobase64(t, -1);
			free(t);
		}
		free(response);
		if ((response = r) == 0)
		{
			free(challenge_base64);
			return (AUTHSASL_ERROR);
		}
	}

	chrsp=malloc(strlen(challenge_base64)+strlen(response)+3);
	if (!chrsp)    
	{
		free(challenge_base64);
		free(response);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	strcat(strcat(strcat(strcpy(chrsp, challenge_base64), "\n"),
		response), "\n");
	free(challenge_base64);
	free(response);

	if ( (*authtype=malloc(strlen(method)+1)) == 0)
	{
		free(chrsp);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}
	strcpy( *authtype, method );
	*authdata=chrsp;

	for (chrsp= *authtype; *chrsp; chrsp++)
		*chrsp= tolower( (int)(unsigned char)*chrsp );

	return (AUTHSASL_OK);
}
