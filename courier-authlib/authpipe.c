/*
** Copyright 1998 - 2005 Double Precision, Inc.  See COPYING for
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
#include	<unistd.h>
#include	"numlib/numlib.h"

#include	"courierauth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"

#include	"authpipelib.h"
#include	"authpiperc.h"

extern int _authdaemondopasswd(int wrfd, int rdfd, char *buffer, int bufsiz);
extern int _authdaemondo(int wrfd, int rdfd, const char *authreq,
			 int (*func)(struct authinfo *, void *), void *arg);
extern int _auth_enumerate(int wrfd, int rdfd,
			   void(*cb_func)(const char *name,
					  uid_t uid,
					  gid_t gid,
					  const char *homedir,
					  const char *maildir,
					  const char *options,
					  void *void_arg),
			   void *void_arg);

static int disabled_flag;

/* modelled on auth_generic() in authdaemon.c */
int auth_pipe(const char *service, const char *authtype, char *authdata,
		int (*callback_func)(struct authinfo *, void *),
		void *callback_arg)
{
	char	tbuf[NUMBUFSIZE];
	size_t	l=strlen(service)+strlen(authtype)+strlen(authdata)+2;
	char	*n=libmail_str_size_t(l, tbuf);
	char	*buf=malloc(strlen(n)+l+20);
	int	rdfd, wrfd, rc;

	if (!buf)
		return 1;

	if (disabled_flag)
	{
		free(buf);
		return -1;
	}

	strcat(strcat(strcpy(buf, "AUTH "), n), "\n");
	strcat(strcat(buf, service), "\n");
	strcat(strcat(buf, authtype), "\n");
	strcat(buf, authdata);
	
	if (getPipe(&rdfd, &wrfd))
	{
		free(buf);
		return 1;
	}
	rc=_authdaemondo(wrfd, rdfd, buf, callback_func, callback_arg);
	free(buf);
	if (rc > 0) closePipe();
	return rc;
}


/* modelled on auth_getuserinfo() in preauthdaemon.c (but note first
 * two args are reversed) */
int auth_pipe_pre(const char *uid, const char *service,
		int (*callback)(struct authinfo *, void *),
		void *arg)
{
	char *buf;
	int rdfd, wrfd, rc;

	if (disabled_flag)
		return -1;

	buf=malloc(strlen(service)+strlen(uid)+20);
	if (!buf)
		return 1;
	
	strcat(strcat(strcat(strcat(strcpy(buf, "PRE . "), service), " "),
		uid), "\n");
	
	if (getPipe(&rdfd, &wrfd))
	{
		free(buf);
		return 1;
	}
	rc=_authdaemondo(wrfd, rdfd, buf, callback, arg);
	free(buf);
	if (rc > 0) closePipe();
	return (rc);
}


/* modelled on auth_passwd() in authmoduser2.c */
int auth_pipe_chgpwd(const char *service,
		const char *uid,
		const char *opwd,
		const char *npwd)
{
	char *buf;
	int rdfd, wrfd, rc;

	if (disabled_flag)
		return -1;

	buf=malloc(strlen(service)+strlen(uid)+strlen(opwd)+
			strlen(npwd)+20);
	if (!buf)
		return 1;

	sprintf(buf, "PASSWD %s\t%s\t%s\t%s\n",
		service, uid, opwd, npwd);

	if (getPipe(&rdfd, &wrfd))
	{
		free(buf);
		return 1;
	}
	rc = _authdaemondopasswd(wrfd, rdfd, buf, strlen(buf));
	free(buf);
	if (rc > 0) closePipe();
	return (rc);
}


void auth_pipe_idle()
{
	/* don't need to do anything when idle */
}


void auth_pipe_close()
{
	closePipe();
}

void auth_pipe_enumerate(void(*cb_func)(const char *name,
	                                      uid_t uid,
	                                      gid_t gid,
	                                const char *homedir,
	                                const char *maildir,
					const char *options,
	                                      void *void_arg),
	                         void *void_arg)
{
	int rdfd, wrfd, rc;

	if (disabled_flag)
		return;

	if (getPipe(&rdfd, &wrfd))
		return;
	rc = _auth_enumerate(wrfd, rdfd, cb_func, void_arg);
	if (rc > 0) closePipe();
}

static struct authstaticinfo authpipe_info={
	"authpipe",
	auth_pipe,
	auth_pipe_pre,
	auth_pipe_close,
	auth_pipe_chgpwd,
	auth_pipe_idle,
	auth_pipe_enumerate};

struct authstaticinfo *courier_authpipe_init()
{
	disabled_flag=access(PIPE_PROGRAM, X_OK);
	if (disabled_flag)
	{
		DPRINTF("authpipe: disabled: failed to stat pipe program %s: %s",
			PIPE_PROGRAM, strerror(errno));
	}
	return &authpipe_info;
}
