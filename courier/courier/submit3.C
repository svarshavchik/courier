/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<sys/types.h>
#include	<time.h>
#include	<stdio.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

#include	<string>

extern	time_t	submit_time;

std::string	mkmessageidheader()
{
	char	timebuf[sizeof(time_t)*2+1], pidbuf[sizeof(pid_t)*2+1];
	static const char hex[]="0123456789ABCDEF";

	timebuf[sizeof(timebuf)-1]=0;
	pidbuf[sizeof(pidbuf)-1]=0;

	pid_t	p=getpid();
	int	i;

	for (i=sizeof(pidbuf)-1; i; )
	{
		pidbuf[--i]= hex[p % 16];
		p /= 16;
	}

	time_t	t=submit_time;

	for (i=sizeof(timebuf)-1; i; )
	{
		timebuf[--i]= hex[t % 16];
		t /= 16;
	}

	std::string hdrbuf("Message-ID: <courier.");

	hdrbuf += timebuf;
	hdrbuf += '.';
	hdrbuf += pidbuf;
	hdrbuf += '@';
	hdrbuf += config_msgidhost();
	hdrbuf += ">\n";
	return (hdrbuf);
}
