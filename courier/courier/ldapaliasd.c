/*
** Copyright 2000-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include	"numlib/numlib.h"
#if TIME_WITH_SYS_TIME
#include        <sys/time.h>
#include        <time.h>
#else
#if HAVE_SYS_TIME_H
#include        <sys/time.h>
#else
#include        <time.h>
#endif
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_LBER_H
#include <lber.h>
#endif
#if HAVE_LBER_H
#include <lber.h>
#endif
#if HAVE_LDAP_H
#include <ldap.h>
#if LDAP_VENDOR_VERSION > 20000
#define OPENLDAPV2
#endif
#endif
#if HAVE_SYSLOG_H
#include <syslog.h>
#else
#define syslog(a,b)
#endif
#include <sys/socket.h>
#include <sys/un.h>

#include	<courierauth.h>
#include	"courier.h"
#include	"liblock/config.h"
#include	"liblock/liblock.h"
#include	"ldapaliasdrc.h"
#include	"sysconfdir.h"
#include	"localstatedir.h"


static LDAP *my_ldap_fp=0;
static const char *basedn;
static const char *mailfield;
static const char *maildropfield;
static const char *sourcefield;
static const char *vdomain;
static const char *vuser;
static int timeout_n;


/*
** There's a memory leak in OpenLDAP 1.2.11, presumably in earlier versions
** too.  See http://www.OpenLDAP.org/its/index.cgi?findid=864 for more
** information.  To work around the bug, the first time a connection fails
** we stop trying for 60 seconds.  After 60 seconds we kill the process,
** and let the parent process restart it.
**
** We'll control this behavior via LDAP_MEMORY_LEAK.  Set it to ZERO to turn
** off this behavior (whenever OpenLDAP gets fixed).
*/

static time_t ldapfailflag=0;

static void ldapconnfailure()
{
	const char *p=getenv("LDAP_MEMORY_LEAK");

	if (!p) p="1";

	if (atoi(p) && !ldapfailflag)
	{
		time(&ldapfailflag);
		ldapfailflag += 60;
	}
}

static int ldapconncheck()
{
	time_t t;

	if (!ldapfailflag)
		return (0);

	time(&t);

	if (t >= ldapfailflag)
		exit(0);
	return (1);
}

static void ldapclose()
{
	if (my_ldap_fp)
	{
		ldap_unbind_ext(my_ldap_fp, NULL, NULL);
		my_ldap_fp=0;
	}
}

static int ldaperror(int rc)
{
#ifdef OPENLDAPV2
	if (rc && !LDAP_NAME_ERROR(rc))
#else
	if (rc && !NAME_ERROR(rc))
#endif
	{
		/* If there was a protocol error, close the connection */
		ldapclose();
		ldapconnfailure();
	}
	return (rc);
}

static LDAP *ldapconnect()
{
	LDAP	*p;

	const char *url_s=ldapaliasd_config("LDAP_URI");

	if (!*url_s)
	{
#if	HAVE_SYSLOG_H
		syslog(LOG_DAEMON|LOG_CRIT,
		       "Cannot read LDAP_URI from %s",
		       LDAPALIASDCONFIGFILE);
#endif
		return (0);
	}

	if (ldapconncheck())
		return (NULL);

	if (ldap_initialize(&p, url_s) != LDAP_SUCCESS)
	{
#if	HAVE_SYSLOG_H
		syslog(LOG_DAEMON|LOG_CRIT,
			"Cannot connect to %s: %s",
			url_s, strerror(errno));
#endif
		ldapconnfailure();
	}
	return (p);
}

static int ldapopen()
{
	int     ldrc;
	const char *binddn_s=ldapaliasd_config("LDAP_BINDDN");
	const char *bindpw_s=ldapaliasd_config("LDAP_BINDPW");
	struct berval cred;

	basedn=ldapaliasd_config("LDAP_BASEDN");

	if ((timeout_n=atoi(ldapaliasd_config("LDAP_TIMEOUT"))) <= 0)
	{
#if	HAVE_SYSLOG_H
		syslog(LOG_DAEMON|LOG_CRIT,
			"Cannot read LDAP_TIMEOUT from %s",
		       LDAPALIASDCONFIGFILE);
#endif
		return (-1);
	}


	my_ldap_fp=ldapconnect();

	if (!my_ldap_fp)
	{
		return (-1);
	}

	if (!*binddn_s)
	{
#if 0
#if	HAVE_SYSLOG_H
		syslog(LOG_DAEMON|LOG_CRIT,
			"Cannot read LDAP_BINDDN from %s",
		       LDAPALIASDCONFIGFILE);
#endif
#endif
		return (0);
	}

	cred.bv_len=strlen(bindpw_s ? bindpw_s:"");
	cred.bv_val=bindpw_s ? (char *)bindpw_s:"";


	if (ldaperror(ldrc = ldap_sasl_bind_s(my_ldap_fp, binddn_s, NULL,
					      &cred, NULL, NULL, NULL))
	    != LDAP_SUCCESS)
	{
		const char *s=ldap_err2string(ldrc);

#if	HAVE_SYSLOG_H
		syslog(LOG_DAEMON|LOG_CRIT,
		       "ldap_simple_bind_s failed: %s", s);
#endif
		ldapclose();
		ldapconnfailure();
		return (-1);
	}
	return (0);
}

