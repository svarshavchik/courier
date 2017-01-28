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

extern "C" {
#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauth.h"
#include	"courierauthdebug.h"
#include	"libhmac/hmac.h"
}

#include	"authsqlite.h"

static int auth_sqlite_pre(const char *user, const char *service,
			   int (*callback)(struct authinfo *, void *), void *arg)
{
	authsqliteuserinfo authinfo;
	struct	authinfo	aa;

	if (!auth_sqlite_getuserinfo(user, service, authinfo))
		// Fatal error - such as Sqlite being down
		return (1);

	if (authinfo.home.empty()) // User not found
		return (-1);

	memset(&aa, 0, sizeof(aa));

	/*aa.sysusername=user;*/
	aa.sysuserid= &authinfo.uid;
	aa.sysgroupid= authinfo.gid;
	aa.homedir=authinfo.home.c_str();
	aa.maildir=authinfo.maildir.empty() ? NULL:authinfo.maildir.c_str();
	aa.address=authinfo.username.c_str();
	aa.passwd=authinfo.cryptpw.c_str();
	aa.clearpasswd=authinfo.clearpw.c_str();
	aa.fullname=authinfo.fullname.c_str();
	aa.quota=authinfo.quota.empty() ? NULL:authinfo.quota.c_str();
	aa.options=authinfo.options.c_str();
	return ((*callback)(&aa, arg));
}

extern void auth_sqlite_enumerate( void(*cb_func)(const char *name,
						  uid_t uid,
						  gid_t gid,
						  const char *homedir,
						  const char *maildir,
						  const char *options,
						  void *void_arg),
				   void *void_arg);

static bool docheckpw(authsqliteuserinfo &authinfo, const char *pass)
{
	if (!authinfo.cryptpw.empty())
	{
		if (authcheckpassword(pass, authinfo.cryptpw.c_str()))
		{
			errno=EPERM;
			return false;	/* User/Password not found. */
		}
	}
	else if (!authinfo.clearpw.empty())
	{
		if (authinfo.clearpw != pass)
		{
			if (courier_authdebug_login_level >= 2)
			{
				DPRINTF("supplied password '%s' does not match clearpasswd '%s'",
					pass, authinfo.clearpw.c_str());
			}
			else
			{
				DPRINTF("supplied password does not match clearpasswd");
			}
			errno=EPERM;
			return false;
		}
	}
	else
	{
		DPRINTF("no password available to compare");
		errno=EPERM;
		return false;		/* Username not found */
	}
	return true;
}

static int auth_sqlite_login(const char *service, char *authdata,
			     int (*callback_func)(struct authinfo *, void *),
			     void *callback_arg)
{
	char *user, *pass;
	authsqliteuserinfo authinfo;
	struct	authinfo	aa;


	if ((user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
	{
		errno=EPERM;
		return (-1);
	}

	if (!auth_sqlite_getuserinfo(user, service, authinfo))
		// Fatal error - such as Sqlite being down
	{
		errno=EACCES;
		return (1);
	}

	if (!docheckpw(authinfo, pass))
		return (-1);

	memset(&aa, 0, sizeof(aa));

	aa.sysuserid= &authinfo.uid;
	aa.sysgroupid= authinfo.gid;
	aa.homedir=authinfo.home.c_str();
	aa.maildir=authinfo.maildir.empty() ? NULL:authinfo.maildir.c_str();
	aa.address=authinfo.username.c_str();
	aa.quota=authinfo.quota.empty() ? NULL:authinfo.quota.c_str();
	aa.fullname=authinfo.fullname.c_str();
	aa.options=authinfo.options.c_str();
	aa.clearpasswd=pass;
	aa.passwd=authinfo.cryptpw.c_str();
	courier_authdebug_authinfo("DEBUG: authsqlite: ", &aa,
				   authinfo.clearpw.c_str(),
				   authinfo.cryptpw.c_str());

	return (*callback_func)(&aa, callback_arg);
}

static int auth_sqlite_changepw(const char *service, const char *user,
				const char *pass,
				const char *newpass)
{
	authsqliteuserinfo authinfo;

	if (!auth_sqlite_getuserinfo(user, service, authinfo))
	{
		errno=ENOENT;
		return (-1);
	}

	if (!docheckpw(authinfo, pass))
	{
		errno=EPERM;
		return (-1);	/* User/Password not found. */
	}

	if (auth_sqlite_setpass(user, newpass, authinfo.cryptpw.c_str()))
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

extern "C" {
	struct authstaticinfo *courier_authsqlite_init()
	{
		return &authsqlite_info;
	}
}
