/*
** Copyright 2000-2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"courier.h"
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if TIME_WITH_SYS_TIME
#include	<sys/time.h>
#include	<time.h>
#else
#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#else
#include	<time.h>
#endif
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<sys/socket.h>
#include	<sys/un.h>

#include	"libfilter.h"


int lf_init(const char *modfile,
		const char *allname,
		const char *alltmpname,

		const char *notallname,
		const char *notalltmpname)
{
int	all=0;
const	char *sockname, *tmpsockname;
struct sockaddr_un ssun;
int	listensock;

	if (chdir(courierdir()))
	{
		perror(courierdir());
		return (-1);
	}

	{
	char	*fn=config_localfilename(modfile);
	char	*f;

		f=config_read1l(fn);
		free(fn);
		if (f)
		{
			if (strcmp(f, "all") == 0)
				all=1;
			free(f);
		}
	}

	if (all)
	{
		sockname=allname;
		tmpsockname=alltmpname;
		unlink(notallname);
	}
	else
	{
		sockname=notallname;
		tmpsockname=notalltmpname;
		unlink(allname);
	}

	ssun.sun_family=AF_UNIX;
	strcpy(ssun.sun_path, tmpsockname);
	unlink(ssun.sun_path);
	if ((listensock=socket(PF_UNIX, SOCK_STREAM, 0)) < 0 ||
		bind(listensock, (struct sockaddr *)&ssun, sizeof(ssun)) < 0 ||
		listen(listensock, SOMAXCONN) < 0 ||
		chmod(ssun.sun_path, 0660) ||
		rename (tmpsockname, sockname) ||
		fcntl(listensock, F_SETFL, O_NDELAY) < 0
		)
	{
		perror("socket");
		return (-1);
	}
	return (listensock);
}

int lf_initializing()
{
	struct stat stat_buf;

	return fstat(3, &stat_buf) == 0;
}

void lf_init_completed(int sockfd)
{
	if (sockfd != 3)	close(3);
}

int lf_accept(int listensock)
{
struct sockaddr_un ssun;
fd_set fd0;
int	fd;
socklen_t	sunlen;

	for (;;)
	{
		FD_ZERO(&fd0);
		FD_SET(0, &fd0);
		FD_SET(listensock, &fd0);

		if (select(listensock+1, &fd0, 0, 0, 0) < 0)
		{
			perror("select");
			return (-1);
		}

		if (FD_ISSET(0, &fd0))
		{
		char    buf[16];

			if (read(0, buf, sizeof(buf)) <= 0)
				return (-1);	/* Shutting down */
		}

		if (!FD_ISSET(listensock, &fd0))	continue;

		sunlen=sizeof(ssun);
 		if ((fd=accept(listensock, (struct sockaddr *)&ssun, &sunlen))
			< 0)
                        continue;

		fcntl(fd, F_SETFL, 0);	/* Take out of NDELAY mode */
		break;
	}
	return (fd);
}
