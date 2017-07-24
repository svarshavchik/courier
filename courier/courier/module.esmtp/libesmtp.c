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
#include	"rfc2045/rfc2045.h"
#include	"rw.h"
#include	<courierauthsaslclient.h>

static void esmtp_wait_rw(struct esmtp_info *info, int *waitr, int *waitw);
static int esmtp_wait_read(struct esmtp_info *info);
static int esmtp_wait_write(struct esmtp_info *info);
static void esmtp_disconnect(struct esmtp_info *info);
static void esmtp_init(struct esmtp_info *info);
static void esmtp_timeout(struct esmtp_info *info, unsigned nsecs);
static int esmtp_writeflush(struct esmtp_info *info);
static int esmtp_dowrite(struct esmtp_info *info, const char *, unsigned);
static int esmtp_writestr(struct esmtp_info *info, const char *);
static const char *esmtp_readline(struct esmtp_info *info);
static int esmtp_helo(struct esmtp_info *info, int using_tls,
		      void *arg);
static int esmtp_get_greeting(struct esmtp_info *info,
			      void *arg);
static int esmtp_enable_tls(struct esmtp_info *,
			    const char *,
			    int,
			    void *arg);
static int esmtp_auth(struct esmtp_info *info,
		      const char *auth_key,
		      void *arg);
static char *esmtp_mailfrom_cmd(struct esmtp_info *info,
				struct esmtp_mailfrom_info *mf_info);

static int esmtp_sendcommand(struct esmtp_info *info,
			     const char *cmd,
			     void *arg);
static int esmtp_parsereply(struct esmtp_info *info,
			    const char *cmd,
			    void *arg);

static void connect_error(struct esmtp_info *info, void *arg)
{
	(*info->log_smtp_error)(info,
				errno == 0
				? "Connection closed by remote host"
				: errno == ECONNRESET
				? "Network connection shut down by the remote mail server"
				: strerror(errno), '4', arg);
}

static void esmtp_init(struct esmtp_info *info)
{
	mybuf_init(&info->esmtp_sockbuf, info->esmtp_sockfd);
	info->esmtp_writebufptr=info->esmtp_writebuf;
	info->esmtp_writebufleft=sizeof(info->esmtp_writebuf);
}

/* Set the timeout */

static void esmtp_timeout(struct esmtp_info *info, unsigned nsecs)
{
	time(&info->esmtp_timeout_time);
	info->esmtp_timeout_time += nsecs;
}

/* Flush out anything that's waiting to be written out */

static void doflush(struct esmtp_info *info)
{
int	n;
int	i;

	if (esmtp_wait_write(info))
	{
		if (info->esmtp_sockfd >= 0)
			sox_close(info->esmtp_sockfd);
		info->esmtp_sockfd= -1;
		return;
	}
	if ((n=sox_write(info->esmtp_sockfd, info->esmtp_writebuf, info->esmtp_writebufptr-info->esmtp_writebuf)) <= 0)
	{
		if (info->esmtp_sockfd >= 0)
			sox_close(info->esmtp_sockfd);
		info->esmtp_sockfd= -1;
		return;
	}

	for (i=n; info->esmtp_writebuf+i < info->esmtp_writebufptr; i++)
		info->esmtp_writebuf[i-n]=info->esmtp_writebuf[i];
	info->esmtp_writebufptr -= n;
	info->esmtp_writebufleft += n;
}

static int esmtp_writeflush(struct esmtp_info *info)
{
	while (info->esmtp_writebufptr > info->esmtp_writebuf && info->esmtp_sockfd >= 0)
		doflush(info);
	if (info->esmtp_sockfd < 0)	return (-1);
	return (0);
}


/* Write various stuff to the socket */

static int esmtp_dowrite(struct esmtp_info *info, const char *p, unsigned l)
{
	while (l)
	{
	int n;

		if (info->esmtp_sockfd < 0)	return (-1);

		if (info->esmtp_writebufleft == 0)
		{
			doflush(info);
			continue;
		}
		if (info->esmtp_writebufleft < l)
			n=info->esmtp_writebufleft;
		else
			n=l;

		memcpy(info->esmtp_writebufptr, p, n);
		p += n;
		l -= n;
		info->esmtp_writebufptr += n;
		info->esmtp_writebufleft -= n;
	}
	return (0);
}

/* Wait for either a response, or availability for write, until we time out */

static void esmtp_wait_rw(struct esmtp_info *info, int *waitr, int *waitw)
{
fd_set	fdr, fdw;
struct	timeval	tv;
time_t	current_time;

	time( & current_time );
	if (waitr)	*waitr=0;
	if (waitw)	*waitw=0;

	if (current_time >= info->esmtp_timeout_time || info->esmtp_sockfd < 0)
	{
		errno=ETIMEDOUT;
		if (info->esmtp_sockfd >= 0)
			sox_close(info->esmtp_sockfd);
		info->esmtp_sockfd= -1;
		return;
	}

	FD_ZERO(&fdr);
	FD_ZERO(&fdw);

	if (waitr)
		FD_SET(info->esmtp_sockbuf.fd, &fdr);

	if (waitw)
		FD_SET(info->esmtp_sockbuf.fd, &fdw);

	tv.tv_sec= info->esmtp_timeout_time - current_time;
	tv.tv_usec=0;

	if ( sox_select(info->esmtp_sockbuf.fd+1, &fdr, &fdw, 0, &tv) > 0)
	{
		if (waitw && FD_ISSET(info->esmtp_sockbuf.fd, &fdw))
			*waitw=1;
		if (waitr && FD_ISSET(info->esmtp_sockbuf.fd, &fdr))
			*waitr=1;
		return;
	}

	errno=ETIMEDOUT;
	sox_close(info->esmtp_sockfd);
	info->esmtp_sockfd= -1;
}

