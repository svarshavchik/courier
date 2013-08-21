/*
** Copyright 1998 - 2000 Double Precision, Inc.  See COPYING for
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



void auth_pwd_enumerate( void(*cb_func)(const char *name,
					uid_t uid,
					gid_t gid,
					const char *homedir,
					const char *maildir,
					const char *options,
					void *void_arg),
			 void *void_arg)
{
	struct passwd *pw;

	setpwent();

	while ( (pw=getpwent()) != NULL)
	{
		if (pw->pw_uid < 100)
			continue;

		(*cb_func)(pw->pw_name, pw->pw_uid,
			   pw->pw_gid,
			   pw->pw_dir,
			   NULL,
			   NULL,
			   void_arg);
	}
	endpwent();
	(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
}
