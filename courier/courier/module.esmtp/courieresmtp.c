/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"moduledel.h"
#include	"maxlongsize.h"
#include	"comreadtime.h"
#include	<sys/types.h>
#include	<sys/uio.h>
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
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<signal.h>
#include	<errno.h>
#include	"waitlib/waitlib.h"
#include	"numlib/numlib.h"
#include	"mybuf.h"
#include	"esmtpconfig.h"
#include	"rfc1035/rfc1035.h"

static time_t	esmtpkeepalive;

struct esmtpchildinfo {
	pid_t	pid;		/* Just a marker */
	int	cmdpipe;	/* Pipe to the child process */
	char	*host;		/* Host the child process is connected to */
	int	isbusy;		/* The child process is delivering a message */
	char	*pendel;	/* Line that is queued up for this child
				** process to terminate, so we can start
				** another process to a different host
				*/
	time_t	termtime;
	} ;

/* A child process is always in one of the following states. */

#define	ESMTP_NOCHILD(i)	( (i)->pid < 0)
	/* No child process */

#define	ESMTP_BUSY(i)	(!ESMTP_NOCHILD(i) && (i)->cmdpipe >= 0 && (i)->isbusy)
	/* Busy deliverin a message */

#define	ESMTP_IDLE(i)	(!ESMTP_NOCHILD(i) && (i)->cmdpipe >= 0 &&!(i)->isbusy)
	/* Delivered a message, just idling by */

#define	ESMTP_TERMINATING(i)	(!ESMTP_NOCHILD(i) && (i)->cmdpipe < 0)
	/* Being shut down for inactivity, or in order to reuse the proc */


static struct	esmtpchildinfo *info;

static void start_child(unsigned, char *, char **);
static void send_child(unsigned, char *, char **);
static void terminated_child(unsigned, unsigned);

static int completionpipe[2];
static FILE *childresultpipe;

static struct	mybuf	courierdbuf, childbuf;
static void esmtpparent();
extern void esmtpchild(unsigned);

static int call_mybuf_get(void *p)
{
	return (mybuf_get( (struct mybuf *)p ));
}

static struct rfc1035_ifconf *interfaces=NULL;

int isloopback(const char *ip)
{
	struct rfc1035_ifconf *p;

	for (p=interfaces; p; p=p->next)
	{
		char buf[RFC1035_NTOABUFSIZE];

		rfc1035_ntoa(&p->ifaddr, buf);
		if (strcmp(buf, ip) == 0)
			return (1);
	}
	return (0);
}

int main(int argc, char **argv)
{
        clog_open_syslog("courieresmtp");
	esmtpkeepalive=config_time_esmtpkeepalive();

	if (argc < 2)
	{
		esmtpparent();
	}
	else
	{
		interfaces=rfc1035_ifconf(NULL);
		esmtpchild(atoi(argv[1]));
	}
	return (0);
}

/*
** Responses from child processes are of the form "index pid".
** We only need "index", but we also check "pid" in order to catch a rather
** rare race condition: child process notifies of a delivery completion,
** immediately crashes, we reap him before we are notified of the delivery
** completion, then restart another process in the same slot, then get
** the original delivery completion message - oops.
*/

static int parse_ack(const char *msg, unsigned *index, pid_t *pid)
{
	*index=0;
	*pid=0;

	while (*msg != ' ')
	{
		if (*msg < '0' || *msg > '9')	return (-1);
		*index = *index * 10 + (*msg++ - '0');
	}
	++msg;
	while (*msg)
	{
		if (*msg < '0' || *msg > '9')	return (-1);
		*pid = *pid * 10 + (*msg++ - '0');
	}
	return (0);
}

