/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<string.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<signal.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<pwd.h>
#include	<grp.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_SYSLOG_H
#include	<syslog.h>
#else
#undef	TESTMODE
#define	TESTMODE	1
#endif
#include	"liblock/config.h"
#include	"liblock/liblock.h"
#include	"numlib/numlib.h"


static int do_logger(const char *name, int facility, FILE *f)
{
char	buf[512];
char	*p;
int	c;

#if	TESTMODE

#else
	openlog(name, 0

#ifdef	LOG_NDELAY
			| LOG_NDELAY
#else
			| LOG_NOWAIT
#endif

			, facility);
#endif
	if (chdir("/") < 0)
	{
		perror("chdir(\"/\")");
		exit(1);
	}

	while (fgets(buf, sizeof(buf), f))
	{
		if ((p=strchr(buf, '\n')) != 0)	*p=0;
		else
			while ((c=getchar()) != EOF && c != '\n')
				;

#if	TESTMODE
		fprintf(stderr, "%s: %s\n", name, buf);
#else
		c=LOG_INFO;
		if (strncmp(buf, "ERR:", 4) == 0)
		{
			c=LOG_ERR;
			p=buf+4;
		}
		else if (strncmp(buf, "WARN:", 5) == 0)
		{
			c=LOG_WARNING;
			p=buf+5;
		}
		else if (strncmp(buf, "ALERT:", 6) == 0)
		{
			c=LOG_ALERT;
			p=buf+6;
		}
		else if (strncmp(buf, "CRIT:", 5) == 0)
		{
			c=LOG_CRIT;
			p=buf+5;
		}
		else if (strncmp(buf, "DEBUG:", 6) == 0)
		{
			c=LOG_DEBUG;
			p=buf+6;
		}
		else if (strncmp(buf, "INFO:", 5) == 0)
			p=buf+5;
		else	p=buf;

		while (*p == ' ')
			++p;

		syslog(c, "%s", p);
#endif
	}
	return (0);
}

static const char *namearg=0;
static const char *pidarg=0;
static char *lockfilename=0;

struct lognames
{
	char *name;
	int value;
};

static struct lognames facilitynames[] =
{
#ifdef LOG_AUTH
	{ "auth",	LOG_AUTH },
#endif
#ifdef LOG_AUTHPRIV
	{ "authpriv",	LOG_AUTHPRIV },
#endif
#ifdef LOG_CONSOLE
	{ "console",	LOG_CONSOLE },
#endif
#ifdef LOG_CRON
	{ "cron",	LOG_CRON },
#endif
#ifdef LOG_DAEMON
	{ "daemon",	LOG_DAEMON },
#endif
#ifdef LOG_FTP
	{ "ftp",	LOG_FTP },
#endif
#ifdef LOG_KERN
	{ "kern",	LOG_KERN },
#endif
#ifdef LOG_LPR
	{ "lpr",	LOG_LPR },
#endif
#ifdef LOG_MAIL
	{ "mail",	LOG_MAIL },
#endif
#ifdef LOG_AUTHPRIV
	{ "news",	LOG_NEWS },
#endif
#ifdef LOG_SECURITY
	{ "security",	LOG_SECURITY },
#endif
#ifdef LOG_USER
	{ "user",	LOG_USER },
#endif
#ifdef LOG_UUCP
	{ "uucp",	LOG_UUCP },
#endif
#ifdef LOG_LOCAL0
	{ "local0",	LOG_LOCAL0 },
#endif
#ifdef LOG_LOCAL1
	{ "local1",	LOG_LOCAL1 },
#endif
#ifdef LOG_LOCAL2
	{ "local2",	LOG_LOCAL2 },
#endif
#ifdef LOG_LOCAL3
	{ "local3",	LOG_LOCAL3 },
#endif
#ifdef LOG_LOCAL4
	{ "local4",	LOG_LOCAL4 },
#endif
#ifdef LOG_LOCAL5
	{ "local5",	LOG_LOCAL5 },
#endif
#ifdef LOG_LOCAL6
	{ "local6",	LOG_LOCAL6 },
#endif
#ifdef LOG_LOCAL7
	{ "local7",	LOG_LOCAL7 },
#endif
	{ 0,		0 }
};

static int hup_restart = 0;
static int respawn = 0;
static int child_pid = -1;

