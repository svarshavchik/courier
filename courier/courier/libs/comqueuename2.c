/*
** Copyright 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_lib_config.h"
#endif
#include	"courier.h"
#include	"numlib/numlib.h"
#include	"comqueuename.h"
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<time.h>

static int bigbrother_create_rename(const char *, char *);

void qmsgunlink(ino_t n, time_t rightnow)
{
	const char *bigbrother=getenv("ARCHIVEDIR");

	if (bigbrother && *bigbrother)
	{
		struct tm *tmptr=localtime(&rightnow);

		/* Archive this message */

		if (tmptr)
		{
			const char *dname=qmsgsdatname(n);
			const char *tname=strrchr(dname, '/');

			char *p=courier_malloc(strlen(bigbrother)+
					       strlen(tname)+40);

			sprintf(p, "%s/%04d.%02d.%02d%s",
				bigbrother, tmptr->tm_year + 1900,
				tmptr->tm_mon+1,
				tmptr->tm_mday, tname);

			if (bigbrother_create_rename(dname, p) == 0)
			{
				dname=qmsgsctlname(n);

				strcpy(strrchr(p, '/'), strrchr(dname, '/'));

				if (rename(dname, p) == 0)
				{
					free(p);
					return;
				}
			}

			clog_msg_start_err();
			clog_msg_str("Unable to move ");
			clog_msg_str(dname);
			clog_msg_str(" to ");
			clog_msg_str(p);
			clog_msg_send();
			clog_msg_prerrno();

			free(p);
		}
	}

	unlink(qmsgsdatname(n));
	unlink(qmsgsctlname(n));
}

static int bigbrother_create_rename(const char *from, char *to)
{
	char *p;

	if (rename(from, to) == 0)
		return (0);

	/* Perhaps the directory has to be created? */

	p=strrchr(to, '/');

	*p=0;
	mkdir(to, 0700);
	*p='/';
	return (rename(from, to));
}
