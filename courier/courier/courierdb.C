/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"localstatedir.h"
#include	<string.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<time.h>
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_SYS_FILE_H
#include	<sys/file.h>
#endif
#include	<signal.h>
#include	"waitlib/waitlib.h"
#include	"liblock/config.h"
#include	"liblock/liblock.h"

pid_t	couriera, courierb;
time_t	courierastart, courierbstart;
unsigned	queuelo, queuehi;
const char *triggername=TMPDIR "/trigger";

extern int courierbmain();

int main(int argc, char **argv)
{
int	rc;
struct	stat	stat_buf;
int	triggerr;
int	fd;
int	i;

	clog_open_syslog("courierd");
	if (chdir(courierdir()))
		clog_msg_errno();

	umask(022);

	if ((fd=open(TMPDIR "/courierd.lock", O_RDWR|O_CREAT, 0600))
		< 0)	clog_msg_errno();

	if ( ll_lock_ex_test(fd) )
	{
		_exit(0);	/* Daemon already running */
	}

	for (i=3; i<256; i++)
		if (i != fd)	close(i);
			/*
			** Close any stale file descriptors, including #3:
			** pipe from courier.c, close it to let it know
			** that we're ready to rock-n-roll.
			*/

	try
	{
		couriera=getpid();
		courierb=0;
		time(&courierastart);
		if (argc > 1 && strcmp(argv[1], "test") == 0)
		{
			exit (courierbmain());
		}

		signal(SIGCHLD, SIG_DFL);

		// Create the trigger pipe

		if (stat(triggername, &stat_buf)
			|| !S_ISFIFO(stat_buf.st_mode)
			|| (stat_buf.st_mode & 0777) != 0660)
		{
			umask(002);
			unlink(triggername);
			rmdir(triggername);
#if HAVE_MKFIFO
			mkfifo(triggername, 0660);
#else
			mknod(triggername, S_IFIFO | 0660, 0);
#endif

			umask(022);
		}

		/* First time we create the trigger, it must be owned by
		** MAILUID/MAILGID
		*/
		if (
			chown(triggername, MAILUID, MAILGID) < 0 ||
			(triggerr=open(triggername,
				O_RDONLY | O_NONBLOCK, 0)) < 0)
		{
			clog_msg_errno();
			exit(1);
			return (1);
		}

		for (;;)
		{
		int	waitstat;

			courierb=fork();
			if (courierb == (pid_t)-1)
			{
				clog_msg_prerrno();
				sleep(15);
				continue;
			}
			if (courierb == 0)
			{
				close(fd);
				close(triggerr);
				rc=courierbmain();
				_exit(rc);
			}
			while ( wait(&waitstat) != courierb )
				;
			if (WIFEXITED(waitstat))
			{
				if (WEXITSTATUS(waitstat) == 0)
					continue;	/* Normal recycle */
				if (WEXITSTATUS(waitstat) == 99)
				{
					_exit(0);	/* Termination */
				}
			}
			clog_msg_start_err();
			clog_msg_str("ABNORMAL TERMINATION");
			if (WIFEXITED(waitstat))
			{
				clog_msg_str(", exit status: ");
				clog_msg_int(WEXITSTATUS(waitstat));
			}
			else	clog_msg_str(" BY A SIGNAL");
			clog_msg_send();
			clog_msg_start_err();
			clog_msg_str("Will restart in 60 seconds.");
			clog_msg_send();

			sleep(60);
		}
		close(fd);
	}
	catch (const char *p)
	{
		clog_msg_start_err();
		clog_msg_str("EXIT: ");
		clog_msg_str(p);
		clog_msg_send();
		exit(1);
	}
	catch (char *p)
	{
		clog_msg_start_err();
		clog_msg_str("EXIT: ");
		clog_msg_str(p);
		clog_msg_send();
		exit(1);
	}
	exit(rc);
}
