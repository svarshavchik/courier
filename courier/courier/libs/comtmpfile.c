/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"maxlongsize.h"
#if	HAVE_SYS_TYPES_H
#include	<sys/types.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<time.h>


static char tbuf[MAXLONGSIZE], pbuf[MAXLONGSIZE];
static const char *hostnameptr=0;
static char *timeptr=0, *pidptr=0;

void	gettmpfilenameargs(const char **time_buf,
	const char **pid_buf, const char **hostname_buf)
{
	if (!hostnameptr)
	{
	time_t	t;
	pid_t	p;

		time(&t);
		timeptr=tbuf+sizeof(tbuf)-1;
		*timeptr=0;
		do
		{
			*--timeptr= '0' + (t % 10);
			t /= 10;
		} while (t);

		p=getpid();
		pidptr=pbuf+sizeof(pbuf)-1;
		*pidptr=0;
		do
		{
			*--pidptr= '0' + (p % 10);
			p /= 10;
		} while (p);
		hostnameptr=config_me();
	}
	*time_buf=timeptr;
	*pid_buf=pidptr;
	*hostname_buf=hostnameptr;
}

void	getnewtmpfilenameargs(const char **time_buf,
	const char **pid_buf, const char **hostname_buf)
{
	hostnameptr=0;
	gettmpfilenameargs(time_buf, pid_buf, hostname_buf);
}
