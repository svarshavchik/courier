/*
** Copyright 2017 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"libesmtp.h"
#include	"smtproutes.h"
#include	<errno.h>
#include	<sys/types.h>
#include	<sys/uio.h>
#include	<sys/socket.h>
#include	<string.h>
#include	<stdlib.h>
#include	"courier.h"
#include	"numlib/numlib.h"
#include	"tcpd/spipe.h"
#include	"tcpd/tlsclient.h"
#include	<courierauthsaslclient.h>

int esmtp_sockfd;
time_t	esmtp_timeout_time;
struct mybuf esmtp_sockbuf;
char esmtp_writebuf[BUFSIZ];
char *esmtp_writebufptr;
unsigned esmtp_writebufleft;

static void connect_error(struct esmtp_info *info, void *arg)
{
	(*info->log_smtp_error)(info,
				errno == 0
				? "Connection closed by remote host"
				: errno == ECONNRESET
				? "Network connection shut down by the remote mail server"
				: strerror(errno), '4', arg);
}

void esmtp_init()
{
	mybuf_init(&esmtp_sockbuf, esmtp_sockfd);
	esmtp_writebufptr=esmtp_writebuf;
	esmtp_writebufleft=sizeof(esmtp_writebuf);
}

/* Set the timeout */

void esmtp_timeout(struct esmtp_info *info, unsigned nsecs)
{
	time(&esmtp_timeout_time);
	esmtp_timeout_time += nsecs;
}

/* Flush out anything that's waiting to be written out */

static void doflush(struct esmtp_info *info)
{
int	n;
int	i;

	if (esmtp_wait_write(info))
	{
		if (esmtp_sockfd >= 0)
			sox_close(esmtp_sockfd);
		esmtp_sockfd= -1;
		return;
	}
	if ((n=sox_write(esmtp_sockfd, esmtp_writebuf, esmtp_writebufptr-esmtp_writebuf)) <= 0)
	{
		if (esmtp_sockfd >= 0)
			sox_close(esmtp_sockfd);
		esmtp_sockfd= -1;
		return;
	}

	for (i=n; esmtp_writebuf+i < esmtp_writebufptr; i++)
		esmtp_writebuf[i-n]=esmtp_writebuf[i];
	esmtp_writebufptr -= n;
	esmtp_writebufleft += n;
}

int esmtp_writeflush(struct esmtp_info *info)
{
	while (esmtp_writebufptr > esmtp_writebuf && esmtp_sockfd >= 0)
		doflush(info);
	if (esmtp_sockfd < 0)	return (-1);
	return (0);
}


/* Write various stuff to the socket */

int esmtp_dowrite(struct esmtp_info *info, const char *p, unsigned l)
{
	while (l)
	{
	int n;

		if (esmtp_sockfd < 0)	return (-1);

		if (esmtp_writebufleft == 0)
		{
			doflush(info);
			continue;
		}
		if (esmtp_writebufleft < l)
			n=esmtp_writebufleft;
		else
			n=l;

		memcpy(esmtp_writebufptr, p, n);
		p += n;
		l -= n;
		esmtp_writebufptr += n;
		esmtp_writebufleft -= n;
	}
	return (0);
}

/* Wait for either a response, or availability for write, until we time out */

void esmtp_wait_rw(struct esmtp_info *info, int *waitr, int *waitw)
{
fd_set	fdr, fdw;
struct	timeval	tv;
time_t	current_time;

	time( & current_time );
	if (waitr)	*waitr=0;
	if (waitw)	*waitw=0;

	if (current_time >= esmtp_timeout_time || esmtp_sockfd < 0)
	{
		errno=ETIMEDOUT;
		if (esmtp_sockfd >= 0)
			sox_close(esmtp_sockfd);
		esmtp_sockfd= -1;
		return;
	}

	FD_ZERO(&fdr);
	FD_ZERO(&fdw);

	if (waitr)
		FD_SET(esmtp_sockbuf.fd, &fdr);

	if (waitw)
		FD_SET(esmtp_sockbuf.fd, &fdw);

	tv.tv_sec= esmtp_timeout_time - current_time;
	tv.tv_usec=0;

	if ( sox_select(esmtp_sockbuf.fd+1, &fdr, &fdw, 0, &tv) > 0)
	{
		if (waitw && FD_ISSET(esmtp_sockbuf.fd, &fdw))
			*waitw=1;
		if (waitr && FD_ISSET(esmtp_sockbuf.fd, &fdr))
			*waitr=1;
		return;
	}

	errno=ETIMEDOUT;
	sox_close(esmtp_sockfd);
	esmtp_sockfd= -1;
}

int esmtp_writestr(struct esmtp_info *info, const char *p)
{
	return (esmtp_dowrite(info, p, strlen(p)));
}


int esmtp_wait_read(struct esmtp_info *info)
{
	int	flag;

	esmtp_wait_rw(info, &flag, 0);
	return (flag ? 0:-1);
}