static LDAPMessage *search_attr_retry(const char *filter, const char *attr)
{
	char *attrlist[2];
	struct timeval timeout;
	LDAPMessage *result;

	attrlist[0]=(char *)attr;
	attrlist[1]=0;

	timeout.tv_sec=timeout_n;
	timeout.tv_usec=0;

	if (ldaperror(ldap_search_ext_s(my_ldap_fp, (char *)basedn,
					LDAP_SCOPE_SUBTREE,
					(char *)filter, attrlist,
					0,
					NULL, NULL, &timeout,
					1000000,
					&result))
	    != LDAP_SUCCESS)
		return (NULL);
	return (result);
}

static LDAPMessage *search_attr(const char *filter, const char *attr)
{
	LDAPMessage *result=search_attr_retry(filter, attr);

	if (result == NULL && my_ldap_fp == NULL && ldapopen() == 0)
		result=search_attr_retry(filter, attr);

	return result;
}

static int search_maildrop2(const char *mail, const char *source, FILE *outfp);

static int search_maildrop(const char *mail, const char *source, FILE *outfp)
{
	int rc;
	char *escaped=courier_auth_ldap_escape(mail);

	if (!escaped)
	{
		syslog(LOG_DAEMON|LOG_CRIT, "malloc failed: %m");
		return(0);
	}

	rc=search_maildrop2(escaped, source, outfp);
	free(escaped);
	return rc;
}

static int search_maildrop2(const char *mail, const char *source, FILE *outfp)
{
	char *filter;
	char *p;
	LDAPMessage *result;
	int rc=1;

	if (my_ldap_fp == 0)
	{
		if (ldapopen())
			return(0);
	}

	filter=malloc(strlen(mail)+(source ? strlen(source):0)+
		      (sourcefield ? strlen(sourcefield):0)+
		      strlen(mailfield)+80);
	if (!filter)
	{
		syslog(LOG_DAEMON|LOG_CRIT, "malloc failed: %m");
		return (0);
	}

	strcpy(filter, "(&(");
	strcat(filter, mailfield);
	strcat(filter, "=");

	p=filter+strlen(filter);
	while (*mail)
	{
		if (*mail != '\\' && *mail != '(' && *mail != ')')
			*p++=*mail;
		++mail;
	}
	strcpy(p, ")");

	if (source)
	{
		strcat(filter,"(");
		strcat(filter, sourcefield);
		strcat(filter, "=");
		p=filter+strlen(filter);
		while (*source)
		{
			if (*source != '\\' && *source != '('
			    && *source != ')')
				*p++= *source;
			++source;
		}
		strcpy(p, ")");
	}
	else if (*sourcefield) {
		strcat(filter, "(!(");
		strcat(filter, sourcefield);
		strcat(filter, "=*))");
	}
	strcat(filter, ")");

	result=search_attr(filter, maildropfield);
	free(filter);

	if (!result)
		return (0);	/* Protocol error. */

	if (ldap_count_entries(my_ldap_fp, result) == 1)
	{
		LDAPMessage *entry;
		struct berval **values;

		if ((entry=ldap_first_entry(my_ldap_fp, result)) != NULL &&
		    (values=ldap_get_values_len(my_ldap_fp, entry,
						(char *)
						maildropfield)) != NULL)
		{
			int n=ldap_count_values_len(values);
			int i;

			for (i=0; i<n; i++)
			{
				int rc;

				fprintf(outfp, "maildrop: ");
				if ((rc=fwrite(values[i]->bv_val,
					       values[i]->bv_len,
					       1, outfp)) < 0)
					break;
				fprintf(outfp, "\n");
				rc=0;
			}
			ldap_value_free_len(values);
		}
	}
	ldap_msgfree(result);

	if (rc == 0)
		fprintf(outfp, ".\n");
	return (rc);
}

