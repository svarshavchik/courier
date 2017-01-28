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
#include	"courierauth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"
#include	"libhmac/hmac.h"
#include	"userdb/userdb.h"


extern void auth_userdb_enumerate( void(*cb_func)(const char *name,
						  uid_t uid,
						  gid_t gid,
						  const char *homedir,
						  const char *maildir,
						  const char *options,
						  void *void_arg),
				   void *void_arg);

extern int auth_userdb_pre_common(const char *, const char *, int,
        int (*callback)(struct authinfo *, void *),
                        void *arg);

extern void auth_userdb_cleanup();

struct callback_info {
        const char *pass;
	int (*callback_func)(struct authinfo *, void *);
	void *callback_arg;
        };

static int callback_userdb(struct authinfo *a, void *p)
{
struct callback_info *i=(struct callback_info *)p;

	if (a->passwd == 0)
	{
		DPRINTF("no password available to compare\n");
		errno=EPERM;
		return (-1);
	}

	if (authcheckpassword(i->pass, a->passwd))
		return (-1);

	a->clearpasswd=i->pass;
	return (*i->callback_func)(a, i->callback_arg);
}


static int auth_cram(const char *service, const char *authtype, char *authdata,
		     int (*callback_func)(struct authinfo *, void *),
		     void *callback_arg)
{
char	*u;
char	*udbs;
char	*passwords;
char	*services;
struct	userdbs *udb;
struct cram_callback_info cci;

struct	authinfo	aa;
int	rc;

	if (auth_get_cram(authtype, authdata, &cci))
		return (-1);

	userdb_set_debug(courier_authdebug_login_level);
	userdb_init(USERDB ".dat");
        if ( (u=userdb(cci.user)) == 0)
	{
		userdb_close();
		return (-1);
	}

	if ( (udbs=userdbshadow(USERDB "shadow.dat", cci.user)) == 0)
	{
		free(u);
		userdb_close();
		return (-1);
	}

	if ((services=malloc(strlen(service)+strlen(cci.h->hh_name)
			     +sizeof("-hmac-pw"))) == 0)
	{
		free(udbs);
		free(u);
		userdb_close();
		errno=ENOSPC;
		return (1); /* tempfail */
	}

	strcat(strcat(strcat(strcpy(services, service), "-hmac-"),
		      cci.h->hh_name), "pw");

	passwords=userdb_gets(udbs, services);
	if (passwords == 0)
	{
		strcat(strcat(strcpy(services, "hmac-"),
			      cci.h->hh_name), "pw");
		passwords=userdb_gets(udbs, services);
	}
	if (passwords == 0)
	{
		DPRINTF("authcram: no %s-%s or %s value found",
			service, services, services);
	}
	free(services);

	if (passwords == 0)
	{
		free(udbs);
		free(u);
		userdb_close();
		return (-1);
	}

	if (auth_verify_cram(cci.h, cci.challenge, cci.response,
			     passwords))
	{
		free(passwords);
		free(udbs);
		free(u);
		userdb_close();
		return (-1);
	}

	free(passwords);
	free(udbs);
        if ((udb=userdb_creates(u)) == 0)
        {
		free(u);
		userdb_close();
                return (1);
        }


	memset(&aa, 0, sizeof(aa));

	/*aa.sysusername=user;*/
	aa.sysuserid= &udb->udb_uid;
	aa.sysgroupid= udb->udb_gid;
	aa.homedir=udb->udb_dir;
	aa.address=cci.user;
	aa.maildir=udb->udb_mailbox;
	aa.options=udb->udb_options;
	rc=(*callback_func)(&aa, callback_arg);

        free(u);
	userdb_close();

	userdb_frees(udb);
	return rc;
}

int auth_userdb(const char *service, const char *authtype, char *authdata,
		int (*callback_func)(struct authinfo *, void *),
		void *callback_arg)
{
	const char *user, *pass;
	struct	callback_info	ci;

	if (strcmp(authtype, AUTHTYPE_LOGIN) ||
		(user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
		return auth_cram(service, authtype, authdata,
				 callback_func, callback_arg);

	ci.pass=pass;
	ci.callback_func=callback_func;
	ci.callback_arg=callback_arg;
	return auth_userdb_pre_common(user, service, 1, &callback_userdb, &ci);
}

extern int auth_userdb_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
		    void *arg);

extern int auth_userdb_passwd(const char *service,
			      const char *userid,
			      const char *opwd_buf,
			      const char *npwd_buf);

static struct authstaticinfo authuserdb_info={
	"authuserdb",
	auth_userdb,
	auth_userdb_pre,
	auth_userdb_cleanup,
	auth_userdb_passwd,
	auth_userdb_cleanup,
	auth_userdb_enumerate};


struct authstaticinfo *courier_authuserdb_init()
{
	return &authuserdb_info;
}
