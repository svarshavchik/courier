
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

int authsaslclient_plain(const struct authsaslclientinfo *info)
{
char *q, *r;
int	i;
const char *userid, *password;
size_t userid_l, password_l;

	userid=info->userid ? info->userid:"";
	password=info->password ? info->password:"";

	userid_l=strlen(userid);
	password_l=strlen(password);

	q=malloc(userid_l+password_l+2);

	if (!q)
	{
		perror("malloc");
		return (AUTHSASL_ERROR);
	}
	q[0]=0;
	strcpy(q+1, userid);
	memcpy(q+2+userid_l, password, password_l);

	r=authsasl_tobase64(q, userid_l+password_l+2);
	free(q);

	if (!r)
	{
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	i=(*info->plain_conv_func)("PLAIN", r, info->conv_func_arg);
	free(r);
	return (i);
}
