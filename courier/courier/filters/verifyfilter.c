/*
** Copyright 2017 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"bindir.h"
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	"comctlfile.h"
#include	"filtersocketdir.h"
#include	"threadlib/threadlib.h"

#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#include	"courier.h"
#include	"smtproutes.h"

#include	"libfilter/libfilter.h"
#include	"rfc1035/rfc1035.h"
#include	"module.esmtp/libesmtp.h"

void rfc2045_error(const char *p)
{
	fprintf(stderr, "%s\n", p);
	exit(1);
}

struct verify_info {

	char	sent[256];
	char	server[300];
	char	msgbuf[1024];
	int	errcode;
	int	broken_starttls;
};

struct verify_info *verify_info_init()
{
	struct verify_info *m=malloc(sizeof(struct verify_info));

	if (!m)
		abort();
	memset(m, '\0', sizeof(*m));

	return m;
}

void verify_info_deinit(struct verify_info *p)
{
	free(p);
}

static void log_talking(struct esmtp_info *info, void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;
	char	address[RFC1035_NTOABUFSIZE];

	esmtp_sockipname(info, address);

	snprintf(my_info->server, sizeof(my_info->server),
		 "%s%s[%s]", info->sockfdaddrname ? info->sockfdaddrname:"",
		 (info->sockfdaddrname ? " ":""), address);
}

static void log_sent(struct esmtp_info *info,
		     const char *command,
		     int rcpt_num,
		     void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;

	snprintf(my_info->sent, sizeof(my_info->sent), "%s", command);
}

static void add_reply(struct verify_info *info,
		      const char *msg)
{
	size_t l=sizeof(info->msgbuf)-1-strlen(info->msgbuf);

	if (l)
		strncat(info->msgbuf, msg, l);
}

static void log_reply(struct esmtp_info *info,
		      const char *reply, int rcpt_num, void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;

	add_reply(my_info, reply);
	add_reply(my_info, "\n");
}

static void log_net_error(struct esmtp_info *info, int rcpt_num, void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;

	add_reply(my_info, "Error: ");
	add_reply(my_info, strerror(errno));
	add_reply(my_info, "\n");
}

static void log_rcpt_error(struct esmtp_info *info, int n, int errcode,
			   void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;

	my_info->errcode=errcode;
}

static void log_smtp_error(struct esmtp_info *info,
			   const char *msg,
			   int errcode,
			   void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;

	my_info->errcode=errcode;
	add_reply(my_info, msg);
	add_reply(my_info, "\n");
}


static void report_broken_starttls(struct esmtp_info *info,
				   const char *address,
				   void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;

	my_info->broken_starttls=1;
}

static int lookup_broken_starttls(struct esmtp_info *info,
				  const char *address,
				  void *arg)
{
	struct verify_info *my_info=(struct verify_info *)arg;

	return my_info->broken_starttls;
}

static void do_verify(struct verify_info *my_info,
		      const char *address, int in_environment)
{
	const char *domain=strrchr(address, '@');
	struct esmtp_info *info;
	int smtproutes_flags=0;
	char *smtproute=0;

	if (!domain)
	{
		strcpy(my_info->msgbuf, "500 Invalid email address\n");
		my_info->errcode='5';
		return;
	}

	if (in_environment)
	{
		smtproute=smtproutes(domain+1, &smtproutes_flags);
	}

	info=esmtp_info_alloc(domain+1, smtproute, smtproutes_flags);
	if (smtproute)
		free(smtproute);

	if (in_environment)
	{
		char *q;

		q=config_localfilename("esmtpauthclient");
		esmtp_setauthclientfile(info, q);
		free(q);
		info->helohost=config_esmtphelo();
	}

	// TODO: helohost

	info->log_talking=log_talking;
	info->log_sent=log_sent;
	info->log_smtp_error=log_smtp_error;
	info->log_rcpt_error=log_rcpt_error;
	info->log_net_error=log_net_error;
	info->log_smtp_error=log_smtp_error;
	info->log_reply=log_reply;
	info->report_broken_starttls=report_broken_starttls;
	info->lookup_broken_starttls=lookup_broken_starttls;

	if (esmtp_connect(info, my_info) == 0 &&
	    esmtp_misccommand(info, "MAIL FROM:<>", my_info) == 0)
	{
		char *rcpt_to_cmd=malloc(sizeof("RCPT TO:<>")+strlen(address));

		if (!rcpt_to_cmd)
			abort();

		strcat(strcat(strcpy(rcpt_to_cmd, "RCPT TO:<"), address),
		       ">");
		info->log_good_reply=log_reply;
		esmtp_misccommand(info, rcpt_to_cmd, my_info);
		free(rcpt_to_cmd);
		info->log_good_reply=NULL;
	}

	if (my_info->errcode == 0)
	{
		esmtp_quit(info, my_info);
		my_info->errcode=0;
	}
	else
	{
		if (esmtp_connected(info) && !my_info->server[0])
			log_talking(info, my_info);

		esmtp_quit(info, my_info);
	}
	esmtp_info_free(info);
}

static struct verify_info *verify(const char *address, int in_environment)
{
	struct verify_info *my_info=verify_info_init();
	size_t l;

	do_verify(my_info, address, in_environment);

	if (my_info->broken_starttls)
	{
		verify_info_deinit(my_info);
		my_info=verify_info_init();

		my_info->broken_starttls=1;
		do_verify(my_info, address, in_environment);
	}

	l=strlen(my_info->msgbuf);

	if (l && my_info->msgbuf[--l] == '\n')
		my_info->msgbuf[l]=0;

	return my_info;
}

void format_as_smtp(FILE *statusfp, struct verify_info *info,
		    const char *sender)
{
	const char *status=info->errcode == '5' ? "530":"430";
	char *p;
	char *msg=info->msgbuf;

	if (info->server[0])
		fprintf(statusfp, "%s-%s:\n",
			status, info->server);
	if (info->sent[0])
		fprintf(statusfp, "%s->>> %s\n",
			status, info->sent);

	while ((p=strtok(msg, "\n")) != 0)
	{
		if (isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2]))
		{
			p += 3;
			if (*p == ' ' || *p == '-')
				++p;
		}

		fprintf(statusfp, "%s-%s\n", status, p);
		msg=NULL;
	}
	fprintf(statusfp, "%s <%s> verification failed.\n",
		status, sender);
}

void lookup(int argc, char **argv)
{
	int i;
	int exitcode=0;

	if (argc == 2 && strcmp(argv[1], ".") == 0)
	{
		const char *sender=getenv("SENDER");
		struct verify_info *info;

		/* Called from maildroprcs */

		if (!sender) sender="";

		if (!*sender)
			exit(0);

		info=verify(sender, 0);

		if (info->errcode == 0)
		{
			verify_info_deinit(info);
			exit(0);
		}
		format_as_smtp(stdout, info, sender);
		verify_info_deinit(info);
		exit(1);
	}


	for (i=1; i<argc; ++i)
	{
		struct verify_info *info;

		if (i > 1)
			printf("\n");

		printf("<%s>: ", argv[i]);
		fflush(stdout);

		info=verify(argv[i], 0);

		if (info->errcode == 0)
		{
			char *msg=strtok(info->msgbuf, "\n");
			char *p;

			while ((p=strtok(NULL, "\n")) != 0)
				msg=p;

			printf("%s\n", msg ? msg:"Ok");
		}
		else
		{
			char *msg=info->msgbuf;
			char *p;

			printf("verification failed\n");

			if (info->sent[0])
				printf("    >>> %s:\n    >>> %s\n",
				       info->server, info->sent);
			while ((p=strtok(msg, "\n")) != 0)
			{
				printf("    %s\n", p);
				msg=NULL;
			}
			exitcode=1;
		}
		verify_info_deinit(info);
	}
	exit(exitcode);
}