int esmtp_wait_write(struct esmtp_info *info)
{
	int	flag;

	esmtp_wait_rw(info, 0, &flag);
	return (flag ? 0:-1);
}


static char socklinebuf[sizeof(esmtp_sockbuf.buffer)+1];
static unsigned socklinesize=0;

static void swallow(struct esmtp_info *info, unsigned);
static void burp(struct esmtp_info *info, const char *, unsigned);

/* Receive a CRLF-terminated reply from the remote server */

const char *esmtp_readline(struct esmtp_info *info)
{
int	c;
char	cc;
char	*p;
unsigned cnt, i;

	socklinesize=0;
	if (esmtp_sockfd < 0)	return (0);
	for (;;)
	{
		p=mybuf_ptr( &esmtp_sockbuf );
		cnt=mybuf_ptrleft( &esmtp_sockbuf );
		if (cnt == 0)
		{
			if (esmtp_wait_read(info))	return (0);

			/* Check for unexpected shutdown */

			if ((c=mybuf_get( &esmtp_sockbuf )) < 0)
			{
				sox_close(esmtp_sockfd);
				esmtp_sockfd= -1;
				errno=ECONNRESET;
				return (0);
			}
			p = --mybuf_ptr( &esmtp_sockbuf );
			cnt = ++mybuf_ptrleft( &esmtp_sockbuf );
		}
		for (i=0; i<cnt; i++)
			if (p[i] == '\r')
				break;

		if (i < cnt)
		{
			swallow(info, i);
			(void)mybuf_get( &esmtp_sockbuf );	/* Skip the CR */

			for (;;)	/* Skip continuous CRs */
			{
				if (mybuf_ptrleft( &esmtp_sockbuf ) == 0 &&
				    esmtp_wait_read(info))	return (0);

				if ((c=mybuf_get( &esmtp_sockbuf )) != '\r')
					break;
				burp(info, "\r", 1);
			}

			if (c < 0)
			{
				sox_close(esmtp_sockfd);
				esmtp_sockfd= -1;
				return (0);
			}
			if (c == '\n')	break;	/* Seen CRLF */
			cc=c;
			burp(info, &cc, 1);
			continue;
		}
		swallow(info, i);
	}

	socklinebuf[socklinesize]=0;
	return (socklinebuf);
}

/* Copy stuff read from socket into the line buffer */

static void swallow(struct esmtp_info *info, unsigned l)
{
	burp(info, mybuf_ptr( &esmtp_sockbuf ), l);

	mybuf_ptr( &esmtp_sockbuf ) += l;
	mybuf_ptrleft( &esmtp_sockbuf ) -= l;
}

/* Replies are collected into a fixed length line buffer. */

static void burp(struct esmtp_info *info, const char *p, unsigned n)
{
	if (n > sizeof(socklinebuf)-1-socklinesize)
		n=sizeof(socklinebuf)-1-socklinesize;
	memcpy(socklinebuf+socklinesize, p, n);
	socklinesize += n;
}

struct esmtp_info *esmtp_info_alloc(const char *host)
{
	struct esmtp_info *p=malloc(sizeof(struct esmtp_info));

	if (!p)
		abort();

	memset(p, 0, sizeof(*p));
	p->host=strdup(host);

	if (!p->host)
		abort();

	p->smtproute=smtproutes(p->host, &p->smtproutes_flags);

	p->esmtpkeepaliveping=0;
	p->quit_timeout=10;
	p->connect_timeout=60;
	p->helo_timeout=300;
	p->data_timeout=300;
	p->cmd_timeout=600;
	p->delay_timeout=300;

	return p;
}

void esmtp_info_free(struct esmtp_info *p)
{
	if (p->authsasllist)
		free(p->authsasllist);
	if (p->smtproute)
		free(p->smtproute);
	if (p->sockfdaddrname)
		free(p->sockfdaddrname);
	free(p->host);
	free(p);
}

int esmtp_connected(struct esmtp_info *info)
{
	return esmtp_sockfd >= 0;
}

void esmtp_disconnect(struct esmtp_info *info)
{
	if (esmtp_sockfd < 0)
		return;
	sox_close(esmtp_sockfd);
	esmtp_sockfd= -1;
}

int esmtp_get_greeting(struct esmtp_info *info,
		       void *arg)
{
	const char *p;

	esmtp_init();
	esmtp_timeout(info, info->helo_timeout);

	if ((p=esmtp_readline(info)) == 0)	/* Wait for server first */
		return (1);

	if (*p == '5')	/* Hard error */
	{
		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "(initial greeting)", arg);

		while (!ISFINALLINE(p))	/* Skip multiline replies */
		{
			(*info->log_reply)(info, p, arg);
			if ((p=esmtp_readline(info)) == 0)
				return (1);
				/* Caller will report the error */
		}
		(*info->log_smtp_error)(info, p, '5',  arg);
		return (-1);
	}

	if (*p != '1' && *p != '2' && *p != '3')	/* Soft error */
	{
		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "(initial greeting)", arg);

		for (;;)
		{
			if (ISFINALLINE(p))
				break;

			(*info->log_reply)(info, p, arg);
			if ((p=esmtp_readline(info)) == 0)
			{
				return (1);
			}
		}
		(*info->log_smtp_error)(info, p, '4',  arg);
		info->quit_needed=1;
		return (-1);	/*
				** Let caller handle this as a hard error,
				** so that it does not try the next MX.
				*/
	}

	/* Skip multiline good response. */

	while (!ISFINALLINE(p))
	{
		if ((p=esmtp_readline(info)) == 0)
		{
			(*info->log_talking)(info, arg);
			return (1);
		}
	}
	return 0;
}

