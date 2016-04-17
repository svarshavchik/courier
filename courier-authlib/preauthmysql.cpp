/*
** Copyright 1998 - 1999 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<pwd.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

extern "C" {
#include	"auth.h"
}
#include	"authmysql.h"

int	auth_mysql_pre(const char *user, const char *service,
		int (*callback)(struct authinfo *, void *), void *arg)
{
	authmysqluserinfo authinfo;
	struct	authinfo	aa;

	if (!auth_mysql_getuserinfo(user, service, authinfo))
		/* Fatal error - such as MySQL being down */
		return (1);

	if (authinfo.home.size() == 0)	/* User not found */
		return (-1);

	memset(&aa, 0, sizeof(aa));

	/*aa.sysusername=user;*/
	aa.sysuserid= &authinfo.uid;
	aa.sysgroupid= authinfo.gid;

#define STR(z) (authinfo.z.size() ? authinfo.z.c_str():0)

	aa.homedir=STR(home);
	aa.maildir=STR(maildir);
	aa.address=STR(username);
	aa.passwd=STR(cryptpw);
	aa.clearpasswd=STR(clearpw);
	aa.fullname=STR(fullname);
	aa.quota=STR(quota);
	aa.options=STR(options);
	return ((*callback)(&aa, arg));
}
