/*
** Copyright 1998 - 1999 Double Precision, Inc.  See COPYING for
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
#include	"courierauthdebug.h"
#include	"userdb/userdb.h"


int auth_userdb_pre_common(const char *userid, const char *service,
	int needpass,
	int (*callback)(struct authinfo *, void *),
                        void *arg)
{
char	*u;
struct	userdbs *udb;
struct authinfo auth;
char	*udbs;
char	*services;
char	*passwords=0;
int	rc;

	userdb_set_debug(courier_authdebug_login_level);
	userdb_init(USERDB ".dat");
	/* We rely on DPRINTF doing 'safe' printing */
	DPRINTF("userdb: looking up '%s'", userid);
        if ( (u=userdb(userid)) == 0)
	{
		userdb_close();
		errno=EPERM;
		return (-1);
	}

        if ((udb=userdb_creates(u)) == 0)
        {
		free(u);
                return (-1);
        }
	free(u);

        memset(&auth, 0, sizeof(auth));

	auth.sysuserid= &udb->udb_uid;
        auth.sysgroupid=udb->udb_gid;
        auth.homedir=udb->udb_dir;
        auth.address=userid;
        auth.fullname=udb->udb_gecos;
	auth.options=udb->udb_options;

	if (needpass)
	{
		udbs=userdbshadow(USERDB "shadow.dat", userid);

		if (udbs)
		{
			if ((services=malloc(strlen(service)+sizeof("pw"))) == 0)
			{
				perror("malloc");
				free(udbs);
				userdb_frees(udb);
				return (1);
			}

			strcat(strcpy(services, service), "pw");

			passwords=userdb_gets(udbs, services);

			if (passwords)
			{
				DPRINTF("found %s in userdbshadow", services);
			}
			else
			{
				passwords=userdb_gets(udbs, "systempw");
				if (passwords)
				{
					DPRINTF("found systempw in userdbshadow");
				}
				else
				{
					DPRINTF("no %s or systempw value in userdbshadow for %s",
						services, userid);
				}
			}

			free(services);
			free(udbs);
		}
		auth.passwd=passwords;
	}

	auth.maildir=udb->udb_mailbox;
	auth.quota=udb->udb_quota;

	courier_authdebug_authinfo("DEBUG: authuserdb: ", &auth, 0, passwords);
	rc= (*callback)(&auth, arg);
	if (passwords)	free(passwords);
	userdb_frees(udb);
	return (rc);
}

void auth_userdb_cleanup()
{
	userdb_close();
}

void auth_userdb_enumerate( void(*cb_func)(const char *name,
					   uid_t uid,
					   gid_t gid,
					   const char *homedir,
					   const char *maildir,
					   const char *options,
					   void *void_arg),
			    void *void_arg)
{
	struct userdbs *u;

	userdb_init(USERDB ".dat");

	for (u=userdb_enum_first(); u; u=userdb_enum_next())
	{
		(*cb_func)(u->udb_name,
			   u->udb_uid,
			   u->udb_gid,
			   u->udb_dir,
			   u->udb_mailbox,
			   u->udb_options,
			   void_arg);
		userdb_frees(u);
	}
	(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
}