int esmtp_helo(struct esmtp_info *info, int using_tls,
	       void *arg)
{
	const	char *p;
	char	hellobuf[512];
	char buf[RFC1035_MAXNAMESIZE+128];

	info->haspipelining=0;
	info->hasdsn=0;
	info->has8bitmime=0;
	info->hasverp=0;
	info->hassize=0;
	info->hasexdata=0;
	info->hascourier=0;
	info->hasstarttls=0;
	info->hassecurity_starttls=0;
	info->is_secure_connection=0;


	if (info->authsasllist)
		free(info->authsasllist);
	info->authsasllist=0;

	p=config_esmtphelo();

	/*
	** If the remote host is "*", use reverse DNS from the local IP addr.
	*/

	if (strcmp(p, "*") == 0)
	{
		struct rfc1035_res res;
		int rc;

		rfc1035_init_resolv(&res);

		p=buf;
		rc=rfc1035_ptr(&res, &info->laddr, buf);

		rfc1035_destroy_resolv(&res);

		if (rc != 0)
		{
			char *q;

			rfc1035_ntoa(&info->laddr, buf+1);

			q=buf+1;

			if (strncmp(q, "::ffff:", 7) == 0)
				q += 7;
			*--q='[';
			strcat(q, "]");
			p=q;
		}
	}

	esmtp_timeout(info, info->helo_timeout);
	strcpy(hellobuf, "EHLO ");
	strncat(hellobuf, p, sizeof(hellobuf)-10);
	strcat(hellobuf, "\r\n");

	if (esmtp_writestr(info, hellobuf) || esmtp_writeflush(info))
	{
		(*info->log_talking)(info, arg);
		return (1);
	}

	if ((p=esmtp_readline(info)) == 0)
	{
		(*info->log_talking)(info, arg);
		return (1);
	}

	if (*p == '5')	/* Hard error, let's try a HELO */
	{
		while (!ISFINALLINE(p))	/* Skip multiline error */
		{
			if ((p=esmtp_readline(info)) == 0)
			{
				(*info->log_talking)(info, arg);
				return (1);
			}
		}
		hellobuf[0]='H';
		hellobuf[1]='E';

		esmtp_timeout(info, info->helo_timeout);
		if (esmtp_writestr(info, hellobuf) || esmtp_writeflush(info))
		{
			(*info->log_talking)(info, arg);
			return (1);
		}

		if ((p=esmtp_readline(info)) == 0)
		{
			(*info->log_talking)(info, arg);
			return (1);
		}
	}

	if (*p != '1' && *p != '2' && *p != '3') /* Some kind of an error */
	{
		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, hellobuf, arg);
		while (!ISFINALLINE(p))
		{
			(*info->log_reply)(info, p, arg);
			if ((p=esmtp_readline(info)) == 0)
				return (1);
		}
		(*info->log_smtp_error)(info, p, 0, arg);
		return (-1);	/*
				** Let the caller consider this a hard error,
				** so that it doesn't try the next MX.
				*/
	}

	/*
	** If we're here after a HELO, just eat it up, otherwise, we want to
	** parse available ESMTP keywords.
	*/

	if (hellobuf[0] == 'H')
	{
		while (!ISFINALLINE(p))
		{
			if ((p=esmtp_readline(info)) == 0)
			{
				(*info->log_talking)(info, arg);
				return (1);
			}
		}
		return (0);
	}

	if (!ISFINALLINE(p))
	{
/*
**	Read remaining lines, parse the keywords.
*/
		do
		{
		const char *q;
		unsigned l;

			if ((p=esmtp_readline(info)) == 0)
			{
				(*info->log_talking)(info, arg);
				return (1);
			}

			if (!isdigit((int)(unsigned char)p[0]) ||
				!isdigit((int)(unsigned char)p[1]) ||
				!isdigit((int)(unsigned char)p[2]) ||
				(p[3] != ' ' && p[3] != '-'))
			{
				continue;
			}
			q=p+4;
			for (l=0; q[l] && q[l] != ' '; l++)
			{
				if (l >= sizeof(hellobuf)-1)	break;
				hellobuf[l]=toupper(q[l]);
			}
			hellobuf[l]=0;

			if (strcmp(hellobuf, "PIPELINING") == 0)
				info->haspipelining=1;
			if (strcmp(hellobuf, "DSN") == 0)
				info->hasdsn=1;
			if (strcmp(hellobuf, "8BITMIME") == 0)
				info->has8bitmime=1;
			if (strcmp(hellobuf, "SIZE") == 0)
				info->hassize=1;
			if (strcmp(hellobuf, "STARTTLS") == 0)
				info->hasstarttls=1;

			if (strcmp(hellobuf, "AUTH") == 0
				|| strncmp(hellobuf, "AUTH=", 5) == 0)
			{
			const char *p=q+4;

				if (isspace((int)(unsigned char)*p)||*p == '=')
				{
				char	*s;
				unsigned	l=(info->authsasllist ?
					strlen(info->authsasllist)+1: 0)+strlen(p)+1;

					if (l > 10000)	continue;
							/* Script kiddies... */
					++p;
					s=courier_malloc(l);
					*s=0;
					if (info->authsasllist)
						strcat(strcpy(s, info->authsasllist),
							" ");
					strcat(s, p);
					if (info->authsasllist)
						free(info->authsasllist);
					info->authsasllist=s;
				}
			}


#define	KEYWORD(x)	(strcmp(hellobuf, x) == 0)
#define KEYWORDARG(x)	(strncmp(hellobuf, x, sizeof(x)-1) == 0)

			if (IS_EXDATA_KEYWORD)
				info->hasexdata=1;

			if (IS_VERP_KEYWORD)
			{
				char *p=strchr(hellobuf, '=');

				if (p)
				{
					for (++p; (p=strtok(p, ",")) != 0; p=0)
						if (strcasecmp(p, "Courier")
						    == 0)
							info->hasverp=1;
				}
			}

			if (IS_COURIER_EXTENSIONS)
				info->hascourier=1;

			if (IS_SECURITY_KEYWORD)
			{
				char *p=strchr(hellobuf, '=');

				if (p)
				{
					for (++p; (p=strtok(p, ",")) != 0; p=0)
						if (strcmp(p, "STARTTLS") == 0)
							info->hassecurity_starttls=1;
				}
			}
		} while (!ISFINALLINE(p));

		if (!info->hascourier) /* No courier extensions, no EXDATA or VERP */
			info->hasexdata=info->hasverp=info->hassecurity_starttls=0;
	}

	if (info->hasstarttls)
	{
		const char *q=getenv("COURIERTLS");
		struct	stat stat_buf;

		if (!q || stat(q, &stat_buf))
			info->hasstarttls=0;
	}

	if (info->hasstarttls)
	{
		const char *p=getenv("ESMTP_USE_STARTTLS");

		if (!p || !atoi(p))
			info->hasstarttls=0;
	}

	if ((info->smtproutes_flags & ROUTE_STARTTLS) && !using_tls)
	{

		if (!info->hasstarttls || !info->hassecurity_starttls)
		{
			(*info->log_talking)(info, arg);
			(*info->log_sent)(info, "SECURITY=STARTTLS REQUESTED FOR THIS MESSAGE", arg);
			(*info->log_smtp_error)(info,
						"500 Unable to set minimum security level.",
						0, arg);
			return (-1);
		}
	}

	if (getenv("COURIER_ESMTP_DEBUG_NO8BITMIME"))
		info->has8bitmime=0;
	return (0);
}

