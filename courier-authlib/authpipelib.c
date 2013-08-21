/*
** Copyright 1998 - 2000 Double Precision, Inc.  See COPYING for
** distribution information.
*/

/* Based on code by Christian Loitsch <courier-imap@abc.fgecko.com> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
/* for fork */
#include <sys/types.h>
/*  used to avoid zombies */
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/select.h>

#include	"auth.h"
#include	"authcustom.h"
#include	"courierauthdebug.h"

#include	"courierauthdebug.h"

#include	"authpipelib.h"
#include	"authpiperc.h"

static int lastIn = -1;
static int lastOut = -1;
static pid_t childPID = -1;

static void eliminatePipe(pid_t child);

static void execChild(int to[], int from[])
{
	DPRINTF("executing %s", PIPE_PROGRAM);

	close(STDIN_FILENO); dup2(to[0], STDIN_FILENO);
	close(STDOUT_FILENO); dup2(from[1], STDOUT_FILENO);
	close(to[0]); close(to[1]); close(from[0]); close(from[1]);

	execl(PIPE_PROGRAM, PIPE_PROGRAM, NULL);

	DPRINTF("pipe: failed to execute %s: %s",PIPE_PROGRAM, strerror(errno));
	exit(1);
}

void closePipe(void)
{
	DPRINTF("closing pipe");
	if (lastIn >= 0)	{ close(lastIn); lastIn = -1; }
	if (lastOut >= 0)	{ close (lastOut); lastOut = -1; }
	if (childPID > 1)	{ eliminatePipe(childPID); childPID = -1; }
}

static int forkPipe(int *dataIn, int *dataOut, pid_t *child)
{
	int to[2], from[2];

	/* let's create 2 pipes */
	if(pipe(to) < 0) {
		DPRINTF("pipe: failed to create pipe: %s", strerror(errno));
		return 1;
	}

	if(pipe(from) < 0) {
		DPRINTF("pipe: failed to create pipe: %s", strerror(errno));
		close(to[0]); close(to[1]);
		return 1;
	}

	DPRINTF("attempting to fork");
	*child = fork();
	if(*child < 0) {
		DPRINTF("pipe: failed to fork: %s", strerror(errno));
		close(to[0]); close(to[1]); close(from[0]); close(from[1]);
		return 1;
	}

	/* child */
	if(*child == 0) execChild(to, from);

	/* parent */
	DPRINTF("Pipe auth. started Pipe-program (pid %d)", *child);

	close(to[0]); close(from[1]);
	*dataIn = from[0]; *dataOut = to[1];
	return 0;
}

/* kills and waits for child
 * in a quite inefficient way, but this shouldn't happen very often */
static void eliminatePipe(pid_t child)
{
	unsigned int seconds;

	/* let's first look, if child is already terminated */
	DPRINTF("trying to wait for child (WNOHANG) (pid %d)", child);
	if (waitpid(child, NULL, WNOHANG) > 0) return;
	
	DPRINTF("sleep 2 seconds and try again to wait for pid %d", child);
	/* let's give the pipe-program a few seconds to terminate */
	sleep(2);  /* don't care if interrupted earlier */
	if (waitpid(child, NULL, WNOHANG) > 0) return;

	/* let's TERM it */
	DPRINTF("killing (SIGTERM) child pid %d", child);
	kill(child, SIGTERM);

	/* give it a few seconds */
	for (seconds = 10; seconds > 0; sleep(1), seconds--)
		if (waitpid(child, NULL, WNOHANG) > 0) return;

	/* ok, let's KILL it */
	DPRINTF("killing (SIGKILL) child pid %d", child);
	if (kill(child, SIGKILL) == 0)
	{
		/* and wait, unless we have a kernel bug, it MUST terminate */
		DPRINTF("waitpiding for child pid (blocking!) %d)", child);
		waitpid(child, NULL, 0);
	}
	else
	{

		DPRINTF("error when sending sigkill to %d", child);
		if (errno != ESRCH) return;
		/* strange, we can not kill our own child with SIGKILL*/

		/* errno indicates process does not exist, maybe it's dead
		 * by now, let's try 1 final time, else, ignore it */
		DPRINTF("maybe because already dead (pid: %d)", child);
		waitpid(child, NULL, WNOHANG);
	}
}

int getPipe(int *dataIn, int *dataOut)
{
	int rv;

	if (childPID > 1)
	{
		/* Simple test if the child is still usable: do a read
		** poll on dataIn. If the child has closed the pipe,
		** or there is spurious data, the fd will be ready. */
		fd_set fdr;
		struct timeval tv;
		FD_ZERO(&fdr);
		FD_SET(lastIn, &fdr);
		tv.tv_sec=0;
		tv.tv_usec=0;
		rv = select(lastIn+1, &fdr, 0, 0, &tv);
		if (rv == 0)
		{
			DPRINTF("reusing pipe, with in: %d out: %d", lastIn, lastOut);
			*dataIn = lastIn;
			*dataOut = lastOut;
			return 0;
		}
		if (rv < 0)
			perror("authpipe: getPipe: select");
		else
		{
			DPRINTF("child died or sent spurious data (pid: %d)", childPID);
		}
	}

	/* ok pipe was not usable; either this is the first call, or
	 * the pipe broke the connection.
	 * We have to clean up and start a new one */

	closePipe();
	DPRINTF("forking new one");
	rv = forkPipe(&lastIn, &lastOut, &childPID);
	if (rv)
	{
		DPRINTF("couldn't fork new pipe");
		lastIn = -1;
		lastOut = -1;
		childPID = -1;
	}
	else
	{
		DPRINTF("new pipe has in: %d, out: %d", lastIn, lastOut);
		*dataIn = lastIn;
		*dataOut = lastOut;
	}
	return rv;
}

