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
#include	"auth.h"
#include	"courierauthdebug.h"

#if	HAVE_SHADOW_H
#include	<shadow.h>
#endif


int auth_pam_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
                        void *arg)
{
struct authinfo auth;
struct passwd *pw;
#if	HAVE_GETSPENT
struct spwd *spw;
#endif

	memset(&auth, 0, sizeof(auth));

	if ((pw=getpwnam(userid)) == 0)
	{
		if (errno == ENOMEM)	return (1);
		DPRINTF("authpam: username '%s' not found in password file", userid);
		errno=EPERM;
		return (-1);
	}

	auth.sysusername=userid;
	auth.sysgroupid=pw->pw_gid;
	auth.homedir=pw->pw_dir;
	auth.address=userid;
	auth.fullname=pw->pw_gecos;
	auth.passwd=pw->pw_passwd;

#if	HAVE_GETSPENT
	if ((spw=getspnam(userid)) != 0)
		auth.passwd=spw->sp_pwdp;
#endif
	courier_authdebug_authinfo("DEBUG: authpam: ", &auth, 0, pw->pw_passwd);

	return ((*callback)(&auth, arg));
}