void search_sender(FILE *ctlfile,
		   FILE *status_socket)
{
	char buf[BUFSIZ];
	char sender[BUFSIZ];
	int authenticated_sender=0;
	struct verify_info *info;

	sender[0]=0;

	if (ctlfile)
	{
		while (fgets(buf, sizeof(buf), ctlfile))
		{
			switch (buf[0]) {
			case COMCTLFILE_SENDER:
				{
					char *p=strtok(buf+1, "\r\n");

					if (p)
						strcpy(sender, p);
				}
				break;
			case COMCTLFILE_AUTHNAME:
				authenticated_sender=1;
				break;
			}
		}
	}

	if (sender[0] && !authenticated_sender)
	{
		info=verify(sender, 1);

		if (info->errcode)
		{
			format_as_smtp(status_socket, info, sender);
			verify_info_deinit(info);
			return;
		}
		verify_info_deinit(info);
	}

	fprintf(status_socket, "200 Ok\n");
}

struct verify_thread_info {

	int fd;
};

static void initverifyinfo(struct verify_thread_info *a,
			   struct verify_thread_info *b)
{
	a->fd=b->fd;
}

static FILE *get_first_ctlfile(FILE *fp)
{
	FILE *ctlfile;
	char buf[BUFSIZ];
	char *p;

	buf[0]=0;
	if (fgets(buf, sizeof(buf), fp)) // Data file.
	{
		if (!fgets(buf, sizeof(buf), fp)) // First control file.
			buf[0]=0;
	}

	p=strtok(buf, "\n");

	ctlfile=p ? fopen(p, "r"):NULL;

	// Find the empty line that indicates the end of the control files.
	while (fgets(buf, sizeof(buf), fp))
	{
		char *p=strtok(buf, "\r\n");

		if (!p)
			break;
	}

	return ctlfile;
}

