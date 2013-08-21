
/*
** Copyright 2000-2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	"courierauthsasl.h"
#include	"authsaslclient.h"
#include	<stdlib.h>
#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<errno.h>

/* Use the SASL_LIST macro to build authsaslclientlist */

#define	SASL(a,b,c) {a, c},

struct authsaslclientlist_info authsaslclientlist[] = {

SASL_LIST

	{ 0, 0}};

int auth_sasl_client(const struct authsaslclientinfo *info)
{
char *methodbuf;
int	i;

	if (!info->sasl_funcs
	    ||!info->conv_func
	    ||!info->start_conv_func
	    || !info->plain_conv_func)	return (AUTHSASL_NOMETHODS);

	if ((methodbuf=malloc(strlen(info->sasl_funcs)+1)) == 0)
	{
		perror("malloc");
		return (AUTHSASL_NOMETHODS);
	}

	for (i=0; authsaslclientlist[i].name; i++)
	{
	char	*p;

		strcpy(methodbuf, info->sasl_funcs);
		for (p=methodbuf; *p; p++)
			*p=toupper((int)(unsigned char)*p);
		for (p=methodbuf; (p=strtok(p, " \t\r\n")) != 0; p=0)
			if (strcmp(p, authsaslclientlist[i].name) == 0)
			{
				free(methodbuf);
				return ( (*authsaslclientlist[i].func)(info));
			}
	}
	free(methodbuf);
	return (AUTHSASL_NOMETHODS);
}
