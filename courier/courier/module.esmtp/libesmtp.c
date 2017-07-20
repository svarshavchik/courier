/*
** Copyright 2017 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"libesmtp.h"
#include	<errno.h>
#include	<sys/types.h>
#include	<sys/uio.h>
#include	<sys/socket.h>
#include	<string.h>
#include	<stdlib.h>
#include	"courier.h"
#include	"comreadtime.h"
#include	"esmtpconfig.h"

int esmtp_sockfd;
time_t	esmtp_timeout_time;
struct mybuf esmtp_sockbuf;
char esmtp_writebuf[BUFSIZ];
char *esmtp_writebufptr;
unsigned esmtp_writebufleft;

void esmtp_init()
{
	mybuf_init(&esmtp_sockbuf, esmtp_sockfd);
	esmtp_writebufptr=esmtp_writebuf;
	esmtp_writebufleft=sizeof(esmtp_writebuf);
}

/* Set the timeout */

void esmtp_timeout(unsigned nsecs)
{
	time(&esmtp_timeout_time);
	esmtp_timeout_time += nsecs;
}

/* Flush out anything that's waiting to be written out */

static void doflush()
{
int	n;
int	i;

	if (esmtp_wait_write())
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

int esmtp_writeflush()
{
	while (esmtp_writebufptr > esmtp_writebuf && esmtp_sockfd >= 0)
		doflush();
	if (esmtp_sockfd < 0)	return (-1);
	return (0);
}


/* Write various stuff to the socket */

int esmtp_dowrite(const char *p, unsigned l)
{
	while (l)
	{
	int n;

		if (esmtp_sockfd < 0)	return (-1);

		if (esmtp_writebufleft == 0)
		{
			doflush();
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

void esmtp_wait_rw(int *waitr, int *waitw)
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

int esmtp_writestr(const char *p)
{
	return (esmtp_dowrite(p, strlen(p)));
}


int esmtp_wait_read()
{
int	flag;

	esmtp_wait_rw(&flag, 0);
	return (flag ? 0:-1);
}

int esmtp_wait_write()
{
int	flag;

	esmtp_wait_rw(0, &flag);
	return (flag ? 0:-1);
}


static char socklinebuf[sizeof(esmtp_sockbuf.buffer)+1];
static unsigned socklinesize=0;

static void swallow(unsigned);
static void burp(const char *, unsigned);

/* Receive a CRLF-terminated reply from the remote server */

const char *esmtp_readline()
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
			if (esmtp_wait_read())	return (0);

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
			swallow(i);
			(void)mybuf_get( &esmtp_sockbuf );	/* Skip the CR */

			for (;;)	/* Skip continuous CRs */
			{
				if (mybuf_ptrleft( &esmtp_sockbuf ) == 0 &&
					esmtp_wait_read())	return (0);

				if ((c=mybuf_get( &esmtp_sockbuf )) != '\r')
					break;
				burp("\r", 1);
			}

			if (c < 0)
			{
				sox_close(esmtp_sockfd);
				esmtp_sockfd= -1;
				return (0);
			}
			if (c == '\n')	break;	/* Seen CRLF */
			cc=c;
			burp(&cc, 1);
			continue;
		}
		swallow(i);
	}

	socklinebuf[socklinesize]=0;
	return (socklinebuf);
}

/* Copy stuff read from socket into the line buffer */

static void swallow(unsigned l)
{
	burp(mybuf_ptr( &esmtp_sockbuf ), l);

	mybuf_ptr( &esmtp_sockbuf ) += l;
	mybuf_ptrleft( &esmtp_sockbuf ) -= l;
}

/* Replies are collected into a fixed length line buffer. */

static void burp(const char *p, unsigned n)
{
	if (n > sizeof(socklinebuf)-1-socklinesize)
		n=sizeof(socklinebuf)-1-socklinesize;
	memcpy(socklinebuf+socklinesize, p, n);
	socklinesize += n;
}

struct esmtp_info *esmtp_info_alloc()
{
	struct esmtp_info *p=malloc(sizeof(struct esmtp_info));

	if (!p)
		abort();

	memset(p, 0, sizeof(*p));
	return p;
}

void esmtp_info_free(struct esmtp_info *p)
{
	free(p);
}

int esmtp_get_greeting(struct esmtp_info *info,
		       void *arg)
{
	const char *p;

	esmtp_init();
	esmtp_timeout(config_time_esmtphelo());

	if ((p=esmtp_readline()) == 0)	/* Wait for server first */
		return (1);

	if (*p == '5')	/* Hard error */
	{
		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "(initial greeting)", arg);

		while (!ISFINALLINE(p))	/* Skip multiline replies */
		{
			(*info->log_reply)(info, p, arg);
			if ((p=esmtp_readline()) == 0)
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
			if ((p=esmtp_readline()) == 0)
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
		if ((p=esmtp_readline()) == 0)
		{
			(*info->log_talking)(info, arg);
			return (1);
		}
	}
	return 0;
}

int esmtp_helo(struct esmtp_info *info, int using_tls,
	       const char *security_level, void *arg)
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

	char *authsasllist=0;

	if (authsasllist)	free(authsasllist);
	authsasllist=0;

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

	strcpy(hellobuf, "EHLO ");
	strncat(hellobuf, p, sizeof(hellobuf)-10);
	strcat(hellobuf, "\r\n");

	if (esmtp_writestr(hellobuf) || esmtp_writeflush())
	{
		(*info->log_talking)(info, arg);
		return (1);
	}

	if ((p=esmtp_readline()) == 0)
	{
		(*info->log_talking)(info, arg);
		return (1);
	}

	if (*p == '5')	/* Hard error, let's try a HELO */
	{
		while (!ISFINALLINE(p))	/* Skip multiline error */
		{
			if ((p=esmtp_readline()) == 0)
			{
				(*info->log_talking)(info, arg);
				return (1);
			}
		}
		hellobuf[0]='H';
		hellobuf[1]='E';

		esmtp_timeout(config_time_esmtphelo());
		if (esmtp_writestr(hellobuf) || esmtp_writeflush())
		{
			(*info->log_talking)(info, arg);
			return (1);
		}

		if ((p=esmtp_readline()) == 0)
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
			if ((p=esmtp_readline()) == 0)
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
			if ((p=esmtp_readline()) == 0)
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

			if ((p=esmtp_readline()) == 0)
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
				unsigned	l=(authsasllist ?
					strlen(authsasllist)+1: 0)+strlen(p)+1;

					if (l > 10000)	continue;
							/* Script kiddies... */
					++p;
					s=courier_malloc(l);
					*s=0;
					if (authsasllist)
						strcat(strcpy(s, authsasllist),
							" ");
					strcat(s, p);
					if (authsasllist)
						free(authsasllist);
					authsasllist=s;
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

	if (security_level != 0)
	{
		if ( strcmp(security_level, "STARTTLS") == 0)
		{
			if ((info->hasstarttls || using_tls) &&
			    info->hassecurity_starttls)
				return (0);
		}

		(*info->log_talking)(info, arg);
		(*info->log_sent)(info, "SECURITY=STARTTLS REQUESTED FOR THIS MESSAGE", arg);
		(*info->log_smtp_error)(info,
					"500 Unable to set minimum security level.",
					0, arg);
		return (-1);
	}

	if (info->hasstarttls)
	{
		const char *p=getenv("ESMTP_USE_STARTTLS");

		if (!p || !atoi(p))
			info->hasstarttls=0;
	}

	if (getenv("COURIER_ESMTP_DEBUG_NO8BITMIME"))
		info->has8bitmime=0;
	return (0);
}
