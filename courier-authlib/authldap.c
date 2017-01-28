/*
** Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if HAVE_CONFIG_H
#include "courier_auth_config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<string.h>
#include	<errno.h>

#include	"auth.h"
#include	"authldap.h"
#include	"courierauth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"
#include	"libhmac/hmac.h"

static int auth_ldap_login(const char *service, char *authdata,
			   int (*callback_func)(struct authinfo *, void *),
			   void *callback_arg)
{
	const char *user, *pass;

	if ((user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
	{
		DPRINTF("incomplete authentication data");
		errno=EPERM;
		return (-1);
	}

	return authldapcommon(service, user, pass, callback_func,
			      callback_arg);
}

static int auth_ldap_cram(const char *service,
			  const char *authtype, char *authdata,
			  int (*callback_func)(struct authinfo *, void *),
			  void *callback_arg)
{
	struct	cram_callback_info	cci;

	if (auth_get_cram(authtype, authdata, &cci))
		return (-1);

	cci.callback_func=callback_func;
	cci.callback_arg=callback_arg;

	return authldapcommon(service, cci.user, 0, &auth_cram_callback, &cci);
}

int auth_ldap(const char *service, const char *authtype, char *authdata,
	      int (*callback_func)(struct authinfo *, void *),
	      void *callback_arg)
{
	if (strcmp(authtype, AUTHTYPE_LOGIN) == 0)
		return (auth_ldap_login(service, authdata,
			callback_func, callback_arg));

	return (auth_ldap_cram(service, authtype, authdata,
			callback_func, callback_arg));
}


extern int auth_ldap_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
		  void *arg);

static struct authstaticinfo authldap_info={
	"authldap",
	auth_ldap,
	auth_ldap_pre,
	authldapclose,
	auth_ldap_changepw,
	authldapclose,
	auth_ldap_enumerate};


struct authstaticinfo *courier_authldap_init()
{
	return &authldap_info;
}
