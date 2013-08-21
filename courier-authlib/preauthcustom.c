/*
** Copyright 1998 - 2000 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if HAVE_CONFIG_H
#include "courier_auth_config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include	"auth.h"
#include	"authcustom.h"
#include	"courierauthdebug.h"


int auth_custom_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
                        void *arg)
{
	return (authcustomcommon(userid, 0, callback, arg));
}

static int do_auth_custom(const char *, struct authinfo *);

int authcustomcommon(const char *user, const char *pass,
        int (*callback)(struct authinfo *, void *),
                        void *arg)
{
	struct authinfo auth;
	int rc;

	memset(&auth, 0, sizeof(auth));

	rc=do_auth_custom(user, &auth);

	if (rc)
		return (rc);

	if (pass == 0)
		return (0);	/* Just get the authentication info */

	if (auth.clearpasswd)
	{
		if (strcmp(pass, auth.clearpasswd))
			return (-1);
	}
	else
	{
		const char *p=auth.passwd;

		if (!p || authcheckpassword(pass, p))
			return (-1);
	}

	auth.clearpasswd=pass;
	return ((*callback)(&auth, arg));
}

static int do_auth_custom(const char *userid, struct authinfo *authinfo)
{
	/*
	** Insert custom authentication code here.  This code must obtain
	** authentication information for account 'userid'.
	**
	** If you need to link with specific external libraries (-lnsl_s,
	** et al), you'll just have to bite the bullet, install automake
	** and autoconf, then set authcustom.libsdep and authcustom_LDADD
	** in Makefile.am
	*/

	/*
	** If userid does not exist, return (-1).
	*/

	DPRINTF("authcustom: nothing implemented in do_auth_custom()");
	return (-1);

	/*
	** If there is some kind of a system problem, that is you are
	** unable to check whether userid is valid (the back end database
	** is down, or something) return (1).
	*/

	/*
	** Otherwise, initialize the authinfo structure, and return (0).
	**
	** NOTES: this function can be called repeated within a single
	** process, in certain contexts.  Do not simply dynamically
	** allocate memory for all the character strings, each time, because
	** the caller WILL NOT free the memory of any dynamically allocated
	** strings.  If you keep dynamically allocating memory, each time,
	** you're going to get a memory leak, somewhere, and YOU'LL FUCK
	** YOURSELF.  What you should do is either use a static buffer,
	** or dynamically allocate some memory, and free that memory on
	** the next function call.
	**
	** Additionally:
	**
	** If you open any files, you MUST set FD_CLOEXEC bit on any
	** file descriptor you create (open files, sockets, whatnot).
	**
	** Someone else might do a fork and an exec, so you need to make
	** sure things get cleaned up, in that event.
	**
	** Fields in the auth structure:
	**
	** sysusername - REQUIRED - user name, should simply be userid,
	**                          unless you know what you're doing.
	** sysuserid - REQUIRED - pointer to the user's uid_t (yes, it's
	**                        a pointer).
	** sysgroupid - REQUIRED - gid_t, the group ID of the user.
	**
	** homedir - REQUIRED - home directory.
	**
	** address - REQUIRED - the 'identity' of the authenticated user,
	**                      the e-mail address.  It is acceptable to set
	**                      this field also to userid, if you can't think
	**                      of anything better to do.
	**
	** fullname - OPTIONAL - user's full name.
	**
	** maildir - OPTIONAL - user's primary maildir ($HOME/Maildir default)
	**
	** quota - OPTIONAL - user's maildir quota (see a README somewhere)
	**
	** passwd, clearpasswd - one of these fields must be initialized,
	**                       either one is ok.  Initialize clearpasswd
	**                       if you store cleartext passwords.  If you
	**                       store crypted passwords, initialize passwd.
	*/
}

void authcustomclose()
{
	/*
	** Place any cleanup here.
	*/
}