static void verifythread(struct verify_thread_info *vti)
{
	FILE *fp=fdopen(vti->fd, "w+");
	FILE *ctlfile;

	if (!fp)
	{
		perror("fopen");
		close(vti->fd);
		return;
	}

	ctlfile=get_first_ctlfile(fp);

	search_sender(ctlfile, fp);

	if (ctlfile)
		fclose(ctlfile);
	fclose(fp);
}

static void verifyfinish(struct verify_thread_info *ignored)
{
}

int verifyfilter(unsigned nthreads)
{
	int	listensock;
	struct	cthreadinfo *threads;
	struct	verify_thread_info vti;

	listensock=lf_init("filters/verifyfilter-mode",
		ALLFILTERSOCKETDIR "/verifyfilter",
		ALLFILTERSOCKETDIR "/.verifyfilter",
		FILTERSOCKETDIR "/verifyfilter",
		FILTERSOCKETDIR "/.verifyfilter");

	if (listensock < 0)
		return (1);

	threads=cthread_init(nthreads, sizeof(struct verify_thread_info),
			     (void (*)(void *))&verifythread,
			     (void (*)(void *))&verifyfinish);
	if (!threads)
	{
		perror("cthread_init");
		return (1);
	}

	lf_init_completed(listensock);

	for (;;)
	{
		if ((vti.fd=lf_accept(listensock)) < 0)	break;

		if ( cthread_go(threads,
				(void (*)(void *, void *))&initverifyinfo,
				&vti))
		{
			perror("cthread_go");
			break;
		}
	}
	cthread_wait(threads);
	return (0);
}

int main(int argc, char **argv)
{
	char	*fn;
	char	*f;

	unsigned nthreads=4;

	if (!lf_initializing())
	{
		if (!getenv("COURIERTLS"))
		{
			putenv("COURIERTLS=" BINDIR "/couriertls");
			putenv("ESMTP_USE_STARTTLS=1");

			if (!getenv("TLS_VERIFYPEER"))
				putenv("TLS_VERIFYPEER=NONE");
		}
		lookup(argc, argv);
	}

	fn=config_localfilename("filters/verifyfilter-nthreads");

	if ( (f=config_read1l(fn)) != 0)
	{
		sscanf(f, "%u", &nthreads);
		free(f);
	}
	free(fn);

	verifyfilter(nthreads);
}
