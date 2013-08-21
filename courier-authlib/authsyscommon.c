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



struct callback_info {
	const char *pass;
	int (*callback_func)(struct authinfo *, void *);
	void *callback_arg;
	};

static int check_pw(struct authinfo *a, void *v)
{
	struct callback_info *ci=(struct callback_info *)v;
	int rc;

	if (a->passwd == NULL)
	{
		DPRINTF("no password available to compare");
		errno=EPERM;
		return (-1);
	}

	if (authcheckpassword(ci->pass, a->passwd))
	{
		errno=EPERM;
		return (-1);
	}
	a->clearpasswd=ci->pass;
	rc=(*ci->callback_func)(a, ci->callback_arg);
	a->clearpasswd=NULL;
	return rc;
}

int auth_sys_common( int (*auth_pre_func)(const char *,
					  const char *,
					  int (*)(struct authinfo *,
						  void *),
					  void *),
		     const char *user,
		     const char *pass,
		     const char *service,
		     int (*callback_func)(struct authinfo *, void *),
		     void *callback_arg)
{
	struct callback_info ci;

	ci.pass=pass;
	ci.callback_func=callback_func;
	ci.callback_arg=callback_arg;
	return (*auth_pre_func)(user, service, check_pw, &ci);
}