static int esmtp_writestr(struct esmtp_info *info, const char *p)
{
	return (esmtp_dowrite(info, p, strlen(p)));
}


static int esmtp_wait_read(struct esmtp_info *info)
{
	int	flag;

	esmtp_wait_rw(info, &flag, 0);
	return (flag ? 0:-1);
}

static int esmtp_wait_write(struct esmtp_info *info)
{
	int	flag;

	esmtp_wait_rw(info, 0, &flag);
	return (flag ? 0:-1);
}


static void swallow(struct esmtp_info *info, unsigned);
static void burp(struct esmtp_info *info, const char *, unsigned);

/* Receive a CRLF-terminated reply from the remote server */

static const char *esmtp_readline(struct esmtp_info *info)
{
int	c;
char	cc;
char	*p;
unsigned cnt, i;

	info->socklinesize=0;
	if (info->esmtp_sockfd < 0)	return (0);
	for (;;)
	{
		p=mybuf_ptr( &info->esmtp_sockbuf );
		cnt=mybuf_ptrleft( &info->esmtp_sockbuf );
		if (cnt == 0)
		{
			if (esmtp_wait_read(info))	return (0);

			/* Check for unexpected shutdown */

			if ((c=mybuf_get( &info->esmtp_sockbuf )) < 0)
			{
				sox_close(info->esmtp_sockfd);
				info->esmtp_sockfd= -1;
				errno=ECONNRESET;
				return (0);
			}
			p = --mybuf_ptr( &info->esmtp_sockbuf );
			cnt = ++mybuf_ptrleft( &info->esmtp_sockbuf );
		}
		for (i=0; i<cnt; i++)
			if (p[i] == '\r')
				break;

		if (i < cnt)
		{
			swallow(info, i);
			(void)mybuf_get( &info->esmtp_sockbuf );	/* Skip the CR */

			for (;;)	/* Skip continuous CRs */
			{
				if (mybuf_ptrleft( &info->esmtp_sockbuf ) == 0 &&
				    esmtp_wait_read(info))	return (0);

				if ((c=mybuf_get( &info->esmtp_sockbuf )) != '\r')
					break;
				burp(info, "\r", 1);
			}

			if (c < 0)
			{
				sox_close(info->esmtp_sockfd);
				info->esmtp_sockfd= -1;
				return (0);
			}
			if (c == '\n')	break;	/* Seen CRLF */
			cc=c;
			burp(info, &cc, 1);
			continue;
		}
		swallow(info, i);
	}

	info->socklinebuf[info->socklinesize]=0;
	return (info->socklinebuf);
}

/* Copy stuff read from socket into the line buffer */

static void swallow(struct esmtp_info *info, unsigned l)
{
	burp(info, mybuf_ptr( &info->esmtp_sockbuf ), l);

	mybuf_ptr( &info->esmtp_sockbuf ) += l;
	mybuf_ptrleft( &info->esmtp_sockbuf ) -= l;
}

/* Replies are collected into a fixed length line buffer. */

static void burp(struct esmtp_info *info, const char *p, unsigned n)
{
	if (n > sizeof(info->socklinebuf)-1-info->socklinesize)
		n=sizeof(info->socklinebuf)-1-info->socklinesize;
	memcpy(info->socklinebuf+info->socklinesize, p, n);
	info->socklinesize += n;
}

static void default_rewrite_func(struct rw_info *info,
				 void (*func)(struct rw_info *))
{
	(*func)(info);
}

static void default_log_talking(struct esmtp_info *info, void *arg)
{
}

static void default_log_sent(struct esmtp_info *info, const char *msg,
			     int rcpt_num, void *arg)
{
}

static void default_log_reply(struct esmtp_info *info, const char *msg,
			      int rcpt_num, void *arg)
{
}

static void default_log_smtp_error(struct esmtp_info *info, const char *msg,
				   int rcpt_num, void *arg)
{
}

static void default_log_rcpt_error(struct esmtp_info *info,
				   int rcpt_num, int err_code, void *arg)
{
}

static void default_log_net_error(struct esmtp_info *info, int rcpt_num,
				  void *arg)
{
}

static void default_log_success(struct esmtp_info *info,
				unsigned rcpt_num,
				const char *msg, int dsn_flag,
				void *arg)
{
}

static void default_report_broken_starttls(struct esmtp_info *info,
					   const char *sockfdaddrname,
					   void *arg)
{
}

static int default_lookup_broken_starttls(struct esmtp_info *info,
					  const char *sockfdaddrname,
					  void *arg)
{
	return 0;
}

static int default_is_local_or_loopback(struct esmtp_info *info,
					const char *hostname,
					const char *address,
					void *arg)
{
	return 0;
}

static int default_get_sourceaddr(struct esmtp_info *info,
				  const RFC1035_ADDR *dest_addr,
				  RFC1035_ADDR *source_addr,
				  void *arg)
{
	return 0;
}

struct esmtp_info *esmtp_info_alloc(const char *host,
				    const char *smtproute,
				    int smtproutes_flags)
{
	struct esmtp_info *p=malloc(sizeof(struct esmtp_info));

	if (!p)
		abort();

	memset(p, 0, sizeof(*p));

	p->helohost="*";
	p->log_talking= &default_log_talking;
	p->log_sent= &default_log_sent;
	p->log_reply= &default_log_reply;
	p->log_smtp_error= &default_log_smtp_error;
	p->log_rcpt_error= &default_log_rcpt_error;
	p->log_net_error= &default_log_net_error;
	p->log_success= &default_log_success;
	p->report_broken_starttls= &default_report_broken_starttls;
	p->lookup_broken_starttls= &default_lookup_broken_starttls;
	p->is_local_or_loopback= &default_is_local_or_loopback;
	p->get_sourceaddr= &default_get_sourceaddr;

	p->host=strdup(host);

	if (!p->host)
		abort();

	p->esmtp_sockfd= -1;

	p->smtproutes_flags=smtproutes_flags;
	if (smtproute)
	{
		p->smtproute=strdup(smtproute);
		if (!p->smtproute)
			abort();
	}

	p->esmtpkeepaliveping=0;
	p->quit_timeout=10;
	p->connect_timeout=60;
	p->helo_timeout=300;
	p->data_timeout=300;
	p->cmd_timeout=600;
	p->delay_timeout=300;

	p->rewrite_func=default_rewrite_func;

#ifdef	TCP_CORK

	{
		const char *s=getenv("ESMTP_CORK");

		p->esmtp_cork=s ? atoi(s):0;
	}

#endif

	return p;
}

