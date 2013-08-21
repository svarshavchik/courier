/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<stdio.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/uio.h>
#include	<sys/time.h>
#include	<signal.h>
#include	<errno.h>

#include	"esmtpiov.h"

time_t iovread_timeout=300;
time_t iovwrite_timeout=60;
static char inputbuf[5120];
static char *inputbufhead=inputbuf, *inputbuftail=inputbuf;
static int overflowed=0;
static struct iovec iov[10];
static int	niovec=0;

void iovreset()
{
	inputbufhead=inputbuftail=inputbuf;
}

void iovflush()
{
struct iovec *iovp=iov;
unsigned niovp=niovec;
int	n;
fd_set	fds;
struct	timeval	tv;

	while (niovp && niovec)
	{
		FD_ZERO(&fds);
		FD_SET(1, &fds);
		tv.tv_sec=iovwrite_timeout;
		tv.tv_usec=0;
		if (select(2, 0, &fds, 0, &tv) <= 0 ||
			!FD_ISSET(1, &fds) ||
			(n=writev(1, iov, niovec)) <= 0)
		{
			iov_logerror("writev: ",
#if HAVE_STRERROR
				     strerror(errno)

#else
				     "write error"
#endif
				     );
			exit(1);
		}

		while (n && niovp)
		{
			if (n >= iovp->iov_len)
			{
				n -= iovp->iov_len;
				++iovp;
				--niovp;
				continue;
			}
			iovp->iov_base=(caddr_t)
				( (char *)iovp->iov_base + n);
			iovp->iov_len -= n;
			break;
		}
	}
	niovec=0;
}

int iovwaitfordata(time_t *t, int abort)
{
time_t	now;

	time(&now);
	if (now < *t)
	{
	fd_set	fds;
	struct	timeval	tv;

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		tv.tv_sec=*t - now;
		tv.tv_usec=0;
		if (select(1, &fds, 0, 0, &tv) > 0 &&
			FD_ISSET(0, &fds))
			return (0);
	}

	errno=ETIMEDOUT;

	{
#if HAVE_STRERROR
		const char *ip=getenv("TCPREMOTEIP");

		if (ip && *ip)
			fprintf(stderr, "[%s]: %s\n", ip,
				strerror(errno));
		else
#endif
			perror("read");
	}
	if (abort)
		exit(1);
	return (-1);
}

char *iovreadline()
{
char *p=inputbufhead;
time_t	t;

	time(&t);
	t += iovread_timeout;
	iovflush();
	for (;;)
	{
		if (p >= inputbuftail)
		{
		int	n;

			if (inputbufhead == inputbuftail)
			{
				inputbufhead=inputbuftail=inputbuf;
				iovwaitfordata(&t, 1);
				n=read(0, inputbuf, sizeof(inputbuf));
				if (n <= 0)	_exit(0);
				inputbuftail=inputbuf+n;
				p=inputbuf;
				continue;
			}
			if (inputbufhead > inputbuf)
			{
				n=0;
				while (inputbufhead < inputbuftail)
					inputbuf[n++]= *inputbufhead++;
				inputbuftail=p=inputbuf+n;
				inputbufhead=inputbuf;
			}
			if (inputbuftail >= inputbuf+sizeof(inputbuf))
			{
				if (overflowed)	/* Already overflowed once */
				{
					inputbuf[0]=inputbuftail[-1];
					inputbufhead=inputbuf;
					inputbuftail=inputbufhead+1;
					p=inputbuftail;
					continue;
				}
				overflowed=1;
				inputbuftail[-2]=0;
				inputbufhead=inputbuftail-1;
				return (inputbuf);
			}
			iovwaitfordata(&t, 1);
			n=read(0, inputbuftail, inputbuf+sizeof(inputbuf)-
					inputbuftail);
			if (n <= 0)	_exit(0);
			inputbuftail += n;
		}

		if (*p == '\n' && p > inputbufhead && p[-1] == '\r')
		{
		char *q=inputbufhead;

			p[-1]=0;
			*p++=0;
			inputbufhead=p;
			if (overflowed)
			{
				overflowed=0;
				continue;
			}
			return (q);
		}
		++p;
	}
}

void addiovec(const char *p, size_t l)
{
	iov[niovec].iov_base=(caddr_t)p;
	iov[niovec].iov_len=l;
	if (++niovec >= sizeof(iov)/sizeof(iov[0]))
		iovflush();
}