static void esmtpparent()
{
unsigned i;
fd_set	fdc, fds;
time_t	current_time;

	libmail_changeuidgid(MAILUID, MAILGID);
	module_init(&terminated_child);

	if ((info=(struct esmtpchildinfo *)malloc(sizeof(*info)*
		module_nchildren)) == 0)
		clog_msg_errno();
	for (i=0; i<module_nchildren; i++)
	{
		info[i].pid= -1;
		info[i].cmdpipe= -1;
		info[i].host=0;
		info[i].pendel=0;
	}
	if (pipe(completionpipe) < 0)
		clog_msg_errno();

	if ((childresultpipe=fdopen(completionpipe[0], "r")) == 0)
		clog_msg_errno();
	FD_ZERO(&fdc);
	FD_SET(0, &fdc);
	FD_SET(completionpipe[0], &fdc);
	mybuf_init(&courierdbuf, 0);
	mybuf_init(&childbuf, completionpipe[0]);

	module_blockset();
	time(&current_time);

	for (;;)
	{
	time_t	wait_time;
	struct	timeval	tv;

		wait_time=0;
		for (i=0; i<module_nchildren; i++)
		{
			if (!ESMTP_IDLE(&info[i]))	continue;
			if (info[i].termtime <= current_time)
			{
				close(info[i].cmdpipe);
				info[i].cmdpipe= -1;
				continue;
			}

			if (wait_time == 0 || info[i].termtime < wait_time)
				wait_time=info[i].termtime;
		}

		if (wait_time)
		{
			tv.tv_sec= wait_time - current_time;
			tv.tv_usec=0;
		}

		fds=fdc;

		module_blockclr();
                while (select(completionpipe[0]+1, &fds, (fd_set *)0, (fd_set *)0,
                                (wait_time ? &tv:(struct timeval *)0)) < 0)
                {
                        if (errno != EINTR)     clog_msg_errno();
                }

		module_blockset();
		time(&current_time);

		if (FD_ISSET(completionpipe[0], &fds))
		{
		char	*line;

			do
			{
			pid_t	p;

				line=module_getline( &call_mybuf_get,
						&childbuf);

				if (parse_ack(line, &i, &p) ||
					i >= module_nchildren ||
					(p == info[i].pid &&
						!ESMTP_BUSY(&info[i])))
				{
					clog_msg_start_err();
					clog_msg_str("INVALID message from child process.");
					clog_msg_send();
					_exit(0);
				}
				if (p != info[i].pid)	continue;
				info[i].isbusy=0;
				info[i].termtime=current_time + esmtpkeepalive;
				if (info[i].pendel)
				{
					free(info[i].pendel);
					info[i].pendel=0;
				}

				module_completed(i, module_delids[i]);
			} while (mybuf_more(&childbuf));
		}

		if (!FD_ISSET(0, &fds))	continue;

		do
		{
		char	**cols;
		const char *hostp;
		size_t	hostplen;
		time_t	misctime;
		unsigned j;
		char	*line;

			line=module_getline( &call_mybuf_get, &courierdbuf);
			if (!line)
			{
				module_restore();

				/*
				** If all processes are idle, wait for them
				** to finish normally.  Otherwise, kill
				** the processes.
				*/

				for (j=0; j<module_nchildren; j++)
					if (ESMTP_BUSY(&info[j]))
						break;

				if (j < module_nchildren)
				{
					for (j=0; j<module_nchildren; j++)
						if (info[j].pid > 0)
							kill(info[j].pid,
								SIGTERM);
				}
				else
				{
				int	waitstat;

					for (j=0; j<module_nchildren; j++)
					{
						if (info[j].cmdpipe > 0)
						{
							close(info[j].cmdpipe);
							info[j].cmdpipe= -1;
						}
					}
					while (wait(&waitstat) != -1 ||
						errno == EINTR)
						;
				}
				_exit(0);
			}

			cols=module_parsecols(line);

			if (!cols)	_exit(0);

			hostp=MODULEDEL_HOST(cols);
			for (hostplen=0; hostp[hostplen] &&
				hostp[hostplen] != '\t'; hostplen++)
				;

			for (i=0; i<module_nchildren; i++)
			{
				if (!ESMTP_IDLE(&info[i])) continue;
				if (memcmp(info[i].host, hostp, hostplen) == 0
					&& info[i].host[hostplen] == 0)
					break;
			}

			if (i < module_nchildren)	/* Reuse a process */
			{
				send_child(i, line, cols);
				continue;
			}

			for (i=0; i<module_nchildren; i++)
				if (ESMTP_NOCHILD(&info[i]))	break;

			if (i < module_nchildren)	/* We can fork */
			{
				start_child(i, line, cols);
				send_child(i, line, cols);
				continue;
			}

			/*
			** Find a process that's been idled the longest,
			** and reuse that one.
			*/

			misctime=0;
			j=0;
			for (i=0; i<module_nchildren; i++)
			{
				if (ESMTP_IDLE(&info[i]) &&
					(misctime == 0 || misctime >
						info[i].termtime))
				{
					j=i;
					misctime=info[i].termtime;
				}
			}
			if (misctime)
			{
				if (info[j].pendel)
				{
					clog_msg_start_err();
					clog_msg_str("INTERNAL ERROR: unexpected scheduled delivery.");
					clog_msg_send();
					_exit(1);
				}

				info[j].pendel=strcpy(
					courier_malloc(strlen(line)+1),
					line);
				close(info[j].cmdpipe);
				info[j].cmdpipe= -1;
				continue;
			}

			/* The ONLY remaining possibility is something in
			** the TERMINATING stage, without another delivery
			** already scheduled for that slot.
			*/

			for (i=0; i<module_nchildren; i++)
			{
				if (ESMTP_TERMINATING(&info[i]) &&
					info[i].pendel == 0)
					break;
			}

			if (i < module_nchildren)
			{
				info[i].pendel=strcpy(
					courier_malloc(strlen(line)+1),
					line);
				continue;
			}

			clog_msg_start_err();
			clog_msg_str("INTERNAL ERROR: unexpected delivery.");
			clog_msg_send();
			_exit(1);
		} while (mybuf_more(&courierdbuf));
	}
}