static int search_virtual(const char *mail, const char *source, FILE *outfp)
{
	char *filter;
	char *p;
	LDAPMessage *result;
	int rc=1;
	const char *domain;

	if (*vdomain == 0 || *vuser == 0)
		return (1);

	domain=strrchr(mail, '@');

	if (domain == 0)
		return (1);

	if (my_ldap_fp == 0)
	{
		if (ldapopen())
			return(0);
	}

	filter=malloc(strlen(mail)+(source ? strlen(source):0)+
		      (sourcefield ? strlen(sourcefield):0)+
		      strlen(vdomain)+80);
	if (!filter)
	{
		syslog(LOG_DAEMON|LOG_CRIT, "malloc failed: %m");
		return (0);
	}

	strcpy(filter, "(&(");
	strcat(filter, vdomain);
	strcat(filter, "=");

	p=filter+strlen(filter);
	++domain;	/* Skip past '@' */
	while (*domain)
	{
		if (*domain != '\\' && *domain != '(' && *domain != ')')
			*p++=*domain;
		++domain;
	}
	strcpy(p, ")");

	if (source)
	{
		strcat(filter,"(");
		strcat(filter, sourcefield);
		strcat(filter, "=");
		p=filter+strlen(filter);
		while (*source)
		{
			if (*source != '\\' && *source != '('
			    && *source != ')')
				*p++= *source;
			++source;
		}
		strcpy(p, ")");
	}
	else if (*sourcefield) {
		strcat(filter, "(!(");
		strcat(filter, sourcefield);
		strcat(filter, "=*))");
	}
	strcat(filter, ")");

	result=search_attr(filter, vuser);

	if (!result)
	{
		free(filter);
		return (0);	/* Protocol error. */
	}

	strcpy(filter, mail);
	*strrchr(filter, '@')=0;

	if (ldap_count_entries(my_ldap_fp, result) == 1)
	{
		LDAPMessage *entry;
		struct berval **values;

		if ((entry=ldap_first_entry(my_ldap_fp, result)) != NULL &&
		    (values=ldap_get_values_len(my_ldap_fp, entry,
						(char *)
						vuser)) != NULL)
		{
			int n=ldap_count_values_len(values);
			int i;

			for (i=0; i<n; i++)
			{
				int rc;

				fprintf(outfp, "maildrop: ");
				if ((rc=fwrite(values[i]->bv_val,
					       values[i]->bv_len,
					       1, outfp)) < 0)
					break;

				fprintf(outfp, "-%s\n", filter);
				rc=0;
			}
			ldap_value_free_len(values);
		}
	}
	ldap_msgfree(result);
	free(filter);

	if (rc == 0)
		fprintf(outfp, ".\n");
	return (rc);
}

static void search_source(char *source, FILE *outfp)
{
	char *query;

	for (query=source; *query; query++)
	{
		if (isspace((int)(unsigned char)*query))
		{
			*query++=0;
			while (*query && isspace((int)(unsigned char)*query))
				++query;
			break;
		}
	}

	if (!*query)
		return;

	if (sourcefield && *sourcefield &&
	    search_maildrop(query, source, outfp) == 0)
		return;

	if (sourcefield && *sourcefield &&
	    search_virtual(query, source, outfp) == 0)
		return;

	if (search_maildrop(query, NULL, outfp) &&
	    search_virtual(query, NULL, outfp))
		fprintf(outfp, ".\n");
}