static void connection_closed(struct esmtp_info *info,
			      void *arg)
{
	(*info->log_smtp_error)
		(info,
		 "Connection unexpectedly closed by remote host.",
		 4, arg);
}

int esmtp_enable_tls(struct esmtp_info *info,
		     const char *hostname, int smtps,
		     void *arg)
{
	const char *p;
	int	pipefd[2];
	int rc;
	struct couriertls_info cinfo;
	char	*verify_domain=0;
	char	localfd_buf[NUMBUFSIZE+30];
	char	remotefd_buf[NUMBUFSIZE+30];
	char	miscbuf[NUMBUFSIZE];

	static char *trustcert_buf=0;
	static char *origcert_buf=0;

	char *argvec[10];

	int restore_origcert=0;
	int n;

	if (libmail_streampipe(pipefd))
	{
		perror("libmail_streampipe");
		return (-1);
	}

	if (!smtps)
	{
		if (esmtp_writestr(info, "STARTTLS\r\n") || esmtp_writeflush(info) ||
		    (p=esmtp_readline(info)) == 0)
		{
			(*info->log_talking)(info, arg);
			(*info->log_smtp_error)(info, "Remote mail server disconnected after receiving the STARTTLS command", '4', arg);
			close(pipefd[0]);
			close(pipefd[1]);
			return (1);
		}

		if (*p != '1' && *p != '2' && *p != '3')
		{
			(*info->log_talking)(info, arg);
			(*info->log_sent)(info, "STARTTLS", arg);
			close(pipefd[0]);
			close(pipefd[1]);

			while (!ISFINALLINE(p))
			{
				(*info->log_reply)(info, p, arg);
				if ((p=esmtp_readline(info)) == 0)
					break;
			}

			/* Consider this one a soft error, every time */
			(*info->log_smtp_error)(info, p, '4', arg);
			return (-1);
		}
	}

	couriertls_init(&cinfo);

	/*
	** Make sure that our side of the pipe is closed when couriertls
	** is execed by the child process.
	*/

	fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);

	strcat(strcpy(localfd_buf, "-localfd="),
	       libmail_str_size_t(pipefd[1], miscbuf));
	strcat(strcpy(remotefd_buf, "-remotefd="),
	       libmail_str_size_t(esmtp_sockfd, miscbuf));

	p=getenv("ESMTP_TLS_VERIFY_DOMAIN");

	if ((info->smtproutes_flags & ROUTE_STARTTLS) != 0)
	{
		char *q, *r;

		/*
		** Replace TLS_TRUSTCERTS with TLS_TRUSTSECURITYCERTS,
		** until couriertls is execed.
		*/

		q=getenv("TLS_TRUSTCERTS");

		r=courier_malloc(strlen(q ? q:"")+40);
		strcat(strcpy(r, "TLS_TRUSTCERTS="), q ? q:"");

		if (origcert_buf)
			free(origcert_buf);
		origcert_buf=r;
		restore_origcert=1;

		p=getenv("TLS_TRUSTSECURITYCERTS");
		if (!p || !*p)
		{
			static const char fail[]=
				"500 Unable to set minimum security"
				" level.\n";

			(*info->log_talking)(info, arg);
			(*info->log_sent)(info, "STARTTLS", arg);
			(*info->log_smtp_error)(info, fail, 0, arg);
			sox_close(esmtp_sockfd);
			esmtp_sockfd= -1;
			close(pipefd[0]);
			close(pipefd[1]);
			return (-1);
		}

		q=courier_malloc(strlen(p)+40);

		strcat(strcpy(q, "TLS_TRUSTCERTS="), p);
		putenv(q);
		p="1";

		if (trustcert_buf)
			free(trustcert_buf);
		trustcert_buf=q;
	}

	if (p && atoi(p))
	{
		verify_domain=courier_malloc(sizeof("-verify=")
					     +strlen(hostname));
		strcat(strcpy(verify_domain, "-verify="), hostname);
	}


	n=0;

	argvec[n++]=localfd_buf;
	argvec[n++]=remotefd_buf;
	if (verify_domain)
	{
		argvec[n++]=verify_domain;
	}
	argvec[n]=0;

	n=couriertls_start(argvec, &cinfo);

	if (restore_origcert)
		putenv(origcert_buf);
	if (verify_domain)
		free(verify_domain);

	close(esmtp_sockfd);
	esmtp_sockfd=pipefd[0];
	close(pipefd[1]);

	if (!n && fcntl(esmtp_sockfd, F_SETFL, O_NONBLOCK))
	{
		perror("fcntl");
		n= -1;
		strcpy(cinfo.errmsg, "fcntl() failed");
	}

	if (n)
	{
		char tmperrbuf[sizeof(cinfo.errmsg)+10];

		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "STARTTLS", arg);
		strcat(strcpy(tmperrbuf,
			      (info->smtproutes_flags & ROUTE_STARTTLS)
			      ? "500 ":"400 "),
		       cinfo.errmsg);
		(*info->log_smtp_error)(info, tmperrbuf, 0, arg);
		sox_close(esmtp_sockfd);
		esmtp_sockfd= -1;
		couriertls_destroy(&cinfo);
		return (-1);
	}
	couriertls_destroy(&cinfo);

	/* Reset the socket buffer structure given the new filedescriptor */

	esmtp_init();

	/* Ask again for an EHLO, because the capabilities may differ now */

	if (smtps)
		return 0;

	rc=esmtp_helo(info, 1, arg);

	if (rc > 0)
		connection_closed(info, arg);	/* Make sure to log it */
	else
		info->is_secure_connection=
			(info->smtproutes_flags & ROUTE_STARTTLS) ? 1:0;
	return (rc);

}


