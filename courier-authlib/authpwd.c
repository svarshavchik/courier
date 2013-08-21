/*
** Copyright 1998 - 2004 Double Precision, Inc.  See COPYING for
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
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"


extern int auth_pwd_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
                        void *arg);
extern void auth_pwd_enumerate( void(*cb_func)(const char *name,
					       uid_t uid,
					       gid_t gid,
					       const char *homedir,
					       const char *maildir,
					       const char *options,
					       void *void_arg),
				void *void_arg);

int auth_pwd(const char *service, const char *authtype, char *authdata,
	     int (*callback_func)(struct authinfo *, void *),
	     void *callback_arg)
{
	const char *user, *pass;

	if (strcmp(authtype, AUTHTYPE_LOGIN) ||
		(user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
	{
		errno=EPERM;
		return (-1);
	}

	return auth_sys_common(&auth_pwd_pre,
			       user,
			       pass,
			       service,
			       callback_func,
			       callback_arg);
}

static void auth_pwd_cleanup()
{
#if	HAVE_ENDPWENT

	endpwent();
#endif
}

static struct authstaticinfo authpwd_info={
	"authpwd",
	auth_pwd,
	auth_pwd_pre,
	auth_pwd_cleanup,
	auth_syspasswd,
	auth_pwd_cleanup,
	auth_pwd_enumerate,
} ;


struct authstaticinfo *courier_authpwd_init()
{
	return &authpwd_info;
}