static int mksocket()
{
	int     fd=socket(PF_UNIX, SOCK_STREAM, 0);
	struct  sockaddr_un skun;

        if (fd < 0)     return (-1);
        skun.sun_family=AF_UNIX;
        strcpy(skun.sun_path, SOCKETFILE);
        strcat(skun.sun_path, ".tmp");
        unlink(skun.sun_path);

        if (bind(fd, (const struct sockaddr *)&skun, sizeof(skun)) ||
	    listen(fd, SOMAXCONN) ||
	    chmod(skun.sun_path, 0770) ||
	    rename(skun.sun_path, SOCKETFILE) ||
	    fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
        {
                close(fd);
                return (-1);
        }
        return (fd);
}

static int pipe_tochild;
static int pipe_fromchildren;

static void child(int fd)
{
	int afd;

	sourcefield=ldapaliasd_config("LDAP_SOURCE");
	mailfield=ldapaliasd_config("LDAP_MAIL");
	maildropfield=ldapaliasd_config("LDAP_MAILDROP");
	if (!*mailfield)
		mailfield="mail";
	if (!*maildropfield)
		maildropfield="maildrop";

	vdomain=ldapaliasd_config("LDAP_VDOMAIN");
	vuser=ldapaliasd_config("LDAP_VUSER");

	for (;;)
	{
		struct sockaddr saddr;
		socklen_t	saddr_len;
		FILE	*fp;
		char	buf[BUFSIZ];
		char	*p;
		fd_set  fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		FD_SET(pipe_tochild, &fds);

		tv.tv_sec=300;
		tv.tv_usec=0;

		if (select((fd > pipe_tochild ? fd:pipe_tochild)+1,
			   &fds, 0, 0, &tv) < 0)
		{
                        syslog(LOG_CRIT, "select() failed: %m");
			return;
		}

		if (FD_ISSET(pipe_tochild, &fds))
			return;
		if (!FD_ISSET(fd, &fds))
		{
			ldapclose();
			continue;
		}

                saddr_len=sizeof(saddr);
                if ((afd=accept(fd, &saddr, &saddr_len)) < 0)
                        continue;
                if (fcntl(afd, F_SETFL, 0) < 0)
                {
                        syslog(LOG_CRIT, "fcntl() failed: %m");
			close(afd);
			continue;
		}

		fp=fdopen(afd, "r+");
		if (!fp)
		{
                        syslog(LOG_CRIT, "fdopen() failed: %m");
			close(afd);
			continue;
		}

		if (fgets(buf, sizeof(buf), fp) == NULL)
		{
			fclose(fp);
			continue;
		}
		if ((p=strchr(buf, '\n')) != 0)
			*p=0;
		search_source(buf, fp);
		fclose(fp);
	}
}

static int sigterm_received;
static int sighup_received;
static int sigchild_received;

static RETSIGTYPE sigalarm(int signum)
{
	signal(SIGALRM, sigalarm);
	alarm(2);

#if     RETSIGTYPE != void
        return (0);
#endif
}

static RETSIGTYPE sigterm(int signum)
{
	sigterm_received=1;

#if     RETSIGTYPE != void
        return (0);
#endif
}

static RETSIGTYPE sighup(int signum)
{
	sighup_received=1;

	signal(SIGHUP, sighup);

#if     RETSIGTYPE != void
        return (0);
#endif
}

static RETSIGTYPE sigchild(int signum)
{
	sigchild_received=1;
	alarm(1);

#if     RETSIGTYPE != void
        return (0);
#endif
}

static void loop(int fd)
{
	int afd;

	int pipea[2];
	int pipeb[2];

	if (pipe(pipea) < 0)
	{
		syslog(LOG_CRIT, "pipe() failed: %m");
		sleep(5);
	}

	if (pipe(pipeb) < 0)
	{
		close(pipea[0]);
		close(pipea[1]);
		syslog(LOG_CRIT, "pipe() failed: %m");
		sleep(5);
		return;
	}

	afd=atoi(ldapaliasd_config("LDAP_NUMPROCS"));

	if (afd <= 0)
		afd=5;

	signal(SIGALRM, sigalarm);
	signal(SIGCHLD, sigchild);

	while (afd)
	{
		pid_t p=fork();

		if (p < 0)
		{
			char buf;

			syslog(LOG_CRIT, "pipe() failed: %m");
			close(pipea[0]);
			close(pipea[1]);
			close(pipeb[1]);

			if (read(pipeb[0], &buf, 1) < 0)
				; /* suppress gcc warning */

			close(pipeb[0]);
			sleep (5);
			return;
		}

		if (p == 0)
		{
			alarm(0);
			signal(SIGCHLD, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			signal(SIGHUP, SIG_DFL);
			signal(SIGALRM, SIG_DFL);
			pipe_tochild=pipea[0];
			close(pipea[1]);
			pipe_fromchildren=pipeb[1];
			close(pipeb[0]);
			child(fd);
			exit(0);
		}
		--afd;
	}

	sighup_received=0;
	sigterm_received=0;
	sigchild_received=0;
	pipe_tochild=pipea[1];
	close(pipea[0]);
	pipe_fromchildren=pipeb[0];
	close(pipeb[1]);

	signal(SIGTERM, sigterm);
	signal(SIGHUP, sighup);

	while (!sighup_received && !sigterm_received &&
	       !sigchild_received)
	{
		sigset_t ss;

		sigemptyset(&ss);
		sigaddset(&ss, SIGUSR1);

		sigsuspend(&ss);
	}

	/* should be sigpause(0), but Solaris is broken */

	alarm(0);
	signal(SIGALRM, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGHUP, SIG_IGN);

#if 0
#if USE_NOCLDWAIT

	{
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler=SIG_IGN;
		sa.sa_flags=SA_NOCLDWAIT;
		sigaction(SIGCHLD, &sa, NULL);
	}
#else
	signal(SIGCHLD, SIG_IGN);
#endif
#else
	signal(SIGCHLD, SIG_DFL);
#endif

	close(pipe_tochild);

	if (sighup_received)
		syslog(LOG_DAEMON|LOG_INFO,
		       "Terminating child processes after a SIGHUP");
	if (sigterm_received)
		syslog(LOG_DAEMON|LOG_INFO,
		       "Terminating child processes after a SIGTERM");
	if (sigchild_received)
		syslog(LOG_DAEMON|LOG_CRIT,
		       "Terminating child processes after a SIGCHILD");
	{
		char c;
		int waitstat;

		if (read(pipe_fromchildren, &c, 1) < 0)
			; /* suppress gcc warning */

		close(pipe_fromchildren);
		while (wait(&waitstat) >= 0)
			;
	}
	if (sighup_received)
		syslog(LOG_DAEMON|LOG_INFO,
		       "Terminated child processes after a SIGHUP");
	if (sigterm_received)
		syslog(LOG_DAEMON|LOG_INFO,
		       "Terminated child processes after a SIGTERM");
	if (sigchild_received)
		syslog(LOG_DAEMON|LOG_CRIT,
		       "Terminated child processes after a SIGCHILD");
	if (sigterm_received)
		exit(0);
	ldapaliasd_configchanged();
}

static void start()
{
	int lockfd;
	int sockfd;

	lockfd=ll_daemon_start(LOCKFILE);

	if (lockfd < 0)
	{
		perror("ll_daemon_start");
		exit(1);
	}

	if (atoi(ldapaliasd_config("LDAP_ALIAS")) <= 0)
	{
		ll_daemon_started(PIDFILE, lockfd);
		/* Consider this a good start */
		exit(0);
	}

	if ((sockfd=mksocket()) < 0)
	{
		perror("socket");
		exit(1);
	}

	signal(SIGPIPE, SIG_IGN);
	ll_daemon_started(PIDFILE, lockfd);
	ll_daemon_resetio();


	for (;;)
	{
		if (atoi(ldapaliasd_config("LDAP_ALIAS")) <= 0)
			exit(0);
		loop(sockfd);
	}
}

static void stop()
{
	ll_daemon_stop(LOCKFILE, PIDFILE);
}

static void restart()
{
	ll_daemon_restart(LOCKFILE, PIDFILE);
}

static const char *readline(FILE *fp)
{
	static char buf[BUFSIZ];
	char *p;

	if (fgets(buf, sizeof(buf), fp) == NULL)
	{
		fclose(fp);
		return (NULL);
	}
	if ((p=strchr(buf, '\n')) != 0)
		*p=0;
	return (buf);
}

static void check(const char *source, const char *query)
{
	FILE *fp=ldapaliasd_connect();
	int found=0;
	const char *p;

	if (!fp)
	{
		fprintf(stderr, "connect failed\n");
		exit(1);
	}

	fprintf(fp, "%s %s\n", source, query);
	fflush(fp);

	while ((p=readline(fp)) != NULL)
	{
		if (strcmp(p, ".") == 0)
		{
			if (!found)
			{
				fprintf(stderr, "%s: not found\n", query);
				exit (1);
			}
			exit (0);
		}
		printf("%s\n", p);
		found=1;
	}

	fprintf(stderr, "Error.\n");
	exit (1);
}

int main(int argc, char **argv)
{
	if (chdir(COURIER_HOME) < 0)
	{
		perror(COURIER_HOME);
		exit(1);
	}

	libmail_changeuidgid(MAILUID, MAILGID);
	
#if HAVE_SYSLOG_H
	openlog("courierldapaliasd", LOG_PID, LOG_MAIL);
#endif

	if (argc > 1)
	{
		if (strcmp(argv[1], "start") == 0)
		{
			start();
			return (0);
		}

		if (strcmp(argv[1], "stop") == 0)
		{
			stop();
			return (0);
		}

		if (strcmp(argv[1], "restart") == 0)
		{
			restart();
			return (0);
		}

		if (strcmp(argv[1], "query") == 0 && argc > 3)
		{
			check(argv[2], argv[3]);
			return (0);
		}
	}

	fprintf(stderr, "Usage: %s [start|stop|restart]\n", argv[0]);
	return (1);
}