/****************************************************************************/
/* Authenticated ESMTP client                                               */
/****************************************************************************/

static const char *start_esmtpauth(const char *, const char *, void *);
static const char *esmtpauth(const char *, void *);
static int final_esmtpauth(const char *, void *);
static int plain_esmtpauth(const char *, const char *, void *);

struct esmtp_auth_xinfo {
	struct esmtp_info *info;
	void *arg;
};

int esmtp_auth(struct esmtp_info *info,
	       const char *auth_key,
	       void *arg)
{
	FILE	*configfile;
	char	uidpwbuf[256];
	char	*q;
	const char *p;
	struct authsaslclientinfo clientinfo;
	int	rc;

	struct esmtp_auth_xinfo xinfo;

	q=config_localfilename("esmtpauthclient");
	configfile=fopen( q, "r");
	free(q);

	if (!configfile)	return (0);

	info->auth_error_sent=0;

	for (;;)
	{
		if (fgets(uidpwbuf, sizeof(uidpwbuf), configfile) == 0)
		{
			fclose(configfile);
			return (0);
		}
		q=strtok(uidpwbuf, " \t\r\n");

		if (!auth_key || !q)	continue;

#if	HAVE_STRCASECMP
		if (strcasecmp(q, auth_key) == 0)
			break;
#else
		if (stricmp(q, auth_key) == 0)
			break;
#endif
	}
	fclose(configfile);

	xinfo.info=info;
	xinfo.arg=arg;

	memset(&clientinfo, 0, sizeof(clientinfo));
	clientinfo.userid=strtok(0, " \t\r\n");
	clientinfo.password=strtok(0, " \t\r\n");

	clientinfo.sasl_funcs=info->authsasllist;
	clientinfo.start_conv_func= &start_esmtpauth;
	clientinfo.conv_func= &esmtpauth;
	clientinfo.final_conv_func= &final_esmtpauth;
	clientinfo.plain_conv_func= &plain_esmtpauth;
	clientinfo.conv_func_arg= &xinfo;

	rc=auth_sasl_client(&clientinfo);
	if (rc == AUTHSASL_NOMETHODS)
	{
		(*info->log_talking)(info, arg);
		(*info->log_smtp_error)(info,
					"Compatible SASL authentication not available.", '5', arg);
		return (-1);
	}

	if (rc && !info->auth_error_sent)
	{
		(*info->log_talking)(info, arg);
		(*info->log_smtp_error)(info,
					"Temporary SASL authentication error.",
					'4', arg);
	}

	if (rc)
		return (-1);

	if ((p=esmtp_readline(info)) == 0)
	{
		(*info->log_talking)(info, arg);
		connect_error(info, arg);
		return (-1);
	}

	if (*p != '1' && *p != '2' && *p != '3') /* Some kind of an error */
	{
		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "AUTH", arg);
		while (!ISFINALLINE(p))
		{
			(*info->log_reply)(info, p, arg);
			if ((p=esmtp_readline(info)) == 0)
			{
				connection_closed(info, arg);
				return (-1);
			}
		}
		(*info->log_smtp_error)(info, p, 0, arg);
		info->quit_needed=1;
		return (-1);
	}

	return (0);
}

