/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"moduledel.h"
#include	"courier.h"
#include	"maxlongsize.h"
#include	"waitlib/waitlib.h"
#include	<stdlib.h>
#include	<stdio.h>
#include	<errno.h>
#include	<time.h>
#include	<signal.h>

#include	<sys/types.h>
#if TIME_WITH_SYS_TIME
#include        <sys/time.h>
#include        <time.h>
#else
#if HAVE_SYS_TIME_H
#include        <sys/time.h>
#else
#include        <time.h>
#endif
#endif
#if	HAVE_SYS_SELECT_H
#include	<sys/select.h>
#endif

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

static char *linebuf=0;
static size_t linebufsize=0;
static char **cols=0;
static size_t colcnt=0;

unsigned module_nchildren;
unsigned *module_delids;
pid_t *module_pids;
time_t *module_timeout;
int *module_sig;
static time_t delivery_timeout=0;

char	**module_parsecols(char *p)
{
unsigned l;
size_t	ncols=1;
size_t	n;

	for (l=0; p[l]; l++)
		if (p[l] == '\t')	++ncols;

	if (ncols >= colcnt)
	{
		colcnt=ncols+10;
		cols=(char **)(cols ? realloc(cols, colcnt*sizeof(char *))
				: malloc(colcnt*sizeof(char *)));
	}
	ncols=1;
	cols[0]=linebuf;
	for (n=0; p[n]; n++)
		if (p[n] == '\t')
			cols[ncols++]=p+n+1;
	cols[ncols]=0;
	if (ncols < 6)	return (0);
	return (cols);
}

struct moduledel *module_parsedel(char **cols)
{
size_t	n;
const char *cp;
static struct moduledel msginfo;

	for (n=1; cols[n]; n++)
		cols[n][-1]=0;

	cp=cols[0];
	msginfo.inum=0;
	while (*cp >= '0' && *cp <= '9')
		msginfo.inum = msginfo.inum*10 + (*cp++ - '0');
	msginfo.msgid=cols[1];

	msginfo.sender=cols[2];
	msginfo.delid=cols[3];
	msginfo.host=cols[4];
	msginfo.receipients=&cols[5];
	msginfo.nreceipients=(n - 5)/2;
	return (&msginfo);
}

char *module_getline( int (*get_func)(void *), void *ptr)
{
size_t	n;
int	c;

	for (n=0; ; n++)
	{
		do
		{
			errno=0;
			c= (*get_func)(ptr);
		} while (c == EOF && errno == EINTR);

		if (n >= linebufsize)
		{
			if ((linebuf=(char *)
				(linebufsize ? realloc(linebuf,
					linebufsize += 256):
					malloc(linebufsize += 256))) == 0)
				clog_msg_errno();
		}

		if (c == '\n' || c == EOF)	break;

		linebuf[n]=c;
	}
	linebuf[n]=0;
	if (c == EOF && n == 0)	return (0);
	return (linebuf);
}

static int get_stdin(void *dummy)
{
	static char stdin_buf[256];
	static char *stdin_p=NULL;
	static size_t stdin_left=0;

	while (stdin_left == 0)
	{
		time_t currentTime=time(NULL);
		time_t nextTime=0;
		size_t i;
		fd_set fds;
		struct timeval tv;

		if (delivery_timeout)
			for (i=0; i<module_nchildren; i++)
			{
				size_t n;

				if (module_pids[i] < 0)
					continue;

				if (module_timeout[i] <= currentTime)
				{
					clog_msg_start_err();
					clog_msg_str("Error: stuck delivery,"
						     " PID ");
					clog_msg_uint(module_pids[i]);
					clog_msg_str(", sending signal ");
					clog_msg_uint(module_sig[i]);
					clog_msg_send();

					module_timeout[i]=currentTime+10;
					kill(-module_pids[i], module_sig[i]);
					module_sig[i]=SIGKILL;
					continue;
				}

				n=module_timeout[i] - currentTime;

				if (nextTime == 0 || n < nextTime)
					nextTime=n;
			}

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		tv.tv_sec=nextTime;
		tv.tv_usec=0;

		if (select(1, &fds, NULL, NULL, nextTime ? &tv:NULL) > 0)
		{
			int n=read(0, stdin_buf, sizeof(stdin_buf));

			if (n <= 0)
				break;
			
			stdin_p=stdin_buf;
			stdin_left=n;
		}
	}

	if (stdin_left == 0)
		return EOF;

	--stdin_left;
	return (int)(unsigned char)*stdin_p++;
}

