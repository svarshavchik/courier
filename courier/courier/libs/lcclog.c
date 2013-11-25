/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_lib_config.h"
#endif

#include	"courier.h"
#include	<sys/types.h>
#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<errno.h>
#include	<grp.h>
#if	HAVE_UNISTD_H
#include	"unistd.h"
#endif
#include	"maxlongsize.h"
#include	<sys/types.h>
#include	<sys/uio.h>
#if	HAVE_SYSLOG_H
#include	<syslog.h>
#endif


static int log_opened=0;
static char logbuf[1024];
static const char *logmodule;

static void clog_msg_send_stderr();
#if	HAVE_SYSLOG_H
static void clog_msg_send_syslog();
#endif

static void (*clog_msg_send_func)();

void clog_open_syslog(const char *module)
{
#if	HAVE_SYSLOG_H

	logmodule=module;
	clog_msg_send_func=clog_msg_send_syslog;
	openlog(module, 0
#ifdef  LOG_NDELAY
			| LOG_NDELAY
#else
			| LOG_NOWAIT
#endif
			, LOG_MAIL);

#else
	clog_open_stderr(module);
#endif
	log_opened=1;
}

void clog_open_stderr(const char *module)
{
#ifdef HAVE_SETVBUF_IOLBF
	setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
#endif

	log_opened=1;
	logmodule=module;
	clog_msg_send_func=clog_msg_send_stderr;
}

void clog_msg_start_info()
{
	if (!log_opened)	return;
	strcpy(logbuf, "INFO: ");
}

void clog_msg_start_err()
{
	if (!log_opened)
		clog_open_stderr("init");
	strcpy(logbuf, "ERR: ");
}

void clog_msg_send()
{
	if (!log_opened)	return;

	(*clog_msg_send_func)();
}

static void clog_msg_send_stderr()
{
struct	iovec	iov[4];
int	niov=0;

	if (logmodule)
	{
		iov[niov].iov_base=(caddr_t)logmodule;
		iov[niov++].iov_len=strlen(logmodule);
		iov[niov].iov_base=(caddr_t)": ";
		iov[niov++].iov_len=2;
	}

	iov[niov].iov_base=(caddr_t)logbuf;
	iov[niov++].iov_len=strlen(logbuf);
	iov[niov].iov_base=(caddr_t)"\n";
	iov[niov++].iov_len=1;
	if (writev(2, iov, niov) < 0)
		;
}

#if	HAVE_SYSLOG_H
static void clog_msg_send_syslog()
{
const char *p=logbuf;
int	loglevel=LOG_INFO;

	if (*p == 'E')	loglevel=LOG_ERR;
	while (*p && *p != ':')
		p++;
	if (*p)	p++;
	while (*p == ' ')	++p;

	syslog(loglevel, "%s", p);
}
#endif

void clog_msg_str(const char *p)
{
int	n, l;
int	c;
int	trunc=0;

	if (!p || !log_opened)	return;
	n=strlen(p);
	l=strlen(logbuf);
	if (n > 128)
	{
		n=128;
		trunc=1;
	}

	if (n+l >= sizeof(logbuf)-1)
		n=sizeof(logbuf)-1-l;

	while (n)
	{
		c= *p++;
		if ( c == '\r')	continue;
		if ( c == '\n' && *p == 0)	break;

		if ( c == '\n')	c='/';
		else if (c < ' ' || c >= 0x7E)	c='.';
		logbuf[l++]=c;
		--n;
	}
	logbuf[l]=0;
	if (trunc)	clog_msg_str("...");
}

void clog_msg_ulong(unsigned long n)
{
char	buf[MAXLONGSIZE+2];

	if (!log_opened)	return;
	sprintf(buf, "%lu", n);
	clog_msg_str(buf);
}

void clog_msg_uint(unsigned n)
{
	clog_msg_ulong(n);
}


void clog_msg_int(int n)
{
	if (n < 0)
	{
		n= -n;
		clog_msg_str("-");
	}
	clog_msg_uint(n);
}

void clog_msg_prerrno()
{
	clog_msg_start_err();
#if	HAVE_STRERROR
	clog_msg_str(strerror(errno));
#else
	{
	char	buf[MAXLONGSIZE+40];

		sprintf(buf, "System error, errno=%d", (int)errno);
		clog_msg_str(buf);
	}
#endif
	clog_msg_send();
}

void clog_msg_errno()
{
	clog_msg_prerrno();
	exit(1);
}
