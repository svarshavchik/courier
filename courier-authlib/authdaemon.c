/*
** Copyright 2000-2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthsasl.h"
#include	"authwait.h"
#include	"courierauthdebug.h"
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<signal.h>
#include	<unistd.h>
#include	<errno.h>
#include	<sys/time.h>
#include	<sys/select.h>
#include	"numlib/numlib.h"


extern int authdaemondo(const char *authreq,
	int (*func)(struct authinfo *, void *), void *arg);

extern void auth_daemon_enumerate( void(*cb_func)(const char *name,
						  uid_t uid,
						  gid_t gid,
						  const char *homedir,
						  const char *maildir,
						  const char *options,
						  void *void_arg),
				   void *void_arg);


int auth_generic(const char *service,
		 const char *authtype,
		 char *authdata,
		 int (*callback_func)(struct authinfo *, void *),
		 void *callback_arg)
{
	char	tbuf[NUMBUFSIZE];
	size_t	l=strlen(service)+strlen(authtype)+strlen(authdata)+2;
	char	*n=libmail_str_size_t(l, tbuf);
	char	*buf=malloc(strlen(n)+l+20);
	int	rc;

	courier_authdebug_login_init();

	if (!buf)
		return 1;

	strcat(strcat(strcpy(buf, "AUTH "), n), "\n");
	strcat(strcat(buf, service), "\n");
	strcat(strcat(buf, authtype), "\n");
	strcat(buf, authdata);

	rc=strcmp(authtype, "EXTERNAL") == 0
		? auth_getuserinfo(service, authdata, callback_func,
				   callback_arg)
		: authdaemondo(buf, callback_func, callback_arg);
	free(buf);

	if (courier_authdebug_login_level)
	{
	struct timeval t;

		/* short delay to try and allow authdaemond's courierlogger
		   to finish writing; otherwise items can appear out of order */
		t.tv_sec = 0;
		t.tv_usec = 100000;
		select(0, 0, 0, 0, &t);
	}

	return rc;
}

int auth_callback_default(struct authinfo *ainfo)
{
	if (ainfo->address == NULL)
	{
		fprintf(stderr, "WARN: No address!!\n");
		return (-1);
	}

	if (ainfo->sysusername)
		libmail_changeusername(ainfo->sysusername,
				       &ainfo->sysgroupid);
	else if (ainfo->sysuserid)
		libmail_changeuidgid(*ainfo->sysuserid,
				     ainfo->sysgroupid);
	else
	{
		fprintf(stderr, "WARN: %s: No UID/GID!!\n", ainfo->address);
		return (-1);
	}

	if (!ainfo->homedir)
	{
		errno=EINVAL;
		fprintf(stderr, "WARN: %s: No homedir!!\n", ainfo->address);
		return (1);
	}

	if (chdir(ainfo->homedir))
	{
		fprintf(stderr, "WARN: %s: chdir(%s) failed!!\n",
			ainfo->address, ainfo->homedir);
		perror("WARN: error");
		return (1);
	}

	return 0;
}
