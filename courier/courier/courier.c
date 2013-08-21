/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"localstatedir.h"
#include	"datadir.h"
#include	"comctlfile.h"
#include	"comparseqid.h"
#include	"comqueuename.h"
#include	"comtrack.h"
#include	<sys/types.h>
#include	<string.h>
#include	<signal.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if	HAVE_SYS_IOCTL_H
#include	<sys/ioctl.h>
#endif
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#include	"waitlib/waitlib.h"
#include	"liblock/config.h"
#include	"liblock/liblock.h"
#include	"numlib/numlib.h"

void courier_clear_all();
void courier_show_all();

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--version") == 0)
	{
		printf("%s\n", COURIER_COPYRIGHT);
		exit(0);
	}

	if (chdir(courierdir()))
	{
		perror("chdir");
		return (1);
	}
	if (argc < 2)	return (0);

	if (strcmp(argv[1], "stop") == 0)
	{
	int	fd;

		trigger(TRIGGER_STOP);

		/* Wait until the exclusive lock goes away: */

		signal(SIGHUP, SIG_DFL);
		if ((fd=open(TMPDIR "/courierd.lock", O_RDWR|O_CREAT, 0600))
			< 0)	clog_msg_errno();

		alarm(15);	/* But abort after 15 seconds. */

		ll_lock_ex(fd);
		return (0);
	}

	if (strcmp(argv[1], "restart") == 0)
	{
		trigger(TRIGGER_RESTART);
		return (0);
	}

	if (strcmp(argv[1], "flush") == 0)
	{
	ino_t	n;
	struct	ctlfile	ctf;

		if (getuid())
		{
		/*
		** We'll fail trying to open the pipe anyway, but let's
		** give a meaningful error message now.
		*/
			fprintf(stderr,
		"courier flush can be executed only by the superuser.\n");
			exit(1);
		}

		if (argc < 3)
		{
			trigger(TRIGGER_FLUSH);	/* Everything */
			exit(0);
		}

		if (comparseqid(argv[2], &n) == 0 &&
			ctlfile_openi(n, &ctf, 1) == 0)
		{
		int c=ctlfile_searchfirst(&ctf, COMCTLFILE_MSGID);

			if (c >= 0 && strcmp(ctf.lines[c]+1, argv[2]) == 0)
			{
			char	*s=courier_malloc(sizeof(TRIGGER_FLUSHMSG)+1+
				strlen(argv[2]));

				strcat(strcat(strcpy(s, TRIGGER_FLUSHMSG),
					argv[2]), "\n");
				trigger(s);
				ctlfile_close(&ctf);
				return (0);
			}
			ctlfile_close(&ctf);
		}
		fprintf(stderr, "No such message.\n");
		exit(1);
		return (1);
	}

	/* Might as well... */

	if (strcmp(argv[1], "start") == 0)
	{
	pid_t	p;
	int	waitstat;
	char	dummy;

	/*
	** Ok, courierd will close file descriptor 3 when it starts, so we
	** put a pipe on there, and wait for it to close.
	*/
	int	pipefd[2];

		close(3);
		if (open("/dev/null", O_RDONLY) != 3 || pipe(pipefd))
		{
			fprintf(stderr, "Cannot open pipe\n");
			exit(1);
		}

		if (getuid())
		{
		/*
		** We'll fail trying to execute courierd anyway, but let's
		** give a meaningful error message now.
		*/
			fprintf(stderr, "courier start can be executed only by the superuser.\n");
			return (1);
		}
		signal(SIGCHLD, SIG_DFL);
		while ((p=fork()) == -1)
		{
			perror("fork");
			sleep(10);
		}
		if (p == 0)
		{
			dup2(pipefd[1], 3);
			close(pipefd[1]);
			close(pipefd[0]);
			while ((p=fork()) == -1)
			{
				perror("fork");
				sleep(10);
			}
			if (p == 0)
			{
				/*
				** stdin from the bitbucket.  stdout to
				** /dev/null, or the bitbucket.
				*/

				signal(SIGHUP, SIG_IGN);
				close(0);
				open("/dev/null", O_RDWR);
				dup2(0, 1);
				dup2(0, 2);
			/* See if we can disconnect from the terminal */

#ifdef	TIOCNOTTY
				{
				int fd=open("/dev/tty", O_RDWR);

					if (fd >= 0)
					{
						ioctl(fd, TIOCNOTTY, 0);
						close(fd);
					}
				}
#endif
			/* Remove any process groups */

#if	HAVE_SETPGRP
#if	SETPGRP_VOID
				setpgrp();
#else
				setpgrp(0, 0);
#endif
#endif
				execl( DATADIR "/courierctl.start",
					"courierctl.start", (char *)0);
				perror("exec");
				_exit(1);
			}
			_exit(0);
		}
		close(pipefd[1]);
		while (wait(&waitstat) != p)
			;
		if (read(pipefd[0], &dummy, 1) < 0)
			; /* ignore */
		close(pipefd[0]);
		close(3);
		return (0);
	}

	if (strcmp(argv[1], "clear") == 0 && argc > 2)
	{
		libmail_changeuidgid(MAILUID, MAILGID);

		if (strcmp(argv[2], "all") == 0)
		{
			courier_clear_all();
		}
		else
		{
			track_save_email(argv[2], TRACK_ADDRACCEPTED);
			printf("%s cleared.\n", argv[2]);
		}
		return (0);
	}

	if (argc > 2 && strcmp(argv[1], "show") == 0 &&
	    strcmp(argv[2], "all") == 0)
	{
		libmail_changeuidgid(MAILUID, MAILGID);
		courier_show_all();
	}

	return (0);
}
