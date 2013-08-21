
/*
** Copyright 2000 Double Precision, Inc.  See COPYING for
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

int authsaslclient_login(const struct authsaslclientinfo *info)
{
const char *p;
char *q;
int	i;

	p=(*info->start_conv_func)("LOGIN", NULL, info->conv_func_arg);
	if (!p)	return (AUTHSASL_CANCELLED);

	q=authsasl_tobase64( info->userid ? info->userid:"", -1);

	if (!q)
	{
		perror("malloc");
		return (AUTHSASL_ERROR);
	}
	p=(*info->conv_func)(q, info->conv_func_arg);
	free(q);
	if (!p)	return (AUTHSASL_CANCELLED);

	q=authsasl_tobase64( info->password ? info->password:"", -1);

	if (!q)
	{
		perror("malloc");
		return (AUTHSASL_ERROR);
	}
	i=(*info->final_conv_func)(q, info->conv_func_arg);
	free(q);
	return (i);
}