static RETSIGTYPE sighup(int n)
{
	if (child_pid > 0)
	{
		/* The child may respond to HUP by dying. If so it will
		   close its stderr and do_logger will terminate; at that
		   point we need to restart it */
		hup_restart = 1;
		kill (child_pid, SIGHUP);
	}
	signal(SIGHUP, sighup);
#if	RETSIGTYPE != void
	return (1);
#endif
}

static RETSIGTYPE sigalrm(int n)
{
	if (child_pid > 0) kill(child_pid, SIGKILL);
#if	RETSIGTYPE != void
	return (1);
#endif
}

static RETSIGTYPE sigterm(int n)
{
	if (child_pid > 0)
	{
		hup_restart = 0;
		respawn = 0;
		kill(child_pid, SIGTERM);
		signal(SIGALRM, sigalrm); /* kill after 8 secs */
		alarm(8);
	}
	else exit(0);
#if	RETSIGTYPE != void
	return (1);
#endif
}

static void checkfd(int actual, int exp)
{
	if (actual == exp) return;
	fprintf(stderr, "Bad fd, got %d, expected %d\n", actual, exp);
	if (actual < 0) perror("error");
	exit(1);
}

static int isid(const char *p)
{
	while (*p)
	{
		if (*p < '0' || *p > '9')	return (0);
		++p;
	}
	return (1);
}

static void setuidgid(const char *userarg,
		      const char *grouparg)
{
	if (grouparg)
	{
	gid_t gid = 0;
	struct group *gr;
	
		if (isid(grouparg))
			gid=atoi(grouparg);
		else if ((gr=getgrnam(grouparg)) == 0)
		{
			fprintf(stderr, "Group not found: %s\n", grouparg);
			exit(1);
		}
		else	gid=gr->gr_gid;

		libmail_changegroup(gid);
	}

	if (userarg)
	{
	uid_t	uid;

		if (isid(userarg))
		{
			uid=atoi(userarg);
			libmail_changeuidgid(uid, getgid());
		}
		else
		{
		gid_t	g=getgid(), *gp=0;

			if (grouparg)	gp= &g;
			libmail_changeusername(userarg, gp);
		}
	}
}


static void startchild(char **argv, const char *userarg, const char *grouparg)
{
pid_t p;
int pipefd[2];
int tmp;

	signal(SIGTERM, sigterm);
	signal(SIGHUP, sighup);

	/* Make sure the pipefds are at least 3 and 4. If we have an open
	   stderr then keep it around for debugging purposes. */
	close(0);
	checkfd(open("/dev/null", O_RDWR), 0);
	close(1);
	checkfd(dup(0), 1);
	if ((tmp = dup(0)) > 2) close(tmp);

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		exit(1);
	}
	p = fork();
	if (p < 0)
	{
		perror("fork");
		exit(1);
	}
	if (p == 0)
	{

		close(pipefd[0]);
		close(2);
		checkfd(dup(pipefd[1]), 2);
		close(pipefd[1]);
		setuidgid(userarg, grouparg);
		execvp(argv[0], argv);
		perror("exec");
		exit(1);
	}

	// We can close stderr now

	close(2);
	checkfd(dup(0), 2);


	close(pipefd[1]);
	close(0);
	checkfd(dup(pipefd[0]), 0);
	close(pipefd[0]);
	child_pid = p;
}
/*
 * Note that we now support several modes of operation:
 *
 * (1) standalone logger, just collect messages on stdin and feed to syslog
 *   courierlogger foo
 *   courierlogger -name=foo
 *
 * (2) run a child process, collect its stderr messages and feed to syslog
 *   courierlogger [-name=foo] foo arg1 arg2
 *
 * (3) start a detached daemon with a child process
 *   courierlogger [-name=foo] -pid=/var/run/foo.pid -start foo arg1 arg2
 *
 * (4) stop or restart a detached daemon
 *   courierlogger -pid=/var/run/foo.pid -stop      (or receive a SIGTERM)
 *   courierlogger -pid=/var/run/foo.pid -restart   (or receive a SIGHUP)
 */
 
