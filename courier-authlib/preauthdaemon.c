/*
** Copyright 2000-2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthsasl.h"
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>


extern int authdaemondo_meta(struct auth_meta *meta,
			     const char *authreq,
			     int (*func)(struct authinfo *, void *),
			     void *arg);

int auth_getuserinfo(const char *service, const char *uid,
		     int (*callback)(struct authinfo *, void *),
		     void *arg)
{
	struct auth_meta dummy;

	memset(&dummy, 0, sizeof(dummy));

	return auth_getuserinfo_meta(&dummy, service, uid, callback, arg);
}

int auth_getuserinfo_meta(struct auth_meta *meta,
			  const char *service, const char *uid,
			  int (*callback)(struct authinfo *, void *),
			  void *arg)
{
char    *buf=malloc(strlen(service)+strlen(uid)+20);
int     rc;

	if (!buf)
	{
		perror("malloc");
		return (1);
	}
	strcat(strcat(strcat(strcat(strcpy(buf, "PRE . "), service), " "),
		uid), "\n");

	rc=authdaemondo_meta(meta, buf, callback, arg);
	free(buf);
	return (rc);
}
