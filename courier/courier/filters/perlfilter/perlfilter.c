/*
** Copyright 2000-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/
#include	<stdio.h>
#include	"courier.h"

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<sys/types.h>
#include	<sys/stat.h>
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
#include	"libfilter/libfilter.h"
#include	"filtersocketdir.h"
#include	"waitlib/waitlib.h"
#include	"wrapperpl.h"


int listen_sock;	/* Filter connection socket */
char *filter;	/* The Perl filter */

unsigned nchildren=5;	/* Number of child perlfilter processes */
pid_t	*children;

/*
** This is, basically, persistent.c from perlembed, with a couple of changes
*/

#undef	PACKAGE
#undef	VERSION

#include "xsinit.c"
 
static void perlfilter()
{
char *embedding[] = { "", WRAPPERPL };
char *args[] = { "", "0", NULL, NULL, NULL};
int exitstatus = 0;
int	sock;
PerlInterpreter *my_perl;
 
	if((my_perl = perl_alloc()) == NULL)
	{
		fprintf(stderr, "no memory!");
		exit(1);
	}
	perl_construct(my_perl);
 
	exitstatus = perl_parse(my_perl, xs_init, 2, embedding, NULL);
 
	if (exitstatus || (exitstatus=perl_run(my_perl)) != 0)
	{
		fprintf(stderr, "Cannot parse " WRAPPERPL "\n");
		exit(exitstatus);
	}

	while ((sock=lf_accept(listen_sock)) >= 0)
	{
	char	sockbuf[100];

		args[0]=filter;
		sprintf(sockbuf, "%d", sock);
		args[2]=sockbuf;

		{
		dSP ;
		ENTER ;
		SAVETMPS ;

		perl_call_argv("Embed::Persistent::eval_file",
			G_VOID | G_DISCARD | G_EVAL, args);

#ifdef	ERRSV
		if(SvTRUE(ERRSV))
#else
		if(SvTRUE(GvSV(errgv)))
#endif
		{
			clog_open_syslog("perlfilter");
			clog_msg_start_err();
			clog_msg_str("eval error: ");

#ifdef	ERRSV
			clog_msg_str(SvPV(ERRSV,PL_na));
#else
			clog_msg_str(SvPV(GvSV(errgv),na));
#endif
			clog_msg_send();
		}
		FREETMPS ;
		LEAVE ;
		}

		close(sock);	/* Just in case */
	}
#ifdef	perl_destruct_level
	perl_destruct_level=0;
#else
	PL_perl_destruct_level=0;
#endif
	perl_destruct(my_perl);
	perl_free(my_perl);
	exit(0);
}

static void reap_child(pid_t pid, int exit_status)
{
	int	n;
	fd_set fd0;
	struct timeval tv;

	FD_ZERO(&fd0);
	FD_SET(0, &fd0);

	tv.tv_sec=0;
	tv.tv_usec=0;

	if (select(1, &fd0, 0, 0, &tv) >= 0)
	{
		if (FD_ISSET(0, &fd0))
			return;
	}

	n=wait_reforkchild(nchildren, children, pid);

	if (n < 0)
	{
	unsigned u;

		clog_open_syslog("perlfilter");
		clog_msg_str("ERR: Unable to restart a child process"
				" - aborting.");
		clog_msg_send();
		for (u=0; u<nchildren; u++)
			if (children[u] >= 0)
				kill(children[u], SIGKILL);
		exit(0);
	}

	if (n > 0)
	{
		wait_restore();
		if (exit_status)
		{
			clog_open_syslog("perlfilter");
			clog_msg_str("ERR: Child process ");
			clog_msg_uint(pid);
			clog_msg_str(" terminated - restarting.");
			clog_msg_send();
		}
		perlfilter();
	}
}

static RETSIGTYPE	reap_children(int signum)
{
	wait_reap(reap_child, reap_children);

#if     RETSIGTYPE != void
        return (0);
#endif
}


int main(int argc, char **argv, char **env)
{
char	*fn, *f;
int	rc;
char	buffer[1];
int	waitstat;
struct	stat	stat_buf;

	fn=config_localfilename("filters/perlfilter-numprocs");
	if ( (f=config_read1l(fn)) != 0)
	{
		sscanf(f, "%u", &nchildren);
		free(f);
	}
	free(fn);
	if (nchildren <= 0)	nchildren=1;

	fn=config_localfilename("filters/perlfilter");
	if ( (f=config_read1l(fn)) == 0)
	{
		fprintf(stderr, "filters/perlfilter: not defined.\n");
		exit(1);
	}

	filter=f;

	if (stat(filter, &stat_buf))
	{
		perror(filter);
		exit(1);
	}

	listen_sock=lf_init("filters/perlfilter-mode",
		ALLFILTERSOCKETDIR "/perlfilter",
		ALLFILTERSOCKETDIR "/.perlfilter",
		FILTERSOCKETDIR "/perlfilter",
		FILTERSOCKETDIR "/.perlfilter");

	if (listen_sock < 0)
	{
		perror("socket");
		exit(1);
	}

	rc=wait_startchildren(nchildren, &children);

	if (rc < 0)
	{
		perror("fork");
		exit(1);
	}

	if (rc > 0)
	{
		lf_init_completed(listen_sock);
		perlfilter();
	}

	signal(SIGCHLD, reap_children);
	lf_init_completed(listen_sock);

	while (read(0, &buffer, 1) != 0)
	{
		;
	}
	wait_restore();

	/* Wait for all child processes to terminate */

	while (wait(&waitstat) > 0)
		;
	exit(0);
	return (0);
}
