/*
** Copyright 2000-2004 Double Precision, Inc.  See COPYING for
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
#include	"authchangepwdir.h"


extern int authdaemondopasswd(char *, int);

static int badstr(const char *p)
{
	if (!p) return 1;
	while (*p)
	{
		if ((int)(unsigned char)*p < ' ')
			return 1;
		++p;
	}
	return 0;
}

int auth_passwd(const char *service,
		const char *uid,
		const char *opwd,
		const char *npwd)
{
	char *buf;

	if (badstr(service) || badstr(uid) || badstr(opwd) || badstr(npwd))
	{
		errno=EINVAL;
		return -1;
	}

	buf=malloc(strlen(service)+strlen(uid)+strlen(opwd)+
			 strlen(npwd)+20);

	if (!buf)
		return -1;

	sprintf(buf, "PASSWD %s\t%s\t%s\t%s\n",
		service, uid, opwd, npwd);

	if (authdaemondopasswd(buf, strlen(buf)))
	{
		free(buf);
		/*sleep(5);*/
		return (-1);
	}
	free(buf);
	return (0);
}
