/* $Id $ */

/*
** Copyright 2008 Double Precision, Inc.  See COPYING for
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

int authsaslclient_external(const struct authsaslclientinfo *info)
{
	int i;

	i=(*info->plain_conv_func)("EXTERNAL", "", info->conv_func_arg);

	return (i);
}
