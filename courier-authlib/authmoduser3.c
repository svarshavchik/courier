/*
** Copyright 1998 - 2005 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"auth.h"
#include	"courierauth.h"
#include	"courierauthdebug.h"
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<errno.h>

static int badstr(const char *p)
{
	while (p && *p)
	{
		if ((int)(unsigned char)*p < ' ')
			return 1;
		++p;
	}
	return 0;
}

/* Create a new string consisting of:
**  - username
**  - DEFDOMAIN if username does not contain any characters from DOMAINSEP
**              (or if DOMAINSEP not set, then if username does not contain
**               the first char of DEFDOMAIN)
**  - strings s1, s2, s3
*/

char *strdupdefdomain(const char *userid, const char *s1, const char *s2,
		      const char *s3)
{
char	*p, *q, *r;

	q=getenv("DEFDOMAIN");
	if (q && q[0])
	{
		r=getenv("DOMAINSEP");
		if (r ? strpbrk(userid, r) : strchr(userid, q[0])) q = "";
	}
	else
		q = "";

	p=malloc(strlen(userid)+strlen(q)+strlen(s1)+strlen(s2)+strlen(s3)+1);
	if (p)
		strcat(strcat(strcat(strcat(strcpy(p, userid), q), s1), s2), s3);
	return p;
}

int auth_login(const char *service,
	       const char *userid,
	       const char *passwd,
	       int (*callback_func)(struct authinfo *, void *),
	       void *callback_arg)

{
	struct auth_meta dummy;

	memset(&dummy, 0, sizeof(dummy));

	return auth_login_meta(&dummy, service, userid, passwd, callback_func,
			       callback_arg);
}

int auth_login_meta(struct auth_meta *meta,
		    const char *service,
		    const char *userid,
		    const char *passwd,
		    int (*callback_func)(struct authinfo *, void *),
		    void *callback_arg)

{
	char	*p;
	int rc;

	if (badstr(userid) || badstr(passwd))
	{
		errno=EINVAL;
		return -1;
	}

	courier_authdebug_login_init();
	courier_authdebug_login( 1, "username=%s", userid );
	courier_authdebug_login( 2, "password=%s", passwd );

	p = strdupdefdomain(userid, "\n", passwd, "\n");
	if (!p)
		return (-1);

	rc=auth_generic_meta(meta, service, AUTHTYPE_LOGIN, p,
			     callback_func,
			     callback_arg);
	free(p);
	return rc;
}