int main(int argc, char **argv)
{
int facility = LOG_DEST;
int daemon = 0;
int lockfd = -1;
const char *userarg = 0;
const char *grouparg = 0;
int droproot=0;

	if (argc == 2 && argv[1][0] != '-')
	{
		/* backwards-compatibility mode */
		close(1);
		close(2);
		checkfd(open("/dev/null", O_WRONLY), 1);
		checkfd(open("/dev/null", O_WRONLY), 2);
		do_logger(argv[1], facility, stdin);
		exit(0);
	}

	if (argc <= 1)
	{
		fprintf(stderr,

			"Usage: courierlogger [-name=name] [-pid=pidfile] [-facility=type]\n"
			"       [-start|-stop|-restart] [cmd [args...]]\n"
		);
		exit(1);
	}

	argv++, argc--;
	while (argc > 0 && argv[0][0]=='-')
	{
		if (strncmp(argv[0],"-pid=",5) == 0 && argv[0][5])
		{
			pidarg=&argv[0][5];
			lockfilename=malloc(strlen(pidarg)+sizeof(".lock"));
			if (!lockfilename)
			{
				perror("malloc");
				exit(1);
			}
			strcat(strcpy(lockfilename, pidarg), ".lock");
		}
		else
		if (strncmp(argv[0],"-name=",6) == 0 && argv[0][6])
			namearg=&argv[0][6];
		else if (strncmp(argv[0],"-user=",6) == 0)
			userarg=argv[0]+6;
		else if (strncmp(argv[0],"-group=",7) == 0)
			grouparg=argv[0]+7;
		else if (strcmp(argv[0], "-droproot") == 0)
			droproot=1;
		else
		if (strncmp(argv[0],"-facility=",10) == 0)
		{
		struct lognames *p = facilitynames;

			while (p->name && strcmp(p->name, &argv[0][10]))
				p++;
			if (p->name == 0)
			{
				fprintf(stderr, "Unknown facility name '%s'\n",
					&argv[0][10]);
				exit(1);
			}
			facility = p->value;
		}
		else
		if (strcmp(argv[0],"-start") == 0)
			daemon = 1;
		else
		if (strcmp(argv[0],"-stop") == 0)
			daemon = 2;
		else
		if (strcmp(argv[0],"-restart") == 0)
			daemon = 3;
		else
		if (strcmp(argv[0],"-respawn") == 0)
			respawn = 1;
		else
		{
			fprintf(stderr, "Unknown option '%s'\n", argv[0]);
			exit(1);
		}
		argv++, argc--;
	}

	if (daemon && !pidarg)
	{
		fprintf(stderr, "-pid argument required\n");
		exit(1);
	}

	if (!daemon && pidarg)
		daemon = 1; /* -start implied */

	if (!namearg && daemon != 2 && daemon != 3)
	{
		/* choose a default name based on the program we're running */
		if (argc <= 0 || !argv[0] || !argv[0][0])
		{
			fprintf(stderr, "-name option required for standalone logger\n");
			exit(1);
		}
		namearg = strrchr(argv[0],'/');
		namearg = namearg ? namearg+1 : argv[0];
	}

	switch (daemon)
	{
	case 1: /* start */
		if (argc <= 0 || !argv[0] || !argv[0][0])
		{
			fprintf(stderr, "-start must be followed by a command to execute\n");
			exit(1);
		}
		lockfd=ll_daemon_start(lockfilename);
		if (lockfd < 0)
		{
			perror("ll_daemon_start");
			exit(1);
		}
		startchild(argv, droproot ? userarg:NULL,
			   droproot ? grouparg:NULL);
		ll_daemon_started(pidarg, lockfd);
		break;
	case 2: /* stop */
		exit(ll_daemon_stop(lockfilename, pidarg));
	case 3: /* restart */
		exit(ll_daemon_restart(lockfilename, pidarg));
	default: /* run in foreground, with or without a child process */

		if (argc > 0)
			startchild(argv, droproot ? userarg:NULL,
				   droproot ? grouparg:NULL);
	}

	setuidgid(userarg, grouparg);

	while (1)
	{
	int waitstat;
	pid_t p2;
	FILE *f = fdopen(0, "r");

		do_logger(namearg, facility, f);
		fclose(f);
		if (child_pid < 0) break;
		while ((p2=wait(&waitstat)) != child_pid &&
			(p2 != -1 || errno != ECHILD))
			;
		if (hup_restart)
			hup_restart = 0;
		else if (respawn)
			sleep (5);
		else
			break;
		startchild(argv, NULL, NULL);
	}
	exit(0);
}
