/*
** Copyright 1998 - 2014 Double Precision, Inc.
** See COPYING for distribution information.
*/
#include	"config.h"
#include	"courier.h"
#include	"mydirent.h"
#include	"liblock/config.h"
#include	"liblock/liblock.h"
#include	"numlib/numlib.h"
#include	"filtersocketdir.h"
#include	"filteractivedir.h"
#include	"pidfile.h"
#include	"sysconfdir.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<signal.h>
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<sys/types.h>
#if	HAVE_SYS_WAIT_H
#include	<sys/wait.h>
#endif


#define	LOCKFILE	FILTERSOCKETDIR "/.lock"

static void stop()
{
FILE	*f;
unsigned long pid;
int	fd;

	if ((f=fopen(PIDFILE, "r")) == 0)	return;
	if (fscanf(f, "%lu", &pid) <= 0)
	{
		fclose(f);
		return;
	}
	fclose(f);

	kill (pid, SIGTERM);

	/*
	** Wait until the daemon goes away by attempting to lock the lock file
	** ourselves.
	*/

	alarm(15);
	fd=open(LOCKFILE, O_RDWR);
	if (fd < 0)	return;
	ll_lock_ex(fd);
	close(fd);
}

static struct filterinfo {
	struct filterinfo *next;
	char *filtername;
	int	fd0;
	pid_t	p;
	} *filterlist=0;

static int sighup_received=0;
static int sigterm_received=0;

static void kill1filter(struct filterinfo **p)
{
int	waitstat;
pid_t	pid;
struct	filterinfo *pp= *p;

	*p=pp->next;

	clog_msg_start_info();
	clog_msg_str("Stopping ");
	clog_msg_str(pp->filtername);
	clog_msg_send();
	close(pp->fd0);
	while ((pid=wait(&waitstat)) >= 0 && pid != pp->p)
		;
	free(pp->filtername);
	free(pp);
}

static char *activename(const char *p)
{
char	*s=malloc(sizeof(FILTERACTIVEDIR "/")+strlen(p));

	if (!s)
	{
		perror("malloc");
		exit(1);
	}
	return (strcat(strcpy(s, FILTERACTIVEDIR "/"), p));
}

/*
**	sighup attempts to synchronize everything.
*/
static void sighup()
{
struct filterinfo **pp;
DIR	*dirp;
struct dirent *de;

	/* Start any new filters */

	dirp=opendir(FILTERACTIVEDIR);
	while (dirp && (de=readdir(dirp)) != 0)
	{
	char	*n;
	int	pipefd[2];
	struct filterinfo *newp;

		if (de->d_name[0] == '.')
			continue;

		for (pp= &filterlist; *pp; pp= &(*pp)->next)
			if (strcmp( (*pp)->filtername, de->d_name) == 0)
				break;

		if (*pp)	continue;

		n=malloc(sizeof(FILTERACTIVEDIR "/")+strlen(de->d_name));
		if (!n)
		{
			perror("malloc");
			exit(1);
		}
		strcat(strcpy(n, FILTERACTIVEDIR "/"), de->d_name);

		if ( (newp=(struct filterinfo *)
				malloc(sizeof(struct filterinfo))) == 0 ||
			(newp->filtername=malloc(strlen(de->d_name)+1)) == 0)
		{
			perror("malloc");
			exit(1);
		}
		strcpy(newp->filtername, de->d_name);
		newp->next=filterlist;
		filterlist=newp;

		if (pipe(pipefd) < 0)
		{
			perror("pipe");
			exit(1);
		}

		newp->fd0=pipefd[1];

		while ((newp->p=fork()) < 0)
		{
			perror("fork");
			sleep(3);
		}

		if ( newp->p == 0)
		{
			dup2(pipefd[0], 0);
			close(pipefd[0]);
			close(pipefd[1]);
			execl(n, de->d_name, (char *)0);
			perror("exec");
			exit(1);
		}
		free(n);
		close(pipefd[0]);
		if (fcntl(newp->fd0, F_SETFD, FD_CLOEXEC) < 0)
		{
			perror("fcntl");
			exit(1);
		}
		clog_msg_start_info();
		clog_msg_str("Starting ");
		clog_msg_str(newp->filtername);
		clog_msg_send();
	}
	if (dirp)	closedir(dirp);

	/* Then, kill any filters that should not be running any more */

	for (pp= &filterlist; *pp; )
	{
	char	*n=activename( (*pp)->filtername );

		if (access(n, 0) == 0)
		{
			free(n);
			pp= &(*pp)->next;
			continue;
		}
		free(n);
		kill1filter(pp);
	}

	/* Finally, remove any sockets for filters that don't run any more */

  {
  static const char *dirs[2]={FILTERSOCKETDIR, ALLFILTERSOCKETDIR};
  int i;

    for (i=0; i<2; i++)
    {
	dirp=opendir(dirs[i]);
	while (dirp && (de=readdir(dirp)) != 0)
	{
	char	*n;

		if (de->d_name[0] == '.')
			continue;

		for (pp= &filterlist; *pp; pp= &(*pp)->next)
			if (strcmp( (*pp)->filtername, de->d_name) == 0)
				break;

		if (*pp)	continue;

		n=malloc(strlen(dirs[i])+strlen(de->d_name)+2);
		if (!n)
		{
			perror("malloc");
			exit(1);
		}
		strcat(strcat(strcpy(n, dirs[i]), "/"), de->d_name);
		unlink(n);
		free(n);
	}
	if (dirp)	closedir(dirp);
    }
  }
}

