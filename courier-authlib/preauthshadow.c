/*
** Copyright 1998 - 2005 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<time.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<pwd.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_SHADOW_H
#include	<shadow.h>
#endif


#include	"auth.h"
#include	"courierauthdebug.h"


int auth_shadow_pre(const char *userid, const char *service,
	int (*callback)(struct authinfo *, void *),
			void *arg)
{
struct authinfo auth;
struct passwd *pw;
struct spwd *spw;
long today;

	memset(&auth, 0, sizeof(auth));

	if ((pw=getpwnam(userid)) == NULL)
	{
		if (errno == ENOMEM)	return 1;
		return -1;
	}

	if ((spw=getspnam(userid)) == NULL)
	{
		if (errno == ENOMEM)	return 1;
		return -1;
	}

	today = (long)time(NULL)/(24L*60*60);

	if ((spw->sp_expire > 0) && (today > spw->sp_expire))
	{
		DPRINTF("authshadow: %s - account expired", userid);
		return -1;			/* account expired */
	}

	if ((spw->sp_lstchg != -1) && (spw->sp_max != -1) &&
		((spw->sp_lstchg + spw->sp_max) < today))
	{
		DPRINTF("authshadow: %s - password expired", userid);
		return -1;			/* password expired */
	}

	auth.sysusername=userid;
	auth.sysgroupid=pw->pw_gid;
	auth.homedir=pw->pw_dir;
	auth.address=userid;
	auth.fullname=pw->pw_gecos;
	auth.passwd=spw->sp_pwdp;

	courier_authdebug_authinfo("DEBUG: authshadow: ", &auth, 0, pw->pw_passwd);
	return ((*callback)(&auth, arg));
}
