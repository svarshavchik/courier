/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<signal.h>
#if HAVE_SYSEXITS_H
#include	<sysexits.h>
#else
#define	EX_TEMPFAIL	75
#endif

#include	"waitlib/waitlib.h"

static void usage()
{
	fprintf(stderr, "Usage: preline [program] [arguments]\n");
	exit(1);
}

int main(int argc, char **argv)
{
int	pipefd[2];
pid_t	p;
char	*env;
int	waitstat;
char	**newargv;
int	c;
char *cmd;

	if (argc <= 1)
	{
		usage();
	}

	if (argc == 1)
	 {
		fprintf(stderr, "%s: commandline parameter 'program' needed\n", argv[0]);
	 	exit (1);
	 }
 
	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		exit(EX_TEMPFAIL);
	}

	signal(SIGCHLD, SIG_DFL);
	p=fork();
	if (p == -1)
	{
		perror("fork");
		exit(EX_TEMPFAIL);
	}

	if (p == 0)
	{
		p=fork();
		if (p == -1)
		{
			perror("fork");
			exit(EX_TEMPFAIL);
		}
		if (p)
			exit(0);
		close(pipefd[0]);
		dup2(pipefd[1], 1);
		close(pipefd[1]);
		if ((env=getenv("UFLINE")) != 0)
			printf("%s\n", env);
		if ((env=getenv("RPLINE")) != 0)
			printf("%s\n", env);
		if ((env=getenv("DTLINE")) != 0)
			printf("%s\n", env);

		while ((c=getchar()) != EOF)
			putchar(c);
		fflush(stdout);
		exit(0);
	}

	close(pipefd[1]);
	dup2(pipefd[0], 0);
	close(pipefd[0]);

	while (wait(&waitstat) != p)
		;
	if (!WIFEXITED(waitstat) || WEXITSTATUS(waitstat))
		exit(EX_TEMPFAIL);

	if ((newargv=(char **)malloc(sizeof(char *)*argc)) == 0)
	{
		perror("malloc");
		exit(EX_TEMPFAIL);
	}
	for (c=1; c<argc; c++)
		newargv[c-1]=argv[c];
	newargv[c-1]=0;

	cmd=newargv[0];

	if ((newargv[0]=strrchr(newargv[0], '/')) != 0)
		++newargv[0];
	else
		newargv[0]=cmd;

	execvp(cmd, newargv);
	perror("exec");
	exit(EX_TEMPFAIL);
}