static const char *getresp(struct esmtp_auth_xinfo *x)
{
	struct esmtp_info *info=x->info;
	void *arg=x->arg;
	const char *p=esmtp_readline(info);

	if (p && *p == '3')
	{
		do
		{
			++p;
		} while ( isdigit((int)(unsigned char)*p));

		do
		{
			++p;
		} while ( isspace((int)(unsigned char)*p));
		return (p);
	}
	info->auth_error_sent=1;
	(*info->log_talking)(info, arg);
	(*info->log_sent)(info, "AUTH", arg);

	if (!p)
	{
		connection_closed(info, arg);
		return (0);
	}

	while (!ISFINALLINE(p))
	{
		(*info->log_reply)(info, p, arg);
		if ((p=esmtp_readline(info)) == 0)
		{
			connection_closed(info, arg);
			return (0);
		}
	}
	(*info->log_smtp_error)(info, p, 0, arg);
	return (0);
}

static const char *start_esmtpauth(const char *method, const char *arg,
	void *voidp)
{
	struct esmtp_auth_xinfo *xinfo=(struct esmtp_auth_xinfo *)voidp;
	struct esmtp_info *info=xinfo->info;

	if (arg && !*arg)
		arg="=";

	if (esmtp_writestr(info, "AUTH ") || esmtp_writestr(info, method) ||
			(arg && (esmtp_writestr(info, " ") ||
				 esmtp_writestr(info, arg))) ||
			esmtp_writestr(info, "\r\n") ||
		esmtp_writeflush(info))
		return (0);

	return (getresp((struct esmtp_auth_xinfo *)voidp));
}

static const char *esmtpauth(const char *msg, void *voidp)
{
	struct esmtp_auth_xinfo *xinfo=(struct esmtp_auth_xinfo *)voidp;
	struct esmtp_info *info=xinfo->info;

	if (esmtp_writestr(info, msg) || esmtp_writestr(info, "\r\n") || esmtp_writeflush(info))
		return (0);
	return (getresp((struct esmtp_auth_xinfo *)voidp));
}

static int final_esmtpauth(const char *msg, void *voidp)
{
	struct esmtp_auth_xinfo *xinfo=(struct esmtp_auth_xinfo *)voidp;
	struct esmtp_info *info=xinfo->info;

	if (esmtp_writestr(info, msg) || esmtp_writestr(info, "\r\n") ||
	    esmtp_writeflush(info))
		return (AUTHSASL_CANCELLED);
	return (0);
}

static int plain_esmtpauth(const char *method, const char *arg,
	void *voidp)
{
	struct esmtp_auth_xinfo *xinfo=(struct esmtp_auth_xinfo *)voidp;
	struct esmtp_info *info=xinfo->info;

	if (arg && !*arg)
		arg="=";

	if (esmtp_writestr(info, "AUTH ") || esmtp_writestr(info, method) ||
	    (arg && (esmtp_writestr(info, " ") ||
		     esmtp_writestr(info, arg))) ||
	    esmtp_writestr(info, "\r\n") ||
	    esmtp_writeflush(info))
		return (AUTHSASL_CANCELLED);

	return (0);
}

