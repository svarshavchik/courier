/*
** Copyright 2000-2008 Double Precision, Inc.  See COPYING for
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
#include	"authsqlite.h"
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"
#include	"libhmac/hmac.h"
#include	"cramlib.h"


extern void auth_sqlite_enumerate( void(*cb_func)(const char *name,
						  uid_t uid,
						  gid_t gid,
						  const char *homedir,
						  const char *maildir,
						  const char *options,
						  void *void_arg),
				   void *void_arg);

static int auth_sqlite_login(const char *service, char *authdata,
			     int (*callback_func)(struct authinfo *, void *),
			     void *callback_arg)
{
	char *user, *pass;
	struct authsqliteuserinfo *authinfo;
	struct	authinfo	aa;


	if ((user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
	{
		errno=EPERM;
		return (-1);
	}

	authinfo=auth_sqlite_getuserinfo(user, service);

	if (!authinfo)		/* Fatal error - such as Sqlite being down */
	{
		errno=EACCES;
		return (1);
	}

	if (authinfo->cryptpw)
	{
		if (authcheckpassword(pass,authinfo->cryptpw))
		{
			errno=EPERM;
			return (-1);	/* User/Password not found. */
		}
	}
	else if (authinfo->clearpw)
	{
		if (strcmp(pass, authinfo->clearpw))
		{
			if (courier_authdebug_login_level >= 2)
			{
				DPRINTF("supplied password '%s' does not match clearpasswd '%s'",
					pass, authinfo->clearpw);
			}
			else
			{
				DPRINTF("supplied password does not match clearpasswd");
			}
			errno=EPERM;
			return (-1);
		}
	}
	else
	{
		DPRINTF("no password available to compare");
		errno=EPERM;
		return (-1);		/* Username not found */
	}

	memset(&aa, 0, sizeof(aa));

	aa.sysuserid= &authinfo->uid;
	aa.sysgroupid= authinfo->gid;
	aa.homedir=authinfo->home;
	aa.maildir=authinfo->maildir && authinfo->maildir[0] ?
		authinfo->maildir:0;
	aa.address=authinfo->username;
	aa.quota=authinfo->quota && authinfo->quota[0] ?
		authinfo->quota:0;
	aa.fullname=authinfo->fullname;
	aa.options=authinfo->options;
	aa.clearpasswd=pass;
	aa.passwd=authinfo->cryptpw;
	courier_authdebug_authinfo("DEBUG: authsqlite: ", &aa,
			    authinfo->clearpw, authinfo->cryptpw);

	return (*callback_func)(&aa, callback_arg);
}

static int auth_sqlite_changepw(const char *service, const char *user,
				const char *pass,
				const char *newpass)
{
	struct authsqliteuserinfo *authinfo;

	authinfo=auth_sqlite_getuserinfo(user, service);

	if (!authinfo)
	{
		errno=ENOENT;
		return (-1);
	}

	if (authinfo->cryptpw)
	{
		if (authcheckpassword(pass,authinfo->cryptpw))
		{
			errno=EPERM;
			return (-1);	/* User/Password not found. */
		}
	}
	else if (authinfo->clearpw)
	{
		if (strcmp(pass, authinfo->clearpw))
		{
			errno=EPERM;
			return (-1);
		}
	}
	else
	{
		errno=EPERM;
		return (-1);
	}

	if (auth_sqlite_setpass(user, newpass, authinfo->cryptpw))
	{
		errno=EPERM;
		return (-1);
	}
	return (0);
}

static int auth_sqlite_cram(const char *service,
			    const char *authtype, char *authdata,
			    int (*callback_func)(struct authinfo *, void *),
			    void *callback_arg)
{
	struct	cram_callback_info	cci;

	if (auth_get_cram(authtype, authdata, &cci))
		return (-1);

	cci.callback_func=callback_func;
	cci.callback_arg=callback_arg;

	return auth_sqlite_pre(cci.user, service, &auth_cram_callback, &cci);
}

int auth_sqlite(const char *service, const char *authtype, char *authdata,
		int (*callback_func)(struct authinfo *, void *),
		void *callback_arg)
{
	if (strcmp(authtype, AUTHTYPE_LOGIN) == 0)
		return (auth_sqlite_login(service, authdata,
					  callback_func, callback_arg));

	return (auth_sqlite_cram(service, authtype, authdata,
			callback_func, callback_arg));
}

extern int auth_sqlite_pre(const char *user, const char *service,
			  int (*callback)(struct authinfo *, void *),
			  void *arg);

static struct authstaticinfo authsqlite_info={
	"authsqlite",
	auth_sqlite,
	auth_sqlite_pre,
	auth_sqlite_cleanup,
	auth_sqlite_changepw,
	auth_sqlite_cleanup,
	auth_sqlite_enumerate};


struct authstaticinfo *courier_authsqlite_init()
{
	return &authsqlite_info;
}