static void terminated_child(unsigned idx, unsigned delid)
{
	if (ESMTP_TERMINATING(&info[idx]))
	{
	char	*p;

		if ((p=info[idx].pendel) != 0)
		{
		char **cols=module_parsecols(info[idx].pendel);

			/*
			** Clear to 0 to prevent infinite loop if fork fails
			** in start_child.
			*/
			info[idx].pendel=0;
			start_child(idx, info[idx].pendel, cols);
			info[idx].pendel=p;
			send_child(idx, info[idx].pendel, cols);
			return;
		}

		info[idx].pid= -1;
		return;
	}

	/* Oops, something crashed.  Clean it up */

	clog_msg_start_err();
	if (info[idx].pid < 0)
	{
		clog_msg_str("Unable to fork");
	}
	else
	{
		clog_msg_str("Crashed child process ");
		clog_msg_uint(info[idx].pid);
	}

	if (ESMTP_BUSY(&info[idx]))
	{
		clog_msg_str(", while delivering to ");
		clog_msg_str(info[idx].host);
		module_completed(idx, delid);
	}
	clog_msg_send();
	close(info[idx].cmdpipe);
	info[idx].cmdpipe= -1;
	info[idx].pid= -1;
}

static void start_child(unsigned i, char *line, char **cols)
{
int	pipebuf[2];
const char *hostp;
size_t	hostplen;
pid_t	pid;

	hostp=MODULEDEL_HOST(cols);
	for (hostplen=0; hostp[hostplen] &&
		hostp[hostplen] != '\t'; hostplen++)
		;

	if (info[i].host)	free(info[i].host);
	memcpy(info[i].host=courier_malloc(hostplen+1), hostp, hostplen);
	info[i].host[hostplen]=0;

	if (pipe(pipebuf) < 0)	clog_msg_errno();

	pid=module_fork_noblock(0, &i);

	if (pid == 0)
	{
	unsigned	j;
	char	buf[MAXLONGSIZE+1];
	char *p;

		dup2(pipebuf[0], 0);
		close(pipebuf[0]);
		close(pipebuf[1]);
		fclose(childresultpipe);
		dup2(completionpipe[1], 1);
		close(completionpipe[1]);
		close(completionpipe[0]);
		for (j=0; j<module_nchildren; j++)
			if (info[j].cmdpipe >= 0)
				close(info[j].cmdpipe);

		for (j=0; MODULEDEL_HOST(cols)[j]; j++)
			if (MODULEDEL_HOST(cols)[j] == '\t')
			{
				MODULEDEL_HOST(cols)[j]=0;
				break;
			}

		p=buf+MAXLONGSIZE;
		*p=0;
		do
		{
			* --p = '0' + (i % 10);
		} while ( (i=i / 10) != 0);

		execl("courieresmtp", "courieresmtp", p,
			MODULEDEL_HOST(cols), (char *)0);
		clog_msg_errno();
		_exit(1);
	}
	info[i].pid=pid;
	close (pipebuf[0]);
	info[i].cmdpipe=pipebuf[1];
}

static void send_child(unsigned i, char *line, char **cols)
{
const char *p;
struct	iovec	iov[2];
size_t	l=strlen(line);

	module_delids[i]=0;

	for (p=MODULEDEL_DELID(cols); *p >= '0' && *p <= '9'; p++)
		module_delids[i] = module_delids[i] * 10 + (*p - '0');

	if (info[i].pid < 0)	/* Failure in start_child */
	{
		terminated_child(i, module_delids[i]);
		return;
	}

        iov[0].iov_base=(caddr_t)line;
        iov[0].iov_len=l;
	iov[1].iov_base="\n";
	iov[1].iov_len=1;

	if (writev(info[i].cmdpipe, iov, 2) != l+1)
	{
		clog_msg_prerrno();
		/* This is usually because the process has terminated,
		** but we haven't cleaned this up yet, but just make sure.
		*/
		kill(info[i].pid, SIGKILL);
	}
	info[i].isbusy=1;
}
