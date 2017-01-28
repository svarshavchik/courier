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
#include	"authcustom.h"
#include	"courierauth.h"
#include	"courierauthstaticlist.h"
#include	"libhmac/hmac.h"


static int auth_custom_login(const char *service, char *authdata,
			     int (*callback_func)(struct authinfo *, void *),
			     int *callback_arg)
{
	const char *user, *pass;

	if ((user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
	{
		errno=EPERM;
		return (-1);
	}

	return authcustomcommon(user, pass, callback_func, callback_arg);
}

static int auth_custom_cram(const char *service,
			    const char *authtype,
			    char *authdata,
			    int (*callback_func)(struct authinfo *, void *),
			    void *callback_arg)
{
	struct	cram_callback_info	cci;

	if (auth_get_cram(authtype, authdata, &cci))
		return (-1);

	cci.callback_func=callback_func;
	cci.callback_arg=callback_arg;

	return authcustomcommon(cci.user, 0, &auth_cram_callback, &cci);
}

int auth_custom(const char *service, const char *authtype, char *authdata,
		int (*callback_func)(struct authinfo *, void *),
		void *callback_arg)
{
	if (strcmp(authtype, AUTHTYPE_LOGIN) == 0)
		return (auth_custom_login(service, authdata,
			callback_func, callback_arg));

	return (auth_custom_cram(service, authtype, authdata,
				 callback_func, callback_arg));
}


extern int auth_custom_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
		  void *arg);

static int auth_custom_chgpwd(const char *service,
			      const char *uid,
			      const char *oldpwd,
			      const char *newpwd)
{
	/*
	** Insert code to change the account's password here.
	**
	** return 0 if changed.
	**
	** return 1 if failed.
	** Set errno to EPERM if we had a temporary failure (such as invalid
	** old pwd).
	**
	** Set errno to EINVAL if we failed because we did not recognize uid.
	*/

	errno=EINVAL;
	return (-1);
}

static void auth_custom_idle()
{
	/*
	** Insert code to temporarily deallocate resources after remaining
	** idle (as part of authdaemond) for more than 5 minutes.
	*/
}

static struct authstaticinfo authcustom_info={
	"authcustom",
	auth_custom,
	auth_custom_pre,
	authcustomclose,
	auth_custom_chgpwd,
	auth_custom_idle};


struct authstaticinfo *courier_authcustom_init()
{
	return &authcustom_info;
}
