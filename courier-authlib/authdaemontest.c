/*
** Copyright 2000 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<fcntl.h>
#include        <unistd.h>
#include        <stdlib.h>
#include        <stdio.h>
#include        <errno.h>
#include	<signal.h>
#include	"authwait.h"


static int runtest(int count, char **argv)
{
int	i;
pid_t	p;
int	waitstat;
int	x;

	for (i=0; i<count; i++)
	{
		p=fork();
		if (p == -1)
		{
			perror("fork");
			return (1);
		}
		if (p == 0)
		{
			execv(argv[0], argv);
			perror("exec");
			exit(1);
		}

		while (wait(&waitstat) != p)
			;
		if (WIFEXITED(waitstat))
			x=WEXITSTATUS(waitstat);
		else
			x=1;
		if (x)
			return (1);
	}
	return (0);
}

static int cleanup()
{
int	waitstat;
int	rc=0;
int	x;

	while (wait(&waitstat) >= 0 || errno != ECHILD)
	{
		x=1;
		if (WIFEXITED(waitstat))
			x=WEXITSTATUS(waitstat);
		if (x)
			rc=1;
	}
	return (rc);
}

static int dotest(int nchildren, int count, char **argv)
{
pid_t	p;
int	i;

	signal(SIGCHLD, SIG_DFL);

	for (i=0; i<nchildren; i++)
	{
		p=fork();
		if (p == -1)
		{
			perror("fork");
			cleanup();
			return (1);
		}
		if (p == 0)
		{
			close(1);
			if (open("/dev/null", O_WRONLY) != 1)
			{
				perror("open");
				exit(1);
			}
			exit(runtest(count, argv));
		}
	}

	return (cleanup());
}

int main(int argc, char **argv)
{
	if (argc >= 4)
	{
	int nchildren=atoi(argv[1]);
	int count=atoi(argv[2]);

		if (nchildren > 0 && count > 0)
			exit(dotest(nchildren, count, argv+3));
	}

	fprintf(stderr, "Usage: authdaemontest [nchildren] [count] ./authtest [userid] [password]\n");
	exit(1);
	return (1);
}