static int do_esmtp_connect(struct esmtp_info *info, void *arg);

int esmtp_connect(struct esmtp_info *info, void *arg)
{
	int rc;

	if (esmtp_connected(info))
		return 0;

	info->quit_needed=0;
	rc=do_esmtp_connect(info, arg);

	if (rc)
		esmtp_disconnect(info);

	return rc;
}

static int hello3(struct esmtp_info *info,
		  int using_tls,
		  void *arg)
{
	int rc;

	rc=esmtp_helo(info, using_tls, arg);

	if (rc == -1)
		esmtp_quit(info, arg);

	if ((*info->lookup_broken_starttls)(info,
					    info->sockfdaddrname, arg))
		info->hasstarttls=0;

	return rc;
}

static int hello(struct esmtp_info *info, void *arg)
{
	int rc=esmtp_get_greeting(info, arg);

	if (rc == 0)
		rc=hello3(info, 0, arg);

	if (info->quit_needed)
		esmtp_quit(info, arg);

	return rc;
}

static int get_sourceaddr(struct esmtp_info *info,
			  int af,
			  const RFC1035_ADDR *dest_addr,
			  RFC1035_NETADDR *addrbuf,
			  const struct sockaddr **addrptr, int *addrptrlen,
			  void *arg)
{
	RFC1035_ADDR in;

	int rc=(*info->get_sourceaddr)(info, dest_addr, &in, arg);

	if (rc)
		return rc;

	rc = rfc1035_mkaddress(af, addrbuf, &in, htons(0),
			       addrptr, addrptrlen);
	if (rc != 0)
		return rc;

	return 0;
}

static int local_sock_address(struct esmtp_info *info,
			      void *arg)
{
	RFC1035_NETADDR lsin;
	socklen_t i;

	i=sizeof(lsin);
	if (sox_getsockname(esmtp_sockfd, (struct sockaddr *)&lsin, &i) ||
	    rfc1035_sockaddrip(&lsin, i, &info->laddr))
	{
		(*info->log_smtp_error)(info,
					"Cannot obtain local socket IP address.",
					'4', arg);
		return -1;
	}
	return 0;
}