static RETSIGTYPE sighuphandler(int signum)
{
	sighup_received=1;
#if     RETSIGTYPE != void
        return (0);
#endif
}

static RETSIGTYPE sigtermhandler(int signum)
{
	sigterm_received=1;
#if     RETSIGTYPE != void
        return (0);
#endif
}

/*
**	Tricky synchronization when we start courierfilter.  Basically, we
**	don't want to exit this process until all filters are initialized.
**
**	Here's what we do.  All filters will manually close file descritor
**	3 when they're ready.  Therefore, we set up a pipe whose write end
**	is set to file descriptor 3, and is inherited via fork and exec
**	by every filter.  We do a read on the pipe, which will complete
**	when all copies of the write side of the pipe are closed.
**
*/
static int reserve3(int* pipe3fd)
{
	int devnull = -1, dupped = -1;
	pipe3fd[0] = pipe3fd[1] = -1;
	close(3);
	if (open("/dev/null", O_RDONLY) != 3
	    || pipe(pipe3fd) < 0
	    || close(3)
	    || (dupped = dup(pipe3fd[1])) != 3)
	{
		fprintf(stderr, "Unable to reserve file descriptor 3.\n");
		close(devnull);
		close(dupped);
		close(pipe3fd[0]);
		close(pipe3fd[1]);
		return (1);
	}
	close(pipe3fd[1]);
	return (0);
}

static int start()
{
	int	lockfd;
	int	pipe3fd[2];
	char	temp_buf;

	lockfd=ll_daemon_start(LOCKFILE);
	if (lockfd <= 0)
	{
		close(3);
		return (lockfd);
	}

	if (lockfd == 3)
	{
		lockfd=dup(lockfd);	/* Get it out of the way */
		if (lockfd < 0)
		{
			perror("dup");
			return (-1);
		}
	}
	if (reserve3(pipe3fd))
	{
		return (1);
	}

	fcntl(lockfd, F_SETFD, FD_CLOEXEC);
	fcntl(pipe3fd[0], F_SETFD, FD_CLOEXEC);

	fcntl(3, F_SETFD, FD_CLOEXEC);	/* For the logger */
	clog_start_logger("courierfilter");
	clog_open_stderr(0);
	fcntl(3, F_SETFD, 0);

	signal(SIGPIPE, SIG_IGN);
	dup2(2, 1);
	sighup();
	close(0);
	open("/dev/null", O_RDONLY);

	close(3);
	if (read(pipe3fd[0], &temp_buf, 1) != 1)
		; /* ignore */
	close(pipe3fd[0]);

	signal(SIGHUP, sighuphandler);
	signal(SIGTERM, sigtermhandler);

	ll_daemon_started(PIDFILE, lockfd);

	while (!sigterm_received)
	{
		if (sighup_received)
		{
			sighup_received=0;
			if (reserve3(pipe3fd) == 0)
			{
				sighup();
				close(3);
				close(pipe3fd[0]);
			}
			signal(SIGHUP, sighuphandler);
		}
#if	HAVE_SIGSUSPEND

	{
	sigset_t	ss;

		sigemptyset(&ss);
		sigsuspend(&ss);
	}
#else
		sigpause(0);
#endif
	}
	while (filterlist)
		kill1filter(&filterlist);
	unlink(PIDFILE);
	return (0);
}

static int restart()
{
	return (ll_daemon_restart(LOCKFILE, PIDFILE));
}

int main(int argc, char **argv)
{
	if (getuid() == 0)
		libmail_changeuidgid(MAILUID, MAILGID);
			/* Drop root privileges */
	if (chdir(courierdir()) < 0)
	{
		perror("courierdir");
		return (1);
	}

	if (argc > 1)
	{
		if (strcmp(argv[1], "stop") == 0)
		{
			stop();
			return (0);
		}
		if (strcmp(argv[1], "start") == 0)
		{
			setenv("sysconfdir", SYSCONFDIR, 1);
			return (start());
		}
		if (strcmp(argv[1], "restart") == 0)
		{
			return (restart());
		}
	}
	fprintf(stderr, "Usage: %s (stop|restart|start)\n", argv[0]);
	return (1);
}
