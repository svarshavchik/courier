/*
** Copyright 2002-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	"courier.h"
#include	"moduledel.h"
#include	"comqueuename.h"
#include	"comfax.h"
#include	"rfc822/rfc822.h"
#include	"waitlib/waitlib.h"
#include	<courier-unicode.h>
#include	"numlib/numlib.h"
#include	"comuidgid.h"
#include	<stdlib.h>
#include	<locale.h>
#include	<langinfo.h>
#include	<string.h>
#include	<ctype.h>
#include	<signal.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<pwd.h>
#include	<sys/wait.h>
#include	<sys/time.h>
#include	"comctlfile.h"
#include	"comqueuename.h"
#include	"comstrtotime.h"
#include	"comstrtimestamp.h"
#include	"comverp.h"
#include	<sys/stat.h>
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif
#include	"faxconvert.h"
#include	"sendfax.h"
#include	"faxtmpdir.h"

/*
**  Kill the child process.
*/

static void killit(int n)
{
	module_signal(SIGKILL);
	exit(0);
}

static void faxabort(int n)
{
	signal(SIGTERM, SIG_IGN);
	kill(-getpid(), SIGTERM);
}

/*
** Main loop.  Receive a message to deliver.  Fork off a child process.  Yawn.
*/

int main(int argc, char **argv)
{
	struct moduledel *p;
	int waitstat;
	const char *cp;

	clog_open_syslog("courierfax");
	if (chdir(getenv("COURIER_HOME")))
		clog_msg_errno();

	cp=getenv("MAXDELS");
	if (!cp || atoi(cp) != 1)
	{
		clog_msg_start_err();
		clog_msg_str("FATAL: courierfax module misconfiguration, MAXDELS must be 1");
		clog_msg_send();
		exit(0);
	}

	cp=getenv("MAXRCPT");
	if (!cp || atoi(cp) != 1)
	{
		clog_msg_start_err();
		clog_msg_str("FATAL: courierfax module misconfiguration, MAXRCPT must be 1");
		clog_msg_send();
		exit(0);
	}

	module_init(0);
	while ((p=module_getdel()) != NULL)
	{
		pid_t	pid;
		unsigned delid;

		delid=atol(p->delid);

		if ((pid=module_fork(delid, 0)) == -1)
		{
			clog_msg_prerrno();
			module_completed(delid, delid);
			continue;
		}

		if (pid == 0)
		{
#if     HAVE_SETPGRP
#if     SETPGRP_VOID
			setpgrp();
#else
			setpgrp(0, 0);
#endif
#else
#if     HAVE_SETPGID
			setpgid(0, 0);
#endif
#endif
			signal(SIGTERM, faxabort);

			courierfax(p);
			exit(0);
		}
	}

	module_signal(SIGTERM);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGALRM, killit);
	alarm(5);
	wait(&waitstat);
	alarm(0);
	return (0);
}

void invoke_sendfax(char **argvec)
{
	execv(SENDFAX, argvec);
	perror(SENDFAX);
	exit(1);
}

void courierfax_failed(unsigned pages_sent,
		       struct ctlfile *ctf, unsigned nreceip,
		       int errcode, const char *errmsg)
{

	auto i=ctlfile_searchfirst(ctf, COMCTLFILE_FAXEXPIRES);
	time_t now_time;

	time(&now_time);
	ctlfile_append_reply(ctf, nreceip,
			     errmsg,
			     (pages_sent == 0 &&
			      i >= 0 &&
			      errcode < 10 &&
			      now_time < strtotime(ctf->lines[i]+1)
			      ? COMCTLFILE_DELDEFERRED:
			      COMCTLFILE_DELFAIL_NOTRACK), 0);
}