static int do_esmtp_connect(struct esmtp_info *info, void *arg)
{
	struct rfc1035_mxlist *mxlist, *p, *q;
	int static_route= info->smtproute != NULL;
	struct rfc1035_res res;
	int rc;
	const char *auth_key;
	int connection_attempt_made;

	errno=0;	/* Detect network failures */

	auth_key=info->smtproute ? info->smtproute:info->host;

	rfc1035_init_resolv(&res);

	rc=rfc1035_mxlist_create_x(&res,
				   auth_key,
				   RFC1035_MX_AFALLBACK |
				   RFC1035_MX_IGNORESOFTERR,
				   &mxlist);
	rfc1035_destroy_resolv(&res);

	switch (rc)	{
	case RFC1035_MX_OK:
		break;
	case RFC1035_MX_HARDERR:
		(*info->log_smtp_error)(info, "No such domain.", '5',
					arg);
		return -1;
	case RFC1035_MX_BADDNS:
		(*info->log_smtp_error)(info,
					"This domain's DNS violates RFC 1035.",
					'5', arg);
		return -1;
	default:
		(*info->log_smtp_error)(info, "DNS lookup failed.",
				       '4', arg);

		 if (errno)
		 {
			 info->net_error=errno;
			 time (&info->net_timeout);
			 info->net_timeout += info->delay_timeout;
		 }
		 return -1;
	}

	/* Check for broken MX records - BOFH */

	q=0;	/* Also see if I'm in the MX list */

	for (p=mxlist; p; p=p->next)
	{
		RFC1035_ADDR    addr;
		char    buf[RFC1035_NTOABUFSIZE];

		if (rfc1035_sockaddrip(&p->address,
				       sizeof(p->address), &addr))
			continue;

		rfc1035_ntoa(&addr, buf);
		if (strcmp(buf, p->hostname) == 0)
		{
			(*info->log_smtp_error)
				(info,
				 "This domain's DNS violates RFC 1035.",
				 '5', arg);
			rfc1035_mxlist_free(mxlist);
			return -1;
		}

		if (!q && !static_route &&
		    (*info->is_local_or_loopback)(info, p->hostname, buf, arg))
			q=p;
	}

	if (q && q->priority == mxlist->priority)
	{
		(*info->log_smtp_error)
			(info,
			 "configuration error: mail loops back to myself (MX problem).",
			 '5', arg);
		rfc1035_mxlist_free(mxlist);
		return -1;
	}

	/* Ok, try each MX server until we get through */

	connection_attempt_made=0;

	for (p=mxlist; p; p=p->next)
	{
		RFC1035_ADDR addr;
		int	port;
		int	af;
		RFC1035_NETADDR addrbuf, saddrbuf;
		const struct sockaddr *addrptr, *saddrptr;
		int	addrptrlen, saddrptrlen;

		if (q && q->priority == p->priority)
			break;
		/*
		** We're a backup MX for this domain, ignore MXs
		** with same, or higher, priority than us
		*/

		if (rfc1035_sockaddrip(&p->address,
				       sizeof(p->address), &addr) ||
		    rfc1035_sockaddrport(&p->address,
					 sizeof(p->address), &port))
			continue;

		connection_attempt_made=1;

		info->sockfdaddr=addr;
		if (info->sockfdaddrname)	free(info->sockfdaddrname);
		info->sockfdaddrname=strdup(p->hostname);	/* Save this for later */

		info->is_secure_connection=0;
		if ((esmtp_sockfd=rfc1035_mksocket(SOCK_STREAM, 0, &af))
		    >= 0 &&
		    rfc1035_mkaddress(af, &addrbuf, &addr, port,
				      &addrptr, &addrptrlen) == 0 &&
		    get_sourceaddr(info, af, &addr, &saddrbuf, &saddrptr,
				   &saddrptrlen, arg) == 0 &&
		    rfc1035_bindsource(esmtp_sockfd, saddrptr,
				       saddrptrlen) == 0 &&
		    s_connect(esmtp_sockfd, addrptr, addrptrlen,
			      info->connect_timeout) == 0)
		{
			/*
			** If we're connected, make sure EHLO or HELO
			** is cool, before blessing the connection.
			*/

			int	rc;

			if (local_sock_address(info, arg) < 0)
			{
				esmtp_disconnect(info);
				return -1;
			}

			if (info->smtproutes_flags & ROUTE_SMTPS)
			{
				if (esmtp_enable_tls(info,
						     p->hostname,
						     1,
						     arg))
				{
					esmtp_disconnect(info);
					continue; /* Next MX, please */
				}

				info->smtproutes_flags &= ~(ROUTE_TLS_REQUIRED);
			}

			rc=hello(info, arg);

			if (rc == 0)
			{
				/*
				** If the following fails, go to the
				** next MX.
				*/

				rc=1;

				if ((info->smtproutes_flags
				     & ROUTE_TLS_REQUIRED)
				    && !info->hasstarttls)
				{
					(*info->log_talking)(info, arg);
					(*info->log_smtp_error)
						(info,
						 "/SECURITY=REQUIRED set, but TLS is not available",
						 '5', arg);
					}
				else if ((info->smtproutes_flags &
					  ROUTE_STARTTLS)
					 && !info->hasstarttls)
				{
					(*info->log_talking)(info, arg);
					(*info->log_smtp_error)
						(info,
						 "/SECURITY set, but TLS is not available",
						 '5', arg);
					}
				else if (info->hasstarttls &&
					 esmtp_enable_tls
					 (info, p->hostname,
					  0, arg))
				{
					/*
					** Only keep track of broken
					** STARTTLS if we did not ask
					** for SECURITY.
					**
					** Otherwise, it's on us.
					*/
					if (!(info->smtproutes_flags &
					      ROUTE_STARTTLS))
					{
						(*info->report_broken_starttls)
							(info,
							 info->sockfdaddrname,
							 arg);
					}
				}
				else
				{
					int rc=esmtp_auth(info, auth_key, arg);

					if (rc == 0)
						break; /* Good HELO */
				}
			}
			esmtp_quit(info, arg);	/* We don't want to talk to him */
			if (rc < 0)
				return -1;	/* HELO failed, perm error */
			errno=0;
		}
		if (errno)
			info->net_error=errno;

		esmtp_disconnect(info);

#if 0
		if (p->next && p->priority == p->next->priority &&
		    strcmp(p->hostname, p->next->hostname) == 0)
		{
			continue; /* Another IP address for same MX */
		}

		/* Skip other MX records with the same priority */
		while (p->next && p->priority == p->next->priority)
			p=p->next;
#endif
	}

	rfc1035_mxlist_free(mxlist);
	if (!esmtp_connected(info))	/* Couldn't find an active server */
	{
		if (!connection_attempt_made)
			(*info->log_smtp_error)
				(info,
				 "Did not find a suitable MX for a connection",
				 '5', arg);
		else
		{
			if (info->net_error)
			{
				errno=info->net_error;
				(*info->log_smtp_error)
					(info, strerror(errno), 4, arg);
			}
			time (&info->net_timeout);
			info->net_timeout += info->delay_timeout;
		}
		return -1;
	}

	return 0;
}

/*
** Send a QUIT, and shut down the connection
*/

void esmtp_quit(struct esmtp_info *info, void *arg)
{
	const char *p;

	if (!esmtp_connected(info))	return;

	esmtp_timeout(info, info->quit_timeout);
	if (esmtp_writestr(info, "QUIT\r\n") || esmtp_writeflush(info))
	{
		esmtp_disconnect(info);
		return;
	}

	while ((p=esmtp_readline(info)) != 0 && !ISFINALLINE(p))
		;

	esmtp_disconnect(info);
}
