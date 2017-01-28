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
#include	"authpgsql.h"
#include	"courierauth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"
#include	"libhmac/hmac.h"

extern void auth_pgsql_enumerate( void(*cb_func)(const char *name,
						 uid_t uid,
						 gid_t gid,
						 const char *homedir,
						 const char *maildir,
						 const char *options,
						 void *void_arg),
				  void *void_arg);

static int auth_pgsql_cram(const char *service,
			   const char *authtype, char *authdata,
			   int (*callback_func)(struct authinfo *, void *),
			   void *callback_arg)
{
	struct	cram_callback_info	cci;

	if (auth_get_cram(authtype, authdata, &cci))
		return (-1);

	cci.callback_func=callback_func;
	cci.callback_arg=callback_arg;

	return auth_pgsql_pre(cci.user, service, &auth_cram_callback, &cci);
}

int auth_pgsql(const char *service, const char *authtype, char *authdata,
	       int (*callback_func)(struct authinfo *, void *),
	       void *callback_arg)
{
	if (strcmp(authtype, AUTHTYPE_LOGIN) == 0)
		return (auth_pgsql_login(service, authdata,
			callback_func, callback_arg));

	return (auth_pgsql_cram(service, authtype, authdata,
			callback_func, callback_arg));
}

extern int auth_pgsql_pre(const char *user, const char *service,
			  int (*callback)(struct authinfo *, void *),
			  void *arg);

static struct authstaticinfo authpgsql_info={
	"authpgsql",
	auth_pgsql,
	auth_pgsql_pre,
	auth_pgsql_cleanup,
	auth_pgsql_changepw,
	auth_pgsql_cleanup,
	auth_pgsql_enumerate};

struct authstaticinfo *courier_authpgsql_init()
{
	return &authpgsql_info;
}
