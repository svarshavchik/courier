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
#include <iostream>

#include	"authmysql.h"
extern "C" {
#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"
#include	"courierauth.h"
}

static bool verify(const authmysqluserinfo &authinfo,
		   const char *user,
		   const char *pass)
{
	if (authinfo.home.size() == 0)	/* User not found */
	{
		errno=EPERM;
		return false;		/* Username not found */
	}

	if (authinfo.cryptpw.size())
	{
		if (authcheckpassword(pass,authinfo.cryptpw.c_str()))
		{
			errno=EPERM;
			return false;	/* User/Password not found. */
		}
	}
	else if (authinfo.clearpw.size())
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
		DPRINTF("no password available to compare for '%s'", user);
		errno=EPERM;
		return false;		/* Username not found */
	}
	return true;
}

static int auth_mysql_login(const char *service, char *authdata,
			    int (*callback_func)(struct authinfo *, void *),
			    void *callback_arg)
{
	char *user, *pass;
	struct	authinfo	aa;

	if ((user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
	{
		errno=EPERM;
		return (-1);
	}

	authmysqluserinfo authinfo;

	if (!auth_mysql_getuserinfo(user, service, authinfo))
		/* Fatal error - such as MySQL being down */
	{
		errno=EACCES;
		return (-1);
	}

	if (!verify(authinfo, user, pass))
		return -1;

	memset(&aa, 0, sizeof(aa));

	aa.sysuserid= &authinfo.uid;
	aa.sysgroupid= authinfo.gid;
	aa.homedir=authinfo.home.c_str();

#define STR(z) (authinfo.z.size() ? authinfo.z.c_str():0)

	aa.maildir=STR(maildir);
	aa.address=STR(username);
	aa.quota=STR(quota);
	aa.fullname=STR(fullname);
	aa.options=STR(options);
	aa.clearpasswd=pass;
	aa.passwd=STR(cryptpw);
	courier_authdebug_authinfo("DEBUG: authmysql: ", &aa,
				   aa.clearpasswd, aa.passwd);

	return (*callback_func)(&aa, callback_arg);
}

static int auth_mysql_changepw(const char *service, const char *user,
			       const char *pass,
			       const char *newpass)
{
	authmysqluserinfo authinfo;

	if (!auth_mysql_getuserinfo(user, service, authinfo))
	{
		errno=ENOENT;
		return (-1);
	}

	if (!verify(authinfo, user, pass))
	{
		return (-1);
	}

	if (!auth_mysql_setpass(user, newpass, authinfo.cryptpw.c_str()))
	{
		errno=EPERM;
		return (-1);
	}
	return (0);
}

static int auth_mysql_cram(const char *service,
			   const char *authtype, char *authdata,
			   int (*callback_func)(struct authinfo *, void *),
			   void *callback_arg)
{
	struct	cram_callback_info	cci;

	if (auth_get_cram(authtype, authdata, &cci))
		return (-1);

	cci.callback_func=callback_func;
	cci.callback_arg=callback_arg;

	return auth_mysql_pre(cci.user, service, &auth_cram_callback, &cci);
}

int auth_mysql(const char *service, const char *authtype, char *authdata,
	       int (*callback_func)(struct authinfo *, void *),
	       void *callback_arg)
{
	if (strcmp(authtype, AUTHTYPE_LOGIN) == 0)
		return (auth_mysql_login(service, authdata,
			callback_func, callback_arg));

	return (auth_mysql_cram(service, authtype, authdata,
			callback_func, callback_arg));
}

extern int auth_mysql_pre(const char *user, const char *service,
			  int (*callback)(struct authinfo *, void *),
			  void *arg);

static struct authstaticinfo authmysql_info={
	"authmysql",
	auth_mysql,
	auth_mysql_pre,
	auth_mysql_cleanup,
	auth_mysql_changepw,
	auth_mysql_cleanup,
	auth_mysql_enumerate};


extern "C" {
	struct authstaticinfo *courier_authmysql_init()
	{
		return &authmysql_info;
	}
}
