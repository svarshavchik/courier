
/*
** Copyright 2000-2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	"courierauthsasl.h"
#include	"libhmac/hmac.h"
#include	"authsaslclient.h"
#include	<stdlib.h>
#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<errno.h>

extern int authsaslclient_cram(const struct authsaslclientinfo *,
				const char *,
				const struct hmac_hashinfo *);

int authsaslclient_cramsha1(const struct authsaslclientinfo *info)
{
const char *p=(*info->start_conv_func)("CRAM-SHA1", NULL, info->conv_func_arg);

	if (!p) return (AUTHSASL_CANCELLED);
	return ( authsaslclient_cram(info, p, &hmac_sha1));
}

int authsaslclient_cramsha256(const struct authsaslclientinfo *info)
{
const char *p=(*info->start_conv_func)("CRAM-SHA256", NULL,
				       info->conv_func_arg);

	if (!p) return (AUTHSASL_CANCELLED);
	return ( authsaslclient_cram(info, p, &hmac_sha256));
}