struct moduledel *module_getdel()
{
char	**cols;
char	*line=module_getline( &get_stdin, 0);

	if (!line)	return (0);

	if ((cols=module_parsecols(linebuf)) == 0)	return (0);
	return (module_parsedel(cols));
}

void module_completed(unsigned i, unsigned n)
{
char	buf[MAXLONGSIZE+1];
char	*p;
unsigned c;

	i=i;
	p=buf+MAXLONGSIZE-1;
	*p='\n';
	p[1]=0;
	c=1;
	do
	{
		*--p= (n % 10) + '0';
		++c;
	} while ((n=n / 10) != 0);
	if (write (1, p, c) != c)
		;
}

static void (*childfunc)(unsigned, unsigned);

static void terminated(pid_t p, int s)
{
int	n;

	for (n=0; n<module_nchildren; n++)
		if (module_pids[n] == p)
		{
			module_pids[n]= -1;
			(*childfunc)(n, module_delids[n]);
			break;
		}
}

static RETSIGTYPE childsig(int);

void module_blockset()
{
	wait_block();
}

void module_blockclr()
{
	wait_clear(childsig);
}

void module_restore()
{
	wait_restore();
}

static RETSIGTYPE childsig(int n)
{
	n=n;

	wait_reap(terminated, childsig);

#if	RETSIGTYPE != void
	return (0);
#endif
}

void module_init(void (*func)(unsigned, unsigned))
{
const	char *p=getenv("MAXDELS");
unsigned n;

	if (!p)	p="0";

	if (!func)
		func= &module_completed;

	module_nchildren=atol(p);
	if (module_nchildren <= 0)	module_nchildren=1;

	if ((module_pids=malloc(module_nchildren*sizeof(*module_pids))) == 0 ||
	    (module_delids=malloc(module_nchildren*sizeof(*module_delids))) == 0 ||
	    (module_timeout=malloc(module_nchildren*sizeof(*module_timeout)))
	    == 0 ||
	    (module_sig=malloc(module_nchildren*sizeof(*module_sig))) == 0)
		clog_msg_errno();

	for (n=0; n<module_nchildren; n++)
		module_pids[n]= -1;

	childfunc=func;
	signal(SIGCHLD, childsig);
}

static pid_t module_fork_common(unsigned delid, unsigned *slotptr,
	int dorelease)
{
pid_t	pid;
unsigned n;
static unsigned idx=0;

	if (slotptr)
		idx= *slotptr;
	else
	{
		for (n=0; n<module_nchildren; n++)
		{
			if (module_pids[idx] < 0)	break;
			if (++idx >= module_nchildren)	idx=0;
		}

		if (n == module_nchildren)
		{
			clog_msg_start_err();
			clog_msg_str(
			"Internal error - no available process slots.");
			clog_msg_send();
			exit(1);
		}
	}

	if ((pid=fork()) == -1)
		return (pid);

	if (pid)
	{
		time_t n=0;

		module_pids[idx]=pid;
		module_delids[idx]=delid;

		if (delivery_timeout)
		{
			time(&n);
			n += delivery_timeout;
		}
		module_timeout[idx] = n;
		module_sig[idx] = SIGTERM;
	}
	else
	{
		module_restore();
	}

	return (pid);
}

void module_delivery_timeout(time_t n)
{
	delivery_timeout=n;
}

pid_t module_fork_noblock(unsigned delid, unsigned *slotptr)
{
	return (module_fork_common(delid, slotptr, 0));
}

pid_t module_fork(unsigned delid, unsigned *slotptr)
{
pid_t	pid;

	module_blockset();

	pid=module_fork_common(delid, slotptr, 1);

	if (pid)
		module_blockclr();
	return (pid);
}

void module_signal(int signum)
{
	unsigned n;

	module_blockset();

	for (n=0; n<module_nchildren; n++)
		if (module_pids[n] > 0)
		{
			kill(module_pids[n], signum);
		}
	module_blockclr();
}
