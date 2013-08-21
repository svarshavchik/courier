/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"courier_lib_config.h"
#include	"sbindir.h"
#include	<string.h>
#include	<stdio.h>
#include	<signal.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<fcntl.h>
#include	"waitlib/waitlib.h"

void clog_start_logger(const char *progname)
{
pid_t	p;
int	waitstat;
int	pipefd[2];

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		return;
	}

	signal(SIGCHLD, SIG_DFL);
	while ((p=fork()) == -1)
	{
		sleep(5);
	}

	if (p == 0)
	{
		dup2(pipefd[0], 0);
		close(pipefd[0]);
		close(pipefd[1]);
		close(1);
		open("/dev/null", O_WRONLY);
		dup2(1, 2);
		if (chdir(courierdir()))
			_exit(5);
		while ((p=fork()) == -1)
		{
			sleep(5);
		}
		if (p == 0)
		{
			execl(COURIERLOGGER,
			      COURIERLOGGER, progname, (char *)0);
			_exit(5);
		}
		_exit(0);
	}
	dup2(pipefd[1], 2);
	close(pipefd[0]);
	close(pipefd[1]);
	while (wait(&waitstat) != p)
		;
}