#ifdef	TCP_CORK

#define	cork(n)					\
	{					\
		int flag=(n);			\
									\
		if (info->esmtp_cork && esmtp_connected(info) &&	\
		    info->corked != flag)				\
		{							\
			setsockopt(info->esmtp_sockfd, SOL_TCP, TCP_CORK, \
				   &flag,				\
				   sizeof(flag));			\
		}							\
		corked=flag;						\
	}
#else
#define	cork(n)
#endif

void esmtp_info_free(struct esmtp_info *p)
{
	esmtp_disconnect(p);
	if (p->authclientfile)
		free(p->authclientfile);
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
	return info->esmtp_sockfd >= 0;
}

static void esmtp_disconnect(struct esmtp_info *info)
{
	if (info->esmtp_sockfd < 0)
		return;
	sox_close(info->esmtp_sockfd);
	info->esmtp_sockfd= -1;
}

static int esmtp_get_greeting(struct esmtp_info *info,
			      void *arg)
{
	const char *p;

	esmtp_init(info);
	esmtp_timeout(info, info->helo_timeout);

	if ((p=esmtp_readline(info)) == 0)	/* Wait for server first */
		return (1);

	if (*p == '5')	/* Hard error */
	{
		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "(initial greeting)", -1, arg);

		while (!ISFINALLINE(p))	/* Skip multiline replies */
		{
			(*info->log_reply)(info, p, -1, arg);
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
		(*info->log_sent)(info, "(initial greeting)", -1, arg);

		for (;;)
		{
			if (ISFINALLINE(p))
				break;

			(*info->log_reply)(info, p, -1, arg);
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

static int esmtp_helo(struct esmtp_info *info, int using_tls,
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

	p=info->helohost;

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
		(*info->log_sent)(info, hellobuf, -1, arg);
		while (!ISFINALLINE(p))
		{
			(*info->log_reply)(info, p, -1, arg);
			if ((p=esmtp_readline(info)) == 0)
				return (1);
		}
		(*info->log_smtp_error)(info, p, *p, arg);
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
					s=malloc(l);
					if (!s)
						abort();
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
			(*info->log_sent)(info, "SECURITY=STARTTLS REQUESTED FOR THIS MESSAGE", -1, arg);
			(*info->log_smtp_error)(info,
						"500 Unable to set minimum security level.",
						'5', arg);
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
		 '4', arg);
}

static int esmtp_enable_tls(struct esmtp_info *info,
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
			(*info->log_sent)(info, "STARTTLS", -1, arg);
			close(pipefd[0]);
			close(pipefd[1]);

			while (!ISFINALLINE(p))
			{
				(*info->log_reply)(info, p, -1, arg);
				if ((p=esmtp_readline(info)) == 0)
					break;
			}

			/* Consider this one a soft error, every time */
			if (!p)
				connection_closed(info, arg);
			else
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
	       libmail_str_size_t(info->esmtp_sockfd, miscbuf));

	p=getenv("ESMTP_TLS_VERIFY_DOMAIN");

	if ((info->smtproutes_flags & ROUTE_STARTTLS) != 0)
	{
		char *q, *r;

		/*
		** Replace TLS_TRUSTCERTS with TLS_TRUSTSECURITYCERTS,
		** until couriertls is execed.
		*/

		q=getenv("TLS_TRUSTCERTS");

		r=malloc(strlen(q ? q:"")+40);
		if (!r)
			abort();
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
			(*info->log_sent)(info, "STARTTLS", -1, arg);
			(*info->log_smtp_error)(info, fail, fail[0], arg);
			sox_close(info->esmtp_sockfd);
			info->esmtp_sockfd= -1;
			close(pipefd[0]);
			close(pipefd[1]);
			return (-1);
		}

		q=malloc(strlen(p)+40);
		if (!q)
			abort();
		strcat(strcpy(q, "TLS_TRUSTCERTS="), p);
		putenv(q);
		p="1";

		if (trustcert_buf)
			free(trustcert_buf);
		trustcert_buf=q;
	}

	if (p && atoi(p))
	{
		verify_domain=malloc(sizeof("-verify=")+strlen(hostname));
		if (!verify_domain)
			abort();
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

	close(info->esmtp_sockfd);
	info->esmtp_sockfd=pipefd[0];
	close(pipefd[1]);

	if (!n && fcntl(info->esmtp_sockfd, F_SETFL, O_NONBLOCK))
	{
		perror("fcntl");
		n= -1;
		strcpy(cinfo.errmsg, "fcntl() failed");
	}

	if (n)
	{
		char tmperrbuf[sizeof(cinfo.errmsg)+10];

		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "STARTTLS", -1, arg);
		strcat(strcpy(tmperrbuf,
			      (info->smtproutes_flags & ROUTE_STARTTLS)
			      ? "500 ":"400 "),
		       cinfo.errmsg);
		(*info->log_smtp_error)(info, tmperrbuf, tmperrbuf[0], arg);
		sox_close(info->esmtp_sockfd);
		info->esmtp_sockfd= -1;
		couriertls_destroy(&cinfo);
		return (-1);
	}
	couriertls_destroy(&cinfo);

	/* Reset the socket buffer structure given the new filedescriptor */

	esmtp_init(info);

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

void esmtp_setauthclientfile(struct esmtp_info *info, const char *filename)
{
	if (info->authclientfile)
		free(info->authclientfile);

	info->authclientfile=strdup(filename);

	if (!info->authclientfile)
		abort();
}

static int esmtp_auth(struct esmtp_info *info,
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

	if (!info->authclientfile)
		return 0;

	configfile=fopen(info->authclientfile, "r");

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
		(*info->log_sent)(info, "AUTH", -1, arg);
		while (!ISFINALLINE(p))
		{
			(*info->log_reply)(info, p, -1, arg);
			if ((p=esmtp_readline(info)) == 0)
			{
				connection_closed(info, arg);
				return (-1);
			}
		}
		(*info->log_smtp_error)(info, p, *p, arg);
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
	(*info->log_sent)(info, "AUTH", -1, arg);

	if (!p)
	{
		connection_closed(info, arg);
		return (0);
	}

	while (!ISFINALLINE(p))
	{
		(*info->log_reply)(info, p, -1, arg);
		if ((p=esmtp_readline(info)) == 0)
		{
			connection_closed(info, arg);
			return (0);
		}
	}
	(*info->log_smtp_error)(info, p, *p, arg);
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
	RFC1035_ADDR in=RFC1035_ADDRANY;

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
	if (sox_getsockname(info->esmtp_sockfd, (struct sockaddr *)&lsin, &i) ||
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
		if ((info->esmtp_sockfd=rfc1035_mksocket(SOCK_STREAM, 0, &af))
		    >= 0 &&
		    rfc1035_mkaddress(af, &addrbuf, &addr, port,
				      &addrptr, &addrptrlen) == 0 &&
		    get_sourceaddr(info, af, &addr, &saddrbuf, &saddrptr,
				   &saddrptrlen, arg) == 0 &&
		    rfc1035_bindsource(info->esmtp_sockfd, saddrptr,
				       saddrptrlen) == 0 &&
		    s_connect(info->esmtp_sockfd, addrptr, addrptrlen,
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
					(info, strerror(errno), '4', arg);
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

int esmtp_ping(struct esmtp_info *info)
{
	const char *p;

	esmtp_timeout(info, info->connect_timeout);

	if (esmtp_writestr(info, "RSET\r\n") || esmtp_writeflush(info))
	{
		esmtp_disconnect(info);
		return -1;
	}

	while ( (p=esmtp_readline(info)) != 0 && !ISFINALLINE(p))
				;

	if (p == 0)
	{
		esmtp_disconnect(info);
		return -1;
	}

	return 0;
}

int esmtp_misccommand(struct esmtp_info *info,
		      const char *cmd,
		      void *arg)
{
	char *with_crlf=malloc(strlen(cmd)+3);

	if (!with_crlf)
		abort();

	strcat(strcpy(with_crlf, cmd), "\r\n");

	esmtp_timeout(info, info->cmd_timeout);

	if (esmtp_sendcommand(info, with_crlf, arg) == 0 &&
	    esmtp_parsereply(info, cmd, arg) == 0)
	{
		free(with_crlf);
		return 0;
	}
	free(with_crlf);

	return -1;
}

static int esmtp_sendcommand(struct esmtp_info *info,
			     const char *cmd,
			     void *arg)
{
	if (esmtp_writestr(info, cmd) || esmtp_writeflush(info))
	{
		connect_error(info, arg);
		esmtp_disconnect(info);
		return (-1);
	}

	return 0;
}

static int esmtp_parsereply(struct esmtp_info *info,
			    const char *cmd,
			    void *arg)
{
	const char *p;

	if ((p=esmtp_readline(info)) == 0)
	{
		connect_error(info, arg);
		esmtp_disconnect(info);
		return (-1);
	}

	switch (*p) {
	case '1':
	case '2':
	case '3':
		break;
	default:
		(*info->log_sent)(info, cmd, -1, arg);
		while (!ISFINALLINE(p))
		{
			(*info->log_reply)(info, p, -1, arg);

			if ((p=esmtp_readline(info)) == 0)
			{
				connect_error(info, arg);
				esmtp_disconnect(info);
				return (-1);
			}
		}
		(*info->log_smtp_error)(info, p, *p, arg);
		return (-1);
	}

	if (info->log_good_reply)
		(*info->log_good_reply)(info, p, -1, arg);

	while (!ISFINALLINE(p))
	{
		if ((p=esmtp_readline(info)) == 0)
		{
			connect_error(info, arg);
			esmtp_disconnect(info);
			return (-1);
		}

		if (info->log_good_reply)
			(*info->log_good_reply)(info, p, -1, arg);

	}
	return (0);
}


/*
** Construct the MAIL FROM: command, taking into account ESMTP capabilities
** of the remote server.
*/

static char *esmtp_mailfrom_cmd(struct esmtp_info *info,
				struct esmtp_mailfrom_info *mf_info)
{
	char	*bodyverb="", *verpverb="", *retverb="";
	char	*oenvidverb="", *sizeverb="";
	const char *seclevel="";
	char	*mailfromcmd;
	size_t l;

	static const char seclevel_starttls[]=" SECURITY=STARTTLS";

	if (info->has8bitmime)	/* ESMTP 8BITMIME capability */
		bodyverb= mf_info->is8bitmsg ? " BODY=8BITMIME":" BODY=7BIT";

	if (info->hasverp && mf_info->verp)
		verpverb=" VERP";	/* ESMTP VERP capability */

	/* ESMTP DSN capability */
	if (info->hasdsn && mf_info->dsn_format)
		retverb=mf_info->dsn_format == 'F' ? " RET=FULL":
			mf_info->dsn_format == 'H' ? " RET=HDRS":"";

	if (info->hasdsn && mf_info->envid)
	{
		oenvidverb=malloc(sizeof(" ENVID=")+10+
				  strlen(mf_info->envid));
		if (!oenvidverb)
			abort();
		strcat(strcpy(oenvidverb, " ENVID="), mf_info->envid);
	}

	/* ESMTP SIZE capability */

	if (mf_info->msgsize > 0)
	{
		if (info->hassize)
		{
			unsigned long s=mf_info->msgsize;
			char	buf[NUMBUFSIZE+1];

			s= s/75 * 77+256;	/* Size estimate */
			if (!info->has8bitmime && mf_info->is8bitmsg)
				s=s/70 * 100;

			libmail_str_off_t(s, buf);
			sizeverb=malloc(sizeof(" SIZE=")+strlen(buf));
			if (!sizeverb)
				abort();
			strcat(strcpy(sizeverb, " SIZE="), buf);
		}
	}

	/* SECURITY extension */

	if (info->smtproutes_flags & ROUTE_STARTTLS)
		seclevel=seclevel_starttls;

	l=sizeof("MAIL FROM:<>\r\n")+
		strlen(mf_info->sender)+
		strlen(bodyverb)+
		strlen(verpverb)+
		strlen(retverb)+
		strlen(oenvidverb)+
		strlen(sizeverb)+
		strlen(seclevel);

	mailfromcmd=malloc(l);

	if (!mailfromcmd)
		abort();

	snprintf(mailfromcmd, l, "MAIL FROM:<%s>%s%s%s%s%s%s\r\n",
		 mf_info->sender,
		 bodyverb,
		 verpverb,
		 retverb,
		 oenvidverb,
		 sizeverb,
		 seclevel);

	if (*oenvidverb)	free(oenvidverb);
	if (*sizeverb)		free(sizeverb);
	return (mailfromcmd);
}

/*
** Build RCPT TOs
*/

static void mk_one_receip(struct esmtp_info *info,
			  struct esmtp_rcpt_info *receip,
			  void (*builder)(const char *, void *),
			  void *arg)
{
	const char *p;

	if (!receip)
	{
		(*builder)("DATA\r\n", arg);
		return;
	}

	(*builder)("RCPT TO:<", arg);
	(*builder)(receip->address, arg);
	(*builder)(">", arg);

	p=receip->dsn_options;

	if (p && info->hasdsn)
	{
		int s=0,f=0,d=0,n=0;

		while (*p)
			switch (*p++)	{
			case 'N':
				n=1;
				break;
			case 'D':
				d=1;
				break;
			case 'F':
				f=1;
				break;
			case 'S':
				s=1;
				break;
			}

		if (n)
			(*builder)(" NOTIFY=NEVER", arg);
		else
		{
			const char *p=" NOTIFY=";

			if (s)
			{
				(*builder)(p, arg);
				(*builder)("SUCCESS", arg);
				p=",";
			}
			if (f)
			{
				(*builder)(p, arg);
				(*builder)("FAILURE", arg);
				p=",";
			}
			if (d)
			{
				(*builder)(p, arg);
				(*builder)("DELAY", arg);
			}
		}
	}

	p=receip->orig_receipient;

	if (p && *p && info->hasdsn)
	{
		(*builder)(" ORCPT=", arg);
		(*builder)(p, arg);
	}

	(*builder)("\r\n", arg);
}

static void count(const char *str, void *arg)
{
	*(size_t *)arg += strlen(str);
}

static void build(const char *str, void *arg)
{
	char **s=(char **)arg;

	size_t l=strlen(str);

	memcpy((*s), str, l);

	*s += l;
}

static char *rcpt_data(struct esmtp_info *info,
		       struct esmtp_rcpt_info *receips,
		       size_t nreceips, int append_data)
{
	size_t i;
	size_t l=1;
	char *p;
	char *ptr;
	size_t n=nreceips;

	if (append_data)
		++n;

	for (i=0; i < n; ++i)
		mk_one_receip(info, (i<nreceips ? &receips[i]:NULL),
			      count, &l);

	p=malloc(l);
	if (!p)
		abort();
	ptr=p;

	for (i=0; i < n; ++i)
	{
		mk_one_receip(info, (i<nreceips ? &receips[i]:NULL),
			      build, &ptr);
	}
	*ptr=0;
	return p;
}

char *esmtp_rcpt_create(struct esmtp_info *info,
			struct esmtp_rcpt_info *receips,
			size_t n)
{
	return rcpt_data(info, receips, n, 0);
}

char *esmtp_rcpt_data_create(struct esmtp_info *info,
			     struct esmtp_rcpt_info *receips,
			     size_t n)
{
	return rcpt_data(info, receips, n, 1);
}



/* Record our peer in the message's control file, for error messages */

void esmtp_sockipname(struct esmtp_info *info, char *buf)
{
	rfc1035_ntoa( &info->sockfdaddr, buf);

#if	RFC1035_IPV6

	if (IN6_IS_ADDR_V4MAPPED(&info->sockfdaddr))
	{
	char	*p, *q;

		if ((p=strrchr(buf, ':')) != 0)
		{
			++p;
			q=buf;
			while ( (*q++ = *p++ ) )
				;
		}
	}
#endif
}


/***************************************************************************/
/*                             RCPT TO                                     */
/***************************************************************************/

/*
** do_pipeline_rcpt handles a pipeline RCPT TO command set.  That is, all
** the RCPT TOs are written at once, and we read the reply from the server
** in parallel.
**
** If the remote server does not support PIPELINING, a tiny hack in the
** write logic arranges for non-pipelined RCPT TO command set.
**
** ( DATA is also pipelined! )
*/

static const char *readpipelinercpt(struct esmtp_info *,
				    char **, size_t *);

static int parsedatareply(struct esmtp_info *info,
			  size_t,
			  int *, char **, size_t *, int, void *);

static int do_pipeline_rcpt_2(struct esmtp_info *info,
			      int *rcptok,
			      size_t nrecipients,
			      char *rcpt_data_cmd,
			      void *arg)
{
	const char *p;
	size_t l=strlen(rcpt_data_cmd);
	size_t i;
	int	rc=0;

	char	*orig_rcpt_data_cmd=rcpt_data_cmd;

	if (info->haspipelining)	/* One timeout */
		esmtp_timeout(info, info->cmd_timeout);

	/* Read replies for the RCPT TO commands */

	for (i=0; i<nrecipients; i++)
	{
		char	err_code=0;
		unsigned line_num=0;
		size_t write_cnt=l;
		char *next_rcpt_data_cmd=rcpt_data_cmd;
		size_t next_cnt=l;
		char *this_rcpt_to_cmd;
		size_t this_rcpt_to_len;

		for (this_rcpt_to_len=0; orig_rcpt_data_cmd[this_rcpt_to_len];
		     ++this_rcpt_to_len)
			if (orig_rcpt_data_cmd[this_rcpt_to_len] == '\n')
			{
				++this_rcpt_to_len;
				break;
			}

		// Should end with \r\n
		this_rcpt_to_cmd=orig_rcpt_data_cmd;
		orig_rcpt_data_cmd += this_rcpt_to_len;

		if (this_rcpt_to_len)
			--this_rcpt_to_len;

		/* If server can't do pipelining, just set niovw to one!!! */

		if (!info->haspipelining)
		{
			int i;

			for (i=0; i<l; ++i)
				if (rcpt_data_cmd[i] == '\n')
				{
					write_cnt=i+1;
					break;
				}

			next_cnt= l-write_cnt;
			next_rcpt_data_cmd= rcpt_data_cmd+write_cnt;

			esmtp_timeout(info, info->cmd_timeout);
		}

		do
		{
			if ((p=readpipelinercpt(info,
						&rcpt_data_cmd, &write_cnt))
			    == 0)
				break;

			if (line_num == 0)
				err_code= *p;

			switch (err_code) {
			case '1':
			case '2':
			case '3':
				continue;
			}

			if (line_num == 0)
			{
				char save=this_rcpt_to_cmd[this_rcpt_to_len];
				// Should be the \r

				this_rcpt_to_cmd[this_rcpt_to_len]=0;

				(*info->log_sent)(info, this_rcpt_to_cmd,
						  i, arg);
				this_rcpt_to_cmd[this_rcpt_to_len]=save;
				++line_num;
			}
			(*info->log_reply)(info, p, i, arg);
		} while (!ISFINALLINE(p));

		if (!p)
		{
			while (i < nrecipients)
				rcptok[i++]=1;
			break;
		}

		if (info->haspipelining)
		{
			l=write_cnt;
		}
		else
		{
			rcpt_data_cmd=next_rcpt_data_cmd;
			l=next_cnt;
		}

		switch (err_code) {
		case '1':
		case '2':
		case '3':
			rcptok[i]=1;	/* This recipient was accepted */
			continue;
		}

		/* Failed.  Report it */

		rcptok[i]=0;

		(*info->log_rcpt_error)(info, i, err_code, arg);
	}

/* ------------------- Read the reply to the DATA ----------------- */

	if (esmtp_connected(info))
	{
		if (!info->haspipelining)	/* DATA hasn't been sent yet */
		{
			for (i=0; i<nrecipients; i++)
				if (rcptok[i])	break;

			if (i >= nrecipients)	return (-1);
					/* All RCPT TOs failed */
		}
		rc=parsedatareply(info, nrecipients,
				  rcptok, &rcpt_data_cmd, &l, 0, arg);
			/* One more reply */
	}

	if (!esmtp_connected(info))
	{
		for (i=0; i<nrecipients; i++)
		{
			if (!rcptok[i])	continue;
			(*info->log_net_error)(info, i, arg);
		}
		rc= -1;
	}

	return (rc);
}


/* Read an SMTP reply line in pipeline mode */

static const char *readpipelinercpt(struct esmtp_info *info,
				    char **write_buf,
				    size_t *write_l)
{
	int	read_flag, write_flag, *writeptr;

	if (!esmtp_connected(info))	return (0);

	if (mybuf_more(&info->esmtp_sockbuf))
		return (esmtp_readline(info));	/* We have the reply buffered */

	do
	{
		write_flag=0;
		writeptr= &write_flag;
		if (!write_buf || !*write_buf || !*write_l)
			writeptr=0;

		esmtp_wait_rw(info, &read_flag, writeptr);

		if (write_flag)	/* We can squeeze something out now */
		{
			int	n=sox_write(info->esmtp_sockfd, *write_buf,
					    *write_l);

			if (n < 0)
			{
				esmtp_disconnect(info);
				return (0);
			}

			*write_buf += n;
			*write_l -= n;
		}
	} while (!read_flag && esmtp_connected(info));

	return (esmtp_readline(info));
}

/***************************************************************************/
/*                               DATA                                      */
/***************************************************************************/

/*
** Parse the reply to the DATA command.
** This is called to parse both the first reply (isfinal=0), and the
** second reply after the message has been sent (isfinal=1).
**
** When isfinal=0, if this is called as part of pipelined commands,
** iovw/niovw must be initialized appropriately, otherwise they must be null.
*/

static int parseexdatareply(struct esmtp_info *,
			    const char *,
			    int *,
			    size_t,
			    void *);

static char *logsuccessto(struct esmtp_info *info)
{
	char	buf[RFC1035_NTOABUFSIZE];
	char	*p;

	esmtp_sockipname(info, buf);
	p=malloc(sizeof("delivered:  []")+
		 (info->sockfdaddrname ?
		  strlen(info->sockfdaddrname):0)+strlen(buf));

	if (!p)
		abort();

	strcpy(p, "delivered: ");
	if (info->sockfdaddrname && *info->sockfdaddrname)
		strcat(strcat(p, info->sockfdaddrname), " ");
	strcat(p, "[");
	strcat(p, buf);
	strcat(p, "]");
	return (p);
}

static int parsedatareply(struct esmtp_info *info,
			  size_t nreceipients,
			  int *rcptok,
			  char **write_buf,
			  size_t *write_l,
			  int isfinal,
			  void *arg)
{
	const char *p;
	unsigned i;
	int errcode;

	p=readpipelinercpt(info, write_buf, write_l);

	if (!p)	return (-1);

	errcode=*p;

	switch (errcode) {
	case '1':
	case '2':
	case '3':
		/*
		** DATA went through.  What we do depends on whether this is
		** the first or the last DATA.
		*/

		if (isfinal)
		{
			for (i=0; i<nreceipients; i++)
			{
				if (!rcptok[i])	continue;

				(*info->log_sent)(info, "DATA", i, arg);
			}
		}

		for (;;)
		{
			if (isfinal)
			{
				/* We want to record the final DATA reply in
				** the log.
				*/

				for (i=0; i<nreceipients; i++)
				{
					if (!rcptok[i])	continue;

					(*info->log_reply)(info, p, i, arg);
				}
			}
			if (ISFINALLINE(p))	break;
			if ((p=esmtp_readline(info)) == 0)
				return (-1);
		}

		if (isfinal)
		{
			char	*p=logsuccessto(info);

			/*
			** Final reply - record a success for recipients that
			** haven't previously failed (in RCPT TO).
			*/

			for (i=0; i<nreceipients; i++)
			{
				if (!rcptok[i])	continue;

				(*info->log_success)(info, i, p,
						     info->hasdsn, arg);
			}
			free(p);
		}
		else
		{
			/* Good response to the first DATA */

			for (i=0; i<nreceipients; i++)
				if (rcptok[i]) break;

			if (i >= nreceipients)
				/* Stupid server wants message with no
				** receipients
				*/
			{
				esmtp_timeout(info, info->data_timeout);
				if (esmtp_writestr(info, ".\r\n") ||
				    esmtp_writeflush(info))
					return (-1);
				do
				{
					p=esmtp_readline(info);
					if (!p)	return (-1);
				} while (!ISFINALLINE(p));
				return (-1);
			}
		}
		return (0);
	}

	/* DATA error */

	if (info->hasexdata && isfinal && *p == '5' && p[1] == '5'  && p[2] == '8')
		return (parseexdatareply(info, p, rcptok,
					 nreceipients, arg));
		/* Special logic for EXDATA extended replies */

	/* Fail the recipients that haven't been failed already */

	for (i=0; i<nreceipients; i++)
	{
		if (!rcptok[i])	continue;
		(*info->log_sent)(info, "DATA", i, arg);
	}

	for (;;)
	{
		for (i=0; i<nreceipients; i++)
		{
			if (!rcptok[i])	continue;

			(*info->log_reply)(info, p, i, arg);
		}

		if (ISFINALLINE(p))
		{
			for (i=0; i<nreceipients; i++)
			{
				if (!rcptok[i])	continue;

				(*info->log_rcpt_error)(info, i, *p, arg);
				rcptok[i]=0;
			}
			break;
		}
		if ((p=esmtp_readline(info)) == 0)
			return (-1);
	}
	return (-1);
}

/*
** Parse EXDATA 558 reply.
**
** See draft-varshavchik-exdata-smtpext.txt for more information.
*/

static int parseexdatareply(struct esmtp_info *info,
			    const char *p,
			    int *rcptok,
			    size_t nreceipients,
			    void *arg)

{
	unsigned i;
	char err_code=0;
	unsigned line_num=0;

	for (i=0; i<nreceipients; i++)
		if (rcptok[i])	break;

	/* i is the next recipient that's getting an extended reply */

	for (;;p=esmtp_readline(info))
	{
		if (!p)	return (-1);

		if (!isdigit((int)(unsigned char)*p) ||
			!isdigit((int)(unsigned char)p[1]) ||
			!isdigit((int)(unsigned char)p[2]) || !p[3])
			continue;

		if (line_num == 0)
		{
			err_code=p[4];
		}

		if (i >= nreceipients)	/* Bad extended reply */
		{
			if (ISFINALLINE(p))	break;
			continue;
		}

		if (line_num == 0)
		{
			switch (err_code) {
			case '1':
			case '2':
			case '3':
				break;
			default:
				(*info->log_sent)(info, "DATA", i, arg);
			}
		}

		(*info->log_reply)(info, p+4, i, arg);

		if (line_num == 0)
			++line_num;

		if (ISFINALLINE((p+4)))
		{
			switch (err_code) {
			case '1':
			case '2':
			case '3':
				{
					char	*p=logsuccessto(info);

					(*info->log_success)(info, i,
							     p,
							     info->hasdsn,
							     arg);

					free(p);
				}
				break;
			default:
				(*info->log_rcpt_error)(info, i, err_code, arg);
				rcptok[i]=0;
				break;
			}

			/* Find next recipient that gets an extended reply */

			while (i < nreceipients &&
				!rcptok[++i])
				;
			line_num=0;
		}
		if (ISFINALLINE(p))	break;
	}

	while (i < nreceipients)
		if (rcptok[++i])
		{
			(*info->log_reply)(info,
					   "Invalid 558 response from server",
					   i, arg);

			(*info->log_rcpt_error)(info, i, '5', arg);
			rcptok[i]=0;
		}

	return (0);
}

/* Write out .\r\n, then wait for the DATA reply */

static int data_wait(struct esmtp_info *info, size_t nreceipients,
		     int *rcptok, void *arg)
{
	esmtp_timeout(info, info->data_timeout);

	if (esmtp_dowrite(info, ".\r\n", 3) ||
	    esmtp_writeflush(info))	return (-1);

	cork(0);

	(void)parsedatareply(info, nreceipients, rcptok, 0, 0, 1, arg);

	if (!esmtp_connected(info))	return (-1);

	return (0);
}

static void parserfc(int fd, struct rfc2045 *rfcp)
{
char	buf[8192];
int	n;

	while ((n=sox_read(fd, buf, sizeof(buf))) > 0)
		rfc2045_parse(rfcp, buf, n);
}

/*
** Ok, everything above is collected into a nice, tight, package.
*/

struct rw_for_esmtp {

	/* State flags for converting msg to ESMTP format */

	int	is_sol;

	unsigned byte_counter;

	struct esmtp_info *info;
	void *arg;
	} ;

static void call_rewrite_func(struct rw_info *p, void (*f)(struct rw_info *),
			      void *arg)
{
	struct rw_for_esmtp *rfe=(struct rw_for_esmtp *)arg;

	(*rfe->info->rewrite_func)(p, f);
}

static int convert_to_crlf(const char *, unsigned, void *);

int esmtp_send(struct esmtp_info *info,
	       struct esmtp_mailfrom_info *mf_info,
	       struct esmtp_rcpt_info *rcpt_info,
	       size_t nreceipients,
	       int fd,
	       void *arg)
{
	unsigned i;
	int	*rcptok;
	char	*mailfroms;
	struct rfc2045 *rfcp=0;

	struct	stat stat_buf;
	char *rcpt_buf;

	if (info->esmtp_sockfd < 0)
		return -1;

	if (fstat(fd, &stat_buf) == 0)
		mf_info->msgsize=stat_buf.st_size;

	mailfroms=esmtp_mailfrom_cmd(info, mf_info);

	(*info->log_talking)(info, arg);
	esmtp_timeout(info, info->cmd_timeout);

	if (esmtp_sendcommand(info, mailfroms, arg))
	{
		free(mailfroms);
		return 0;
	}

	/*
	** While waiting for MAIL FROM to come back, check if the message
	** needs to be converted to quoted-printable.
	*/

	if (!info->has8bitmime && mf_info->is8bitmsg)
	{
		rfcp=rfc2045_alloc_ac();
		if (!rfcp)	clog_msg_errno();
		parserfc(fd, rfcp);

		rfc2045_ac_check(rfcp, RFC2045_RW_7BIT);
	}

	if (esmtp_parsereply(info, mailfroms, arg))	/* MAIL FROM rejected */
	{
		if (rfcp)	rfc2045_free(rfcp);

		free(mailfroms);
		return 0;
	}
	free(mailfroms);

	rcptok=malloc(sizeof(int)*(nreceipients+1));
	if (!rcptok)
		abort();

	rcpt_buf=esmtp_rcpt_data_create(info, rcpt_info, nreceipients);

	if ( do_pipeline_rcpt_2(info, rcptok, nreceipients, rcpt_buf,
				arg) )
	{
		free(rcpt_buf);
		if (rfcp)	rfc2045_free(rfcp);
		free(rcptok);
		return 0;
	}
	free(rcpt_buf);

	{
	struct rw_for_esmtp rfe;

		rfe.is_sol=1;
		rfe.byte_counter=0;
		rfe.info=info;
		rfe.arg=arg;

		cork(1);

		if ((rfcp ?
			rw_rewrite_msg_7bit(fd, rfcp,
				&convert_to_crlf,
				&call_rewrite_func,
				&rfe):
			rw_rewrite_msg(fd,
				&convert_to_crlf,
				&call_rewrite_func,
				&rfe))
		    || data_wait(info, nreceipients, rcptok,  arg))
			for (i=0; i<nreceipients; i++)
				if (rcptok[i])
					(*info->log_net_error)(info, i,
							       arg);
		free(rcptok);
		cork(0);
	}
	if (rfcp)	rfc2045_free(rfcp);
	return 0;
}

static int escape_dots(const char *, unsigned,
		       struct rw_for_esmtp *);

static int convert_to_crlf(const char *msg, unsigned l, void *voidp)
{
unsigned i, j;
int	rc;

	for (i=j=0; i < l; i++)
	{
		if (msg[i] != '\n')
			continue;
		if ((rc=escape_dots(msg+j, i-j,
			(struct rw_for_esmtp *)voidp)) != 0 ||
			(rc=escape_dots("\r", 1,
				(struct rw_for_esmtp *)voidp)) != 0)
			return (rc);
		j=i;
	}
	return (escape_dots(msg+j, i-j, (struct rw_for_esmtp *)voidp));
}

static int escape_dots(const char *msg, unsigned l, struct rw_for_esmtp *ptr)
{
	unsigned i, j;
	int	rc;
	struct esmtp_info *info=ptr->info;

	if ( (ptr->byte_counter += l) >= BUFSIZ)
		ptr->byte_counter=0;

	if (ptr->byte_counter == 0)
		esmtp_timeout(info, info->data_timeout);

	for (i=j=0; i<l; i++)
	{
		if (ptr->is_sol && msg[i] == '.')
		{
			if ((rc=esmtp_dowrite(info, msg+j, i-j)) != 0 ||
			    (rc=esmtp_dowrite(info, ".", 1)) != 0)
				return (rc);
			j=i;
		}
		ptr->is_sol= msg[i] == '\n' ? 1:0;
	}

	return (esmtp_dowrite(info, msg+j, i-j));
}
