/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rfc822/rfc822.h"
#include	<string.h>
#include	<stdlib.h>

int configt_islocal(const struct rfc822token *t, char **p)
{
char	*address=rfc822_gettok(t);
int	rc;

	if (!address)	clog_msg_errno();
	rc=config_islocal(address, p);
	free(address);
	return (rc);
}
