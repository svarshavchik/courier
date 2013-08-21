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

#include	"auth.h"
#include	"courierauthdebug.h"


int auth_pwd_pre(const char *userid, const char *service,
		 int (*callback)(struct authinfo *, void *),
		 void *arg)
{
struct authinfo auth;
struct passwd *pw;

	memset(&auth, 0, sizeof(auth));

	if ((pw=getpwnam(userid)) == 0)
	{
		if (errno == ENOMEM)	return (1);
		return (-1);
	}

	auth.sysusername=userid;
	auth.sysgroupid=pw->pw_gid;
	auth.homedir=pw->pw_dir;
	auth.address=userid;
	auth.fullname=pw->pw_gecos;
	auth.passwd=pw->pw_passwd;

	courier_authdebug_authinfo("DEBUG: authpwd: ", &auth, 0, pw->pw_passwd);
	return ((*callback)(&auth, arg));
}
