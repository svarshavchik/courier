
/*
** Copyright 1998 - 2013 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	"courierauth.h"
#include	"courierauthsasl.h"
#include	"authsaslclient.h"
#include	<stdlib.h>
#include	<ctype.h>
#include	<string.h>
#include	<errno.h>

/* Use the SASL_LIST macro to build authsasl_list */

#define NO_SERVER_FUNC()

#define	SERVER_FUNC(b) int b(const char *, const char *,		\
				 char *(*)(const char *, void *),	\
				 void *,				\
				 char **,				\
				 char **);

#define SASL(a,b,c) b
SASL_LIST

#undef	SASL

#undef  SERVER_FUNC
#define SERVER_FUNC(n) n

#undef  NO_SERVER_FUNC
#define NO_SERVER_FUNC() 0

#define	SASL(a,b,c) {a, b},

struct authsasl_info authsasl_list[] = {

SASL_LIST

	{ 0, 0}};

int auth_sasl(const char *method,
	      const char *initreply,
	      char *(*callback_func)(const char *, void *),
	      void *callback_arg,
	      char **authtype_ptr,		/* Returned - AUTHTYPE */
	      char **authdata_ptr)
{
int	i;
char	*p, *q;

	if ((p=malloc(strlen(method)+1)) == 0)
		return (0);
	strcpy(p, method);
	for (q=p; *q; q++)
		*q=toupper((int)(unsigned char)*q);

	for (i=0; authsasl_list[i].sasl_method; i++)
	{
		if (strcmp(p, authsasl_list[i].sasl_method) == 0 &&
		    authsasl_list[i].sasl_func)
		{
			free(p);
			return ( (*authsasl_list[i].sasl_func)
				 (method,
				  initreply, callback_func,
				  callback_arg,
				  authtype_ptr, authdata_ptr));
		}
	}
	free(p);
	errno=ENOENT;
	return (AUTHSASL_ERROR);
}

int auth_sasl_ex(const char *method,
		 const char *initresponse,
		 const char *externalauth,
		 char *(*callback_func)(const char *, void *),
		 void *callback_arg,
		 char **authtype_ptr,		/* Returned - AUTHTYPE */
		 char **authdata_ptr)
{
	char	*uid;
	int n;

	if (strcmp(method, "EXTERNAL"))
		return auth_sasl(method, initresponse, callback_func,
				 callback_arg,
				 authtype_ptr,
				 authdata_ptr);

	if (!externalauth || !*externalauth)
		return AUTHSASL_ERROR;

	if (initresponse && !*initresponse)
		initresponse=NULL;

	if (initresponse)
	{
		uid=strdup(initresponse);

		if (!uid)
			return AUTHSASL_ERROR;

		n=authsasl_frombase64(uid);

		if (n < 0)
		{
			free(uid);
			return AUTHSASL_ABORTED;
		}
		uid[n]=0;

		if (strcmp(uid, externalauth))
		{
			free(uid);
			return AUTHSASL_ERROR;
		}
		free(uid);
	}
	else
	{
		uid=callback_func("", callback_arg);

		if (*uid == '*')
		{
			free(uid);
			return (AUTHSASL_ABORTED);
		}

		n=authsasl_frombase64(uid);

		if (n < 0)
		{
			free(uid);
			return AUTHSASL_ABORTED;
		}
		uid[n]=0;

		if (uid[0])
		{
			free(uid);
			return AUTHSASL_ABORTED;
		}
		free(uid);
	}

	if ((*authtype_ptr=strdup("EXTERNAL")) == NULL)
		return AUTHSASL_ABORTED;

	if ((*authdata_ptr=strdup(externalauth)) == NULL)
	{
		free(authtype_ptr);
		return AUTHSASL_ABORTED;
	}

	return 0;
}

char *auth_sasl_extract_userid(const char *authtype,
			       const char *authdata)
{
	struct	cram_callback_info	cci;
	char *p;
	char *t;
	char *d;

	if (strcmp(authtype, AUTHTYPE_LOGIN) == 0)
	{
		char *q;

		p=strdup(authdata);

		if (!p)
			return NULL;

		q=strchr(p, '\n');
		if (q) *q=0;
		return p;
	}

	if ((t=strdup(authtype)) == NULL)
		return NULL;

	if ((d=strdup(authdata)) == NULL)
	{
		free(t);
		return NULL;
	}

	if (auth_get_cram_silent(t, d, &cci))
	{
		free(d);
		free(t);
		return (NULL);
	}

	p=strdup(cci.user);
	free(d);
	free(t);
	return p;
}
