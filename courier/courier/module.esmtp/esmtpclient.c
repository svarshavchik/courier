/*
** Copyright 1998 - 2013 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"smtproutes.h"
#include	"localstatedir.h"
#include	"moduledel.h"
#include	"comctlfile.h"
#include	"comreadtime.h"
#include	"comqueuename.h"
#include	"maxlongsize.h"
#include	"comverp.h"
#include	"comtrack.h"
#include	"libesmtp.h"
#include	"rfc1035/rfc1035.h"
#include	"rfc1035/rfc1035mxlist.h"
#include	"rfc822/rfc822.h"
#include	"rfc2045/rfc2045.h"
#include	"numlib/numlib.h"
#include	"tcpd/spipe.h"
#include	"tcpd/tlsclient.h"
#include	<courierauthsaslclient.h>

#include	"rw.h"
#include	"esmtpconfig.h"

#include	<sys/types.h>
#include	<sys/uio.h>
#include	<sys/socket.h>
#include	<sys/wait.h>
#if	HAVE_NETINET_TCP_H
#include	<netinet/tcp.h>
#endif
#if	HAVE_UTIME
#include	<utime.h>
#endif


#include	<ctype.h>
#include	<string.h>
#if HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if HAVE_FCNTL_H
#include	<fcntl.h>
#endif

#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<signal.h>
#include	<errno.h>

static time_t esmtpkeepaliveping;
static time_t connect_timeout;
static time_t cmd_timeout;
static time_t helo_timeout;
static time_t quit_timeout;
static time_t data_timeout;

#ifdef	TCP_CORK

static int esmtp_cork;
static int corked;

#define	cork(n) \
	{\
	int flag=(n);\
\
		if (esmtp_cork && esmtp_sockfd >= 0 && corked != flag) \
		{ \
			setsockopt(esmtp_sockfd, SOL_TCP, TCP_CORK, &flag, \
							sizeof(flag));\
		} \
		corked=flag;\
	}
#else
#define	cork(n)
#endif

static time_t net_timeout;
	/*
	** If all MXs are unreachable, wait until this tick before attempting
	** any new connections.
	*/
static int net_error;

static void sendesmtp(struct moduledel *, struct ctlfile *);
static void quit();


extern struct rw_list *esmtp_rw_install(const struct rw_install_info *);
extern int isloopback(const char *);

static void (*rewrite_func)(struct rw_info *, void (*)(struct rw_info *));

void rfc2045_error(const char *p)
{
	clog_msg_start_err();
	clog_msg_str(p);
	clog_msg_send();
	_exit(1);
}

static void setconfig()
{
       esmtpkeepaliveping=config_time_esmtpkeepaliveping();
       cmd_timeout=config_time_esmtptimeout();
       connect_timeout=config_time_esmtpconnect();
       cmd_timeout=config_time_esmtptimeout();
       helo_timeout=config_time_esmtphelo();
       data_timeout=config_time_esmtpdata();
       quit_timeout=config_time_esmtpquit();
}

/* We get here as a new child process of the courieresmtp driver */

void esmtpchild(unsigned childnum)
{
struct	moduledel *del;
struct	ctlfile	ctf;
unsigned long mypid=(unsigned long)getpid();

	signal(SIGPIPE, SIG_IGN);
	srand(time(NULL));
	rw_init_courier("esmtp");
	rewrite_func=rw_search_transport("esmtp")->rw_ptr->rewrite;

	if (chdir(courierdir()))
		clog_msg_errno();
	esmtp_sockfd= -1;

	setconfig();

	net_timeout=0;

#ifdef	TCP_CORK

	{
	const char *p=getenv("ESMTP_CORK");

		esmtp_cork=p ? atoi(p):0;
		corked=0;
	}

#endif

	/* Retrieve delivery request until courieresmtp closes the pipe */

	while ((del=module_getdel()) != 0)
	{
	fd_set	fdr;
	struct	timeval	tv;
	const char *p;

#if 0
		clog_msg_start_info();
		clog_msg_str("Process ");
		clog_msg_uint(getpid());
		clog_msg_str(" ready to be grabbed.");
		clog_msg_send();
		sleep(60);
#endif

		/*
		** Open the message control file, send the message, close
		** the control file, we're done.
		*/

		if (ctlfile_openi(del->inum, &ctf, 0) == 0)
		{
			int changed_vhosts=ctlfile_setvhost(&ctf);

			if (changed_vhosts)
			{
				quit();
				setconfig();
			}

			sendesmtp(del, &ctf);
			ctlfile_close(&ctf);
		}
		{
		char	pidbuf[NUMBUFSIZE];

			printf("%u %s\n", childnum, libmail_str_pid_t(mypid, pidbuf));
			fflush(stdout);
		}

		/*
		** While waiting for the next message, push a RSET every
		** so-so seconds
		*/

		while (esmtpkeepaliveping && esmtp_sockfd >= 0)
		{
			FD_ZERO(&fdr);
			FD_SET(0, &fdr);
			tv.tv_sec=esmtpkeepaliveping;
			tv.tv_usec=0;

			if ( sox_select(1, &fdr, 0, 0, &tv) > 0)
				break;
			esmtp_timeout(data_timeout);
			if (esmtp_writestr("RSET\r\n") || esmtp_writeflush())
				break;

			while ( (p=esmtp_readline()) != 0 && !ISFINALLINE(p))
				;

			if (p == 0)
			{
				quit();
				break;
			}
		}
	}
	if (esmtp_sockfd >= 0)
		quit();
}

static RFC1035_ADDR sockfdaddr;
static char *sockfdaddrname=0;
static char *auth_key=0;
static char *host;
static char *authsasllist=0;
static int is_secure_connection=0;
static int smtproutes_flags=0;

static int hasdsn, haspipelining, has8bitmime, hasverp, hassize, hasexdata,
	hascourier, hasstarttls, hassecurity_starttls;

static void hard_error(struct moduledel *, struct ctlfile *, const char *);
static void soft_error(struct moduledel *, struct ctlfile *, const char *);

static void connect_error(struct moduledel *, struct ctlfile *);

static int hello(struct moduledel *, struct ctlfile *);
static int local_sock_address(struct moduledel *, struct ctlfile *);
static int starttls(struct moduledel *, struct ctlfile *, const char *, int);
static int authclient(struct moduledel *, struct ctlfile *, const char *);

static int rset(struct moduledel *, struct ctlfile *);
static int smtpreply(const char *, struct moduledel *, struct ctlfile *, int);
static void push(struct moduledel *, struct ctlfile *);

static const char *want_security(struct ctlfile *ctf)
{
	const char *sec;

	/* ROUTE_SMTPS already establishes an encrypted connection */
	if (smtproutes_flags & (ROUTE_SMTPS))
		return (0);

	if (smtproutes_flags & ROUTE_STARTTLS)
		return ("STARTTLS");

	sec=ctlfile_security(ctf);

	if (!sec)
		return (0);
	if (strcmp(sec, "STARTTLS") == 0)
		return (sec);
	return (0);
}

static int get_sourceaddr(int af,
			  const RFC1035_ADDR *dest_addr,
			  RFC1035_NETADDR *addrbuf,
			  const struct sockaddr **addrptr, int *addrptrlen)
{
	int rc;
	RFC1035_ADDR in;
	const char *vhost=config_get_local_vhost();
	char vhost_ip_buf[100];
	const char *buf=getenv(
#if RFC1035_IPV6

			       !IN6_IS_ADDR_V4MAPPED(dest_addr)
			       ? "SOURCE_ADDRESS_IPV6":
#endif
			       "SOURCE_ADDRESS");

	char *source=config_localfilename(
#if RFC1035_IPV6

					  !IN6_IS_ADDR_V4MAPPED(dest_addr)
					  ? "ip6out":
#endif
					  "ipout");
	FILE *fp;
	fp=fopen(source, "r");
	free(source);

	if (fp)
	{
		char *p;

		/*
		** If ip[6]out.xxxx exists, read the IP address from it.
		*/
		vhost_ip_buf[0]=0;
		if (fgets(vhost_ip_buf, sizeof(vhost_ip_buf), fp) == NULL)
			vhost_ip_buf[0]=0;
		fclose(fp);

		p=strchr(vhost_ip_buf, '\n');
		if (p) *p=0;

		/*
		** If the file is empty, use the vhost IP address itself.
		*/
		if (vhost_ip_buf[0])
			buf=vhost_ip_buf;
		else if (vhost && *vhost)
			buf=vhost;
	}

	if (buf && strcmp(buf, "0")) {
		rc = rfc1035_aton(buf, &in);
		if (rc != 0)
		{
			errno=EINVAL;
			return rc;
		}
	} else
		in = RFC1035_ADDRANY;

	rc = rfc1035_mkaddress(af, addrbuf, &in, htons(0),
			addrptr, addrptrlen);
	if (rc != 0)
		return rc;

	return 0;
}

static int backscatter(const char *src)
{
	char buf[1024];
	const char *env;
	char *p;

	buf[0]=0;

	env=getenv("ESMTP_BLOCKBACKSCATTER");

	strncat(buf, env ? env:"", 1000);

	for (p=buf; (p=strtok(p, ",")); p=NULL)
	{
		if (strcmp(p, src) == 0)
			return 1;
	}
	return 0;
}

static void talking2(struct moduledel *del, struct ctlfile *ctf, int n);

static void talking(struct moduledel *del, struct ctlfile *ctf)
{
	talking2(del, ctf, -1);
}

static void smtp_error(struct moduledel *del, struct ctlfile *ctf,
		       const char *msg, int errcode);

static void report_broken_starttls();

/* Attempt to deliver a message */

static void sendesmtp(struct moduledel *del, struct ctlfile *ctf)
{
	char *smtproute;
	int cn;
	int connection_attempt_made;

	if (!host)
		host=strcpy(courier_malloc(strlen(del->host)+1), del->host);

	/* Sanity check */

	if (strcmp(host, del->host))
	{
		clog_msg_start_err();
		clog_msg_str("Internal failure in courieresmtp - daemon mixup.");
		clog_msg_send();
		_exit(1);
	}

	/* If we're connected, send a RSET to make sure the socket is working */

	if (esmtp_sockfd >= 0)
	{
		esmtp_timeout(helo_timeout);
		if (esmtp_writestr("RSET\r\n") == 0 && esmtp_writeflush() == 0)
		{
			if (smtpreply("RSET", del, ctf, -1))
			{
				quit();
				return;
			}
		}
	}

	if (esmtp_sockfd < 0 && net_timeout)
	{
	time_t	t;

		time (&t);
		if (t < net_timeout)
		{
			errno=net_error;
			if (!errno)	errno=ENETDOWN;
			connect_error(del, ctf);
			return;
		}
		net_timeout=0;
	}

	if ((cn=ctlfile_searchfirst(ctf, COMCTLFILE_MSGSOURCE)) >= 0 &&
	    backscatter(ctf->lines[cn]+1))
	{
		int i;

		for (i=0; i<del->nreceipients; i++)
		{
			ctlfile_append_connectioninfo(ctf,
						      (unsigned)
						      atol(del->receipients
							   [i*2]),
						      COMCTLFILE_DELINFO_REPLYTYPE,
						      "smtp");

			ctlfile_append_connectioninfo(ctf,
						      (unsigned)
						      atol(del->receipients
							   [i*2]),
						      COMCTLFILE_DELINFO_REPLY,
						      "250 Backscatter bounce dropped.");

			ctlfile_append_reply(ctf,
					     (unsigned)atol( del->receipients[i*2]),
					     "delivered: backscatter bounce dropped",
					     COMCTLFILE_DELSUCCESS_NOLOG,
					     (hasdsn ? "":" r"));
		}
		return;
	}

	/*
	** If the message wants a secured connection, and the current
	** connection has not been secured, close it, so it can be reopened.
	*/

	smtproute=smtproutes(host, &smtproutes_flags);

	if (esmtp_sockfd >= 0 && want_security(ctf) && !is_secure_connection)
		quit();

	if (esmtp_sockfd < 0)	/* First time, connect to a server */
	{
		struct rfc1035_mxlist *mxlist, *p, *q;
		int static_route= smtproute != NULL;
		struct rfc1035_res res;
		int rc;

		errno=0;	/* Detect network failures */

		if (auth_key)
			free(auth_key);

		auth_key=strdup(smtproute ? smtproute:host);

		if (!auth_key)
			clog_msg_errno();

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
			if (smtproute)	free(smtproute);
			hard_error(del, ctf, "No such domain.");
			return;
		case RFC1035_MX_BADDNS:
			if (smtproute)	free(smtproute);
			hard_error(del, ctf,
				"This domain's DNS violates RFC 1035.");
			return;
		default:
			if (smtproute)	free(smtproute);
			soft_error(del, ctf, "DNS lookup failed.");

			if (errno)
			{
				net_error=errno;
				time (&net_timeout);
				net_timeout += config_time_esmtpdelay();
			}
			return;
		}
		if (smtproute)	free(smtproute);

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
				hard_error(del, ctf,
					"This domain's DNS violates RFC 1035.");
				rfc1035_mxlist_free(mxlist);
				return;
			}

			if (!q && !static_route &&
			    (config_islocal(p->hostname, 0)
			     || isloopback(buf)))
				q=p;
		}

		if (q && q->priority == mxlist->priority)
		{
			hard_error(del, ctf, "configuration error: mail loops back to myself (MX problem).");
			rfc1035_mxlist_free(mxlist);
			return;
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

			sockfdaddr=addr;
			if (sockfdaddrname)	free(sockfdaddrname);
			sockfdaddrname=strcpy(
				courier_malloc(strlen(p->hostname)+1),
				p->hostname);	/* Save this for later */

			is_secure_connection=0;
			if ((esmtp_sockfd=rfc1035_mksocket(SOCK_STREAM, 0, &af))
					>= 0 &&
			    rfc1035_mkaddress(af, &addrbuf, &addr, port,
					      &addrptr, &addrptrlen) == 0 &&
			    get_sourceaddr(af, &addr, &saddrbuf, &saddrptr,
					   &saddrptrlen) == 0 &&
			    rfc1035_bindsource(esmtp_sockfd, saddrptr,
					       saddrptrlen) == 0 &&
			    s_connect(esmtp_sockfd, addrptr, addrptrlen,
				      connect_timeout) == 0)
			{
				/*
				** If we're connected, make sure EHLO or HELO
				** is cool, before blessing the connection.
				*/

				int	rc;

				if (local_sock_address(del, ctf) < 0)
				{
					sox_close(esmtp_sockfd);
					esmtp_sockfd= -1;
					return;
				}

				if (smtproutes_flags & ROUTE_SMTPS)
				{
					if (starttls(del, ctf, p->hostname, 1))
					{
						sox_close(esmtp_sockfd);
						esmtp_sockfd= -1;
						continue; /* Next MX, please */
					}

					smtproutes_flags &= ~(ROUTE_TLS_REQUIRED);
				}

				rc=hello(del, ctf);

				if (rc == 0)
				{
					/*
					** If the following fails, go to the
					** next MX.
					*/

					rc=1;

					if ((smtproutes_flags
					     & ROUTE_TLS_REQUIRED)
					    && !hasstarttls)
					{
						talking(del, ctf);
						smtp_error(del, ctf,
							   "/SECURITY=REQUIRED set, but TLS is not available",
							   '5');
					}
					else if (want_security(ctf)
						 && !hasstarttls)
					{
						talking(del, ctf);
						smtp_error(del, ctf,
							   "/SECURITY set, but TLS is not available",
							   '5');
					}
					else if (hasstarttls &&
						 starttls(del, ctf,
							  p->hostname,
							  0))
					{
						/*
						** Only keep track of broken
						** STARTTLS if we did not ask
						** for SECURITY.
						**
						** Otherwise, it's on us.
						*/
						if (!want_security(ctf))
							report_broken_starttls();
					}
					else if (authclient(del, ctf, auth_key))
					{
						;
					}
					else break; /* Good HELO */
				}
				quit();	/* We don't want to talk to him */
				if (rc < 0)
					return;	/* HELO failed, perm error */
				errno=0;
			}
			if (errno)
				net_error=errno;

			if (esmtp_sockfd >= 0)
				sox_close(esmtp_sockfd);
			esmtp_sockfd= -1;

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
		if (esmtp_sockfd < 0)	/* Couldn't find an active server */
		{
			if (!connection_attempt_made)
				hard_error(del, ctf, "Did not find a suitable MX for a connection");
			else
			{
				if (net_error)
				{
					errno=net_error;
					soft_error(del, ctf, strerror(errno));
				}
				time (&net_timeout);
				net_timeout += config_time_esmtpdelay();
			}
			return;
		}
	}
	else
	{
		if (smtproute)
			free(smtproute);
	}

	/*
	** Ok, we now have a connection.  We want to call push() to deliver
	** this message, but if the VERP flag is set but the remote server
	** does not grok VERPs, we need to do a song-n-dance routine.
	*/

	if (hasverp || ctlfile_searchfirst(ctf, COMCTLFILE_VERP) < 0)
	{
		push(del, ctf);	/* ... but not this time */
		return;
	}

	/*
	** Ok, so what we do is to call push() individually for each
	** recipient, manually munging the return address each time, and
	** fudging the delivery record setting it for that one recipient
	** only.
	*/

	{
	unsigned i;
	unsigned real_recip=del->nreceipients;

		del->nreceipients=1;
		for (i=0; i<real_recip; i++, del->receipients += 2)
		{
		char	*verp_sender;

			if (i && esmtp_sockfd >= 0)	/* Call RSET in between */
			{
				if (rset(del, ctf))
				{
					quit();
					continue;
				}
			}
			if (esmtp_sockfd < 0)
			{
				connect_error(del, ctf);
				continue;
			}

			verp_sender=verp_getsender(ctf, del->receipients[1]);

			del->sender=verp_sender;
			push(del, ctf);
			free(verp_sender);
		}
	}
}

/* Record a permanent failure for one, or all, recipients */

static void hard_error1(struct moduledel *del, struct ctlfile *ctf,
		const char *msg, int n)
{
unsigned        i;

	if (n >= 0)
		ctlfile_append_reply(ctf,
			(unsigned)atol(del->receipients[n*2]), msg,
			COMCTLFILE_DELFAIL, 0);
	else for (i=0; i<del->nreceipients; i++)
		ctlfile_append_reply(ctf,
			(unsigned)atol(del->receipients[i*2]), msg,
			COMCTLFILE_DELFAIL, 0);
}

static void hard_error(struct moduledel *del, struct ctlfile *ctf,
		const char *msg)
{
	hard_error1(del, ctf, msg, -1);
}

/* Record a temporary failure for one, or all, the recipients */

static void soft_error1(struct moduledel *del, struct ctlfile *ctf,
	const char *msg, int n)
{
unsigned        i;

	if (n >= 0)
		ctlfile_append_reply(ctf,
			(unsigned)atol(del->receipients[n*2]), msg,
			COMCTLFILE_DELDEFERRED, 0);
	else for (i=0; i<del->nreceipients; i++)
		ctlfile_append_reply(ctf,
			(unsigned)atol(del->receipients[i*2]), msg,
			COMCTLFILE_DELDEFERRED, 0);
}

static void soft_error(struct moduledel *del, struct ctlfile *ctf,
	const char *msg)
{
	soft_error1(del, ctf, msg, -1);
}

static void connection_closed(struct moduledel *del, struct ctlfile *ctf)
{
	soft_error(del, ctf, "Connection unexpectedly closed by remote host.");
}

/* Record an SMTP error for all the recipients */

static void smtp_error(struct moduledel *del, struct ctlfile *ctf,
		       const char *msg, int errcode)
{
	if (!msg)
	{
		connect_error(del, ctf);
		return;
	}

	if (errcode == 0)
		errcode= *msg;

	if (errcode == '5')
		hard_error1(del, ctf, msg, -1);
	else
		soft_error1(del, ctf, msg, -1);
}


/* Record our peer in the message's control file, for error messages */

static void sockipname(char *buf)
{
	rfc1035_ntoa( &sockfdaddr, buf);

#if	RFC1035_IPV6

	if (IN6_IS_ADDR_V4MAPPED(&sockfdaddr))
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

static void talking2(struct moduledel *del, struct ctlfile *ctf, int n)
{
char	buf[RFC1035_NTOABUFSIZE];
unsigned i;
char	*p;

	sockipname(buf);
	p=courier_malloc(strlen(sockfdaddrname)+strlen(buf)+
		sizeof(" []"));
	strcat(strcat(strcat(strcpy(p, sockfdaddrname), " ["),
		buf), "]");

	if (n >= 0)
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[n*2]),
			COMCTLFILE_DELINFO_PEER, p);
	else for (i=0; i<del->nreceipients; i++)
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
			COMCTLFILE_DELINFO_PEER, p);
	free(p);
}

/* TCP/IP error */

static void connect_error1(struct moduledel *del, struct ctlfile *ctf, int n)
{
#if	HAVE_STRERROR

	if (errno)
		soft_error1(del, ctf, errno == ECONNRESET
			    ? "Network connection shut down by the remote mail server"
			    : strerror(errno), n);
	else
#endif
		soft_error1(del, ctf, "Connection closed by remote host.", n);
}

static void connect_error(struct moduledel *del, struct ctlfile *ctf)
{
	connect_error1(del, ctf, -1);
}

/* Log reply received */

static void smtp_msg(struct moduledel *del, struct ctlfile *ctf)
{
unsigned	i;

	for (i=0; i<del->nreceipients; i++)
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
			COMCTLFILE_DELINFO_REPLYTYPE, "smtp");
}

static void reply(struct moduledel *del, struct ctlfile *ctf, const char *msg)
{
unsigned        i;

	for (i=0; i<del->nreceipients; i++)
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
			COMCTLFILE_DELINFO_REPLY, msg);
}

/* Log the command sent to remote server */

static void sent(struct moduledel *del, struct ctlfile *ctf, const char *msg)
{
unsigned        i;

	for (i=0; i<del->nreceipients; i++)
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
				COMCTLFILE_DELINFO_SENT, msg);
}

/***************************************************************************

  Socket stuff.  All socket operations have a timeout which is set separately,
  then we wait for the socket to be ready for reading or writing, then
  closing the socket if the timeout expires before the socket is ready.

***************************************************************************/

struct my_esmtp_info {
	struct moduledel *del;
	struct ctlfile *ctf;
};

static RFC1035_ADDR laddr;

/* Try an EHLO */

static void log_talking(struct esmtp_info *info, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	talking(my_info->del, my_info->ctf);
}

static void log_sent(struct esmtp_info *ingo, const char *str, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	sent(my_info->del, my_info->ctf, str);
	smtp_msg(my_info->del, my_info->ctf);
}

static void log_reply(struct esmtp_info *info, const char *str, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	reply(my_info->del, my_info->ctf, str);
}

static void log_smtp_error(struct esmtp_info *info,
			   const char *msg, int errcode, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	smtp_error(my_info->del, my_info->ctf, msg, errcode);
}

#define LIBESMTP_COMPAT do {					\
		LIBESMTP_MOVE(haspipelining);			\
		LIBESMTP_MOVE(hasdsn);				\
		LIBESMTP_MOVE(has8bitmime);			\
		LIBESMTP_MOVE(hasverp);				\
		LIBESMTP_MOVE(hassize);				\
		LIBESMTP_MOVE(hasexdata);			\
		LIBESMTP_MOVE(hascourier);			\
		LIBESMTP_MOVE(hasstarttls);			\
		LIBESMTP_MOVE(hassecurity_starttls);		\
		LIBESMTP_MOVE(is_secure_connection);		\
	} while(0)

static struct esmtp_info *libesmtp_init(struct my_esmtp_info *my_info,
					struct ctlfile *ctf,
					struct moduledel *del)
{
	struct esmtp_info *info=esmtp_info_alloc();

	my_info->del=del;
	my_info->ctf=ctf;

	info->log_talking= &log_talking;
	info->log_sent= &log_sent;
	info->log_reply= &log_reply;
	info->log_smtp_error= &log_smtp_error;

	info->laddr=laddr;

#define LIBESMTP_MOVE(n) info->n=n
	LIBESMTP_COMPAT;
#undef LIBESMTP_MOVE
	return info;
}

static void libesmtp_deinit(struct esmtp_info *info)
{
#define LIBESMTP_MOVE(n) n=info->n
	LIBESMTP_COMPAT;
#undef LIBESMTP_MOVE

	esmtp_info_free(info);
}

/*
	Try EHLO then HELO, and see what the other server says.
*/

static int hello3(struct esmtp_info *info,
		  struct my_esmtp_info *my_info,
		  int using_tls);

static int local_sock_address(struct moduledel *del, struct ctlfile *ctf)
{
	RFC1035_NETADDR lsin;
	socklen_t i;

	i=sizeof(lsin);
	if (sox_getsockname(esmtp_sockfd, (struct sockaddr *)&lsin, &i) ||
	    rfc1035_sockaddrip(&lsin, i, &laddr))
	{
		soft_error1(del, ctf, "Cannot obtain local socket IP address.",
			    -1);
		return -1;
	}
	return 0;
}

static int hello(struct moduledel *del, struct ctlfile *ctf)
{
	struct my_esmtp_info my_info;
	struct esmtp_info *info=libesmtp_init(&my_info, ctf, del);

	int rc=esmtp_get_greeting(info, &my_info);

	if (rc == 0)
		rc=hello3(info, &my_info, 0);

	if (info->quit_needed)
		quit();

	libesmtp_deinit(info);

	return rc;
}

static int hello3(struct esmtp_info *info,
		  struct my_esmtp_info *my_info,
		  int using_tls)
{
	int rc;
	time_t timestamp;

	rc=esmtp_helo(info, using_tls, want_security(my_info->ctf), my_info);

	if (rc == -1)
		quit();

	if (track_find_broken_starttls(sockfdaddrname, &timestamp))
		info->hasstarttls=0;

	return rc;
}

static void report_broken_starttls()
{
	track_save_broken_starttls(sockfdaddrname);
}

static int starttls(struct moduledel *del, struct ctlfile *ctf,
		    const char *hostname, int smtps)
{
	struct my_esmtp_info my_info;

	struct esmtp_info *info=libesmtp_init(&my_info, ctf, del);

	int rc=esmtp_enable_tls(info, hostname, smtps,
				want_security(ctf), &my_info);

	libesmtp_deinit(info);

	return rc;
}

/*
** Send a QUIT, and shut down the connection
*/

static void quit()
{
const char *p;

	if (esmtp_sockfd < 0)	return;

	esmtp_timeout(quit_timeout);
	if (esmtp_writestr("QUIT\r\n") || esmtp_writeflush())	return;

	while ((p=esmtp_readline()) != 0 && !ISFINALLINE(p))
		;
	if (esmtp_sockfd >= 0)
		sox_close(esmtp_sockfd);
	esmtp_sockfd= -1;
}

/* Parse a reply to a SMTP command that applies to all recipients */

static int smtpreply(const char *cmd,
	struct moduledel *del, struct ctlfile *ctf, int istalking)
{
const char *p;
unsigned line_num;

	if ((p=esmtp_readline()) == 0)
	{
		if (istalking < 0)	return (0);

		if (!istalking)
			talking(del, ctf);
		connect_error(del, ctf);
		quit();
		return (-1);
	}

	line_num=0;

	switch (SMTPREPLY_TYPE(p))	{
	case COMCTLFILE_DELDEFERRED:
	case COMCTLFILE_DELFAIL:

		if (!istalking || istalking < 0)
			talking(del, ctf);
		sent(del, ctf, cmd);
		smtp_msg(del, ctf);
		while (!ISFINALLINE(p))
		{
			if (line_num < 10)	/* We record up to 10 lines
						** of the reply in our log
						** files.
						*/
			{
				reply(del, ctf, p);
				++line_num;
			}
			if ((p=esmtp_readline()) == 0)
			{
				connect_error(del, ctf);
				quit();
				return (-1);
			}
		}
		smtp_error(del, ctf, p, 0);
		return (-1);
	}

	while (!ISFINALLINE(p))
	{
		if ((p=esmtp_readline()) == 0)
		{
			if (!istalking || istalking < 0)
				talking(del, ctf);
			connect_error(del, ctf);
			quit();
			return (-1);
		}
	}
	return (0);
}

/* Send an SMTP command that applies to all recipients, then wait for a reply */

static int smtpcommand(const char *cmd,
	struct moduledel *del, struct ctlfile *ctf, int istalking)
{
	if (esmtp_writestr(cmd) || esmtp_writeflush())
	{
		if (!istalking)
			talking(del, ctf);
		connect_error(del, ctf);
		quit();
		return (-1);
	}
	return (smtpreply(cmd, del, ctf, istalking));
}


static int rset(struct moduledel *del, struct ctlfile *ctf)
{
	esmtp_timeout(helo_timeout);
	return (smtpcommand("RSET\r\n", del, ctf, 0));
}

static void pushdsn(struct moduledel *, struct ctlfile *);

/*
**	We now resolved issues with VERP support.  Resolve issues with
**	DSNs.  The next function to call is pushdsn.  The following issue
**	is resolved here: sending a message to a server that does not
**	support DSNs.  In this situation what we want to do is to send
**	all recipients with a NOTIFY=NEVER in a transaction where MAIL FROM
**	is <>.  Everyone else is sent in a separate transaction.
**
**	If this is not applicable (the remote server supports DSNs), we
**	simply call pushdsn() to continue with the delivery attempt.
**	Otherwise we call pushdsn() twice.
*/

static void push(struct moduledel *del, struct ctlfile *ctf)
{
unsigned	i;
char	**real_receipients;
const char	*real_sender;
int	pass;
unsigned real_nreceipients;

	real_sender=del->sender;
	real_receipients=del->receipients;
	real_nreceipients=del->nreceipients;

	if (real_nreceipients == 0)	return;

	/* If the sender is <> already, I don't care */

	if (hasdsn || real_sender == 0 || *real_sender == '\0')
	{
		pushdsn(del, ctf);
		return;
	}

	/*
	** If the remote MTA does not support DSNs, and we have some
	** receipients with NOTIFY=NEVER, what we do is set the MAIL FROM:
	** for those recipients to <>.  We call pushdsn twice, once for
	** receipients with NOTIFY=NEVER, and once more for ones without
	** NOTIFY=NEVER.
	*/

	del->receipients=(char **)courier_malloc(
			sizeof(char *)*2*del->nreceipients);

	for (pass=0; pass<2; pass++)
	{
		if (pass)	del->sender="";
		del->nreceipients=0;

		for (i=0; i<real_nreceipients; i++)
		{
		const char *dsnptr=ctf->dsnreceipients[
			atol(real_receipients[i*2])];

			if (dsnptr && strchr(dsnptr, 'N'))
			{
				if (pass == 0)	continue;
			}
			else
			{
				if (pass == 1)	continue;
			}
			del->receipients[del->nreceipients*2]=
				real_receipients[i*2];
			del->receipients[del->nreceipients*2+1]=
				real_receipients[i*2+1];
			++del->nreceipients;
		}
		if (del->nreceipients == 0)	continue;
		pushdsn(del, ctf);
	}
	free(del->receipients);
	del->receipients=real_receipients;
	del->nreceipients=real_nreceipients;
	del->sender=real_sender;
}

/*
** Construct the MAIL FROM: command, taking into account ESMTP capabilities
** of the remote server.
*/

static char *mailfrom(struct moduledel *del, struct ctlfile *ctf,
	int messagefd, int is8bitmsg)
{
char	*bodyverb="", *verpverb="", *retverb="";
char	*oenvidverb="", *sizeverb="";
const char *seclevel="";
char	*mailfromcmd;
int	n;
struct	stat stat_buf;
const char *sec=want_security(ctf);

static const char seclevel_starttls[]=" SECURITY=STARTTLS";

	if (has8bitmime)	/* ESMTP 8BITMIME capability */
		bodyverb= is8bitmsg ? " BODY=8BITMIME":" BODY=7BIT";

	if (hasverp && ctlfile_searchfirst(ctf, COMCTLFILE_VERP) >= 0)
		verpverb=" VERP";	/* ESMTP VERP capability */

	/* ESMTP DSN capability */
	if (hasdsn && (n=ctlfile_searchfirst(ctf, COMCTLFILE_DSNFORMAT)) >= 0)
		retverb=strchr(ctf->lines[n]+1, 'F') ? " RET=FULL":
			strchr(ctf->lines[n]+1, 'H') ? " RET=HDRS":"";
	if (hasdsn && (n=ctlfile_searchfirst(ctf, COMCTLFILE_ENVID)) >= 0 &&
			ctf->lines[n][1])
	{
		oenvidverb=courier_malloc(sizeof(" ENVID=")+
			strlen(ctf->lines[n]+1));
		strcat(strcpy(oenvidverb, " ENVID="), ctf->lines[n]+1);
	}

	/* ESMTP SIZE capability */

	if (fstat(messagefd, &stat_buf) == 0)
	{
		ctf->msgsize=stat_buf.st_size;

		if (hassize)
		{
			off_t s=stat_buf.st_size;
			char	buf[MAXLONGSIZE+1];

			s= s/75 * 77+256;	/* Size estimate */
			if (!has8bitmime && is8bitmsg)
				s=s/70 * 100;
			sprintf(buf, "%lu", (unsigned long)s);
			sizeverb=courier_malloc(sizeof(" SIZE=")+strlen(buf));
			strcat(strcpy(sizeverb, " SIZE="), buf);
		}
	}

	/* SECURITY extension */

	if (sec && strcmp(sec, "STARTTLS") == 0)
		seclevel=seclevel_starttls;

	mailfromcmd=courier_malloc(sizeof("MAIL FROM:<>\r\n")+
				   strlen(del->sender)+
				   strlen(bodyverb)+
				   strlen(verpverb)+
				   strlen(retverb)+
				   strlen(oenvidverb)+
				   strlen(sizeverb)+
				   strlen(seclevel));

	strcat(strcat(strcat(strcat(strcat(
		strcat(strcat(strcat(strcat(strcpy(
						   mailfromcmd, "MAIL FROM:<"),
					    del->sender),
				     ">"),
			      bodyverb),
		       verpverb),
		retverb),
				    oenvidverb),
			     sizeverb),
		      seclevel),
	       "\r\n");

	if (*oenvidverb)	free(oenvidverb);
	if (*sizeverb)		free(sizeverb);
	return (mailfromcmd);
}

/*
** Construct the RCPT TO command along the same lines.
*/

static char *rcptcmd(struct moduledel *del,
	struct ctlfile *ctf, unsigned rcptnum)
{
char notify[sizeof(" NOTIFY=SUCCESS,FAILURE,DELAY")];
char *orcpt="";
const char *p;
char	*q;
unsigned n=atol(del->receipients[rcptnum*2]);

	notify[0]=0;
	if ((p=ctf->dsnreceipients[n]) != 0 && *p && hasdsn)
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
			strcpy(notify, " NOTIFY=NEVER");
		else
		{
			p=" NOTIFY=";
			if (s)
			{
				strcat(strcat(notify, p), "SUCCESS");
				p=",";
			}
			if (f)
			{
				strcat(strcat(notify, p), "FAILURE");
				p=",";
			}
			if (d)
				strcat(strcat(notify, p), "DELAY");
		}
	}

	if ((p=ctf->oreceipients[n]) != 0 && *p && hasdsn)
	{
		orcpt=courier_malloc(sizeof(" ORCPT=")+strlen(p));
		strcat(strcpy(orcpt, " ORCPT="), p);
	}

	p=del->receipients[rcptnum*2+1];

	q=courier_malloc(sizeof("RCPT TO:<>\r\n")+strlen(p)+strlen(notify)+
		strlen(orcpt));

	strcat(strcat(strcat(strcat(strcat(strcpy(q,
		"RCPT TO:<"),
		p),
		">"),
		notify),
		orcpt),
		"\r\n");
	if (*orcpt)	free(orcpt);
	return (q);
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

static const char *readpipelinercpt( struct iovec **, unsigned *);

static int parsedatareply(struct moduledel *, struct ctlfile *,
	int *, struct iovec **, unsigned *, int);

static int do_pipeline_rcpt(struct moduledel *del,
	struct ctlfile *ctf,
	int *rcptok)
{
char	**cmdarray;
struct iovec *iov;
struct iovec *	iovw;
unsigned	niovw;

unsigned i;
const char *p;

int	rc=0;

	/* Construct all the RCPT TOs we'll issue. */

	cmdarray=(char **)courier_malloc(sizeof(char *)*
		(del->nreceipients+1));	/* 1 extra PTR for the DATA */
	iov=(struct iovec *)courier_malloc(sizeof(struct iovec)*
		(del->nreceipients+1));

	/* Allocate cmdarray[], also set up iovecs to point to each cmd */

	for (i=0; i <= del->nreceipients; i++)
	{
		cmdarray[i]= i < del->nreceipients ?  rcptcmd(del, ctf, i):
				strcpy(courier_malloc(sizeof("DATA\r\n")),
						"DATA\r\n");
		iov[i].iov_base=(caddr_t)cmdarray[i];
		iov[i].iov_len=strlen(cmdarray[i]);
	}

	iovw=iov;
	niovw= i;

	if (haspipelining)	/* One timeout */
		esmtp_timeout(cmd_timeout);

	/* Read replies for the RCPT TO commands */

	for (i=0; i<del->nreceipients; i++)
	{
	char	err_code=0;
	unsigned line_num=0;

		/* If server can't do pipelining, just set niovw to one!!! */

		if (!haspipelining)
		{
			iovw=iov+i;
			niovw=1;
			esmtp_timeout(cmd_timeout);
		}

		do
		{
			if ((p=readpipelinercpt( &iovw, &niovw)) == 0)
				break;

			if (line_num == 0)
				err_code= *p;

			if ( SMTPREPLY_TYPE(&err_code) ==
				COMCTLFILE_DELSUCCESS)
				continue;

			if (line_num >= 10)	continue;
			/* Ignore SMTP replies longer than 10 lines */

			if (line_num == 0)
			{
				ctlfile_append_connectioninfo(ctf,
					(unsigned)atol(del->receipients[i*2]),
					COMCTLFILE_DELINFO_SENT,
					cmdarray[i]);
				ctlfile_append_connectioninfo(ctf,
					(unsigned)atol(del->receipients[i*2]),
					COMCTLFILE_DELINFO_REPLYTYPE,
					"smtp");
			}
			ctlfile_append_connectioninfo(ctf,
				(unsigned)atol(del->receipients[i*2]),
					COMCTLFILE_DELINFO_REPLY, p);
		} while (!ISFINALLINE(p));

		if (!p)
		{
			while (i < del->nreceipients)
				rcptok[i++]=1;
			break;
		}

		if ( SMTPREPLY_TYPE(&err_code) == COMCTLFILE_DELSUCCESS)
		{
			rcptok[i]=1;	/* This recipient was accepted */
			continue;
		}

		/* Failed.  Report it */

		rcptok[i]=0;

		if (SMTPREPLY_TYPE(&err_code) == COMCTLFILE_DELFAIL)
			hard_error1(del, ctf, 0, i);
		else
			soft_error1(del, ctf, 0, i);
	}

/* ------------------- Read the reply to the DATA ----------------- */

	if (esmtp_sockfd >= 0)
	{
		if (!haspipelining)	/* DATA hasn't been sent yet */
		{
			for (i=0; i<del->nreceipients; i++)
				if (rcptok[i])	break;

			if (i >= del->nreceipients)	return (-1);
					/* All RCPT TOs failed */

			iovw=iov+del->nreceipients;
			niovw=1;
			esmtp_timeout(cmd_timeout);
		}
		rc=parsedatareply(del, ctf, rcptok, &iovw, &niovw, 0);
			/* One more reply */
	}

	if (esmtp_sockfd < 0)
	{
		for (i=0; i<del->nreceipients; i++)
		{
			if (!rcptok[i])	continue;
			connect_error1(del, ctf, i);
		}
		rc= -1;
	}

	for (i=0; i<del->nreceipients; i++)
		free(cmdarray[i]);
	free(cmdarray);
	free(iov);
	return (rc);
}

/* Sigh... When SOCKSv5 supports writev, I'll be happy... */

static int my_writev(int fd, const struct iovec *vector, size_t count)
{
char	buf[BUFSIZ];
size_t	i=0;

	while (count)
	{
		if (vector->iov_len > sizeof(buf)-i)	break;

		memcpy(buf+i, vector->iov_base, vector->iov_len);
		i += vector->iov_len;
		++vector;
		--count;
	}
	if (i)
		return (sox_write(fd, buf, i));

	return (sox_write(fd, vector->iov_base, vector->iov_len));
}

/* Read an SMTP reply line in pipeline mode */

static const char *readpipelinercpt(
		struct iovec **iovw,	/* Write pipeline */
		unsigned *niovw)
{
int	read_flag, write_flag, *writeptr;

	if (esmtp_sockfd < 0)	return (0);

	if (mybuf_more(&esmtp_sockbuf))
		return (esmtp_readline());	/* We have the reply buffered */

	do
	{
		write_flag=0;
		writeptr= &write_flag;
		if (iovw == 0 || niovw == 0 || *niovw == 0)
			writeptr=0;

		esmtp_wait_rw(&read_flag, writeptr);

		if (write_flag)	/* We can squeeze something out now */
		{
		int	n=my_writev(esmtp_sockfd, *iovw, *niovw);

			if (n < 0)
			{
				sox_close(esmtp_sockfd);
				esmtp_sockfd=-1;
				return (0);
			}

			/* Update iovecs to reflect # bytes written */

			while (n)
			{
				if (n < (*iovw)->iov_len)
				{
					(*iovw)->iov_base=(caddr_t)
						( (char *)(*iovw)->iov_base+n);
					(*iovw)->iov_len -= n;
					break;
				}
				n -= (*iovw)->iov_len;
				++*iovw;
				--*niovw;
			}
		}
	} while (!read_flag && esmtp_sockfd >= 0);

	return (esmtp_readline());
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

static int parseexdatareply(const char *, struct moduledel *, struct ctlfile *,
	int *);

static char *logsuccessto()
{
char	buf[RFC1035_NTOABUFSIZE];
char	*p;

	sockipname(buf);
	p=courier_malloc(sizeof("delivered:  []")+
		(sockfdaddrname ?
			strlen(sockfdaddrname):0)+strlen(buf));

	strcpy(p, "delivered: ");
	if (sockfdaddrname && *sockfdaddrname)
		strcat(strcat(p, sockfdaddrname), " ");
	strcat(p, "[");
	strcat(p, buf);
	strcat(p, "]");
	return (p);
}

static int parsedatareply(struct moduledel *del, struct ctlfile *ctf,
	int *rcptok, struct iovec **iovw, unsigned *niovw, int isfinal)
{
const char *p;
unsigned line_num=0;
unsigned i;

	p=readpipelinercpt(iovw, niovw);

	if (!p)	return (-1);

	if (SMTPREPLY_TYPE(p) == COMCTLFILE_DELSUCCESS)
	{
		/*
		** DATA went through.  What we do depends on whether this is
		** the first or the last DATA.
		*/

		for (;;)
		{
			if (isfinal && line_num < 10)
			{
				/* We want to record the final DATA reply in
				** the log.
				*/

				for (i=0; i<del->nreceipients; i++)
				{
					if (!rcptok[i])	continue;

					if (line_num == 0)
						ctlfile_append_connectioninfo(
							ctf,
						(unsigned)atol(
							del->receipients[i*2]),
						COMCTLFILE_DELINFO_REPLYTYPE,
						"smtp");

					ctlfile_append_connectioninfo(ctf,
						(unsigned)atol(
							del->receipients[i*2]),
						COMCTLFILE_DELINFO_REPLY, p);
				}
				++line_num;
			}
			if (ISFINALLINE(p))	break;
			if ((p=esmtp_readline()) == 0)
				return (-1);
		}

		if (isfinal)
		{
		char	*p=logsuccessto();

			/*
			** Final reply - record a success for recipients that
			** haven't previously failed (in RCPT TO).
			*/

			for (i=0; i<del->nreceipients; i++)
			{
				if (!rcptok[i])	continue;

				ctlfile_append_reply(ctf,
					(unsigned)atol(del->receipients[i*2]),
					p, COMCTLFILE_DELSUCCESS_NOLOG,
					(hasdsn ? "":" r"));
			}
			free(p);
		}
		else
		{
			/* Good response to the first DATA */

			for (i=0; i<del->nreceipients; i++)
				if (rcptok[i]) break;

			if (i >= del->nreceipients)
				/* Stupid server wants message with no
				** receipients
				*/
			{
				esmtp_timeout(data_timeout);
				if (esmtp_writestr(".\r\n") || esmtp_writeflush())
					return (-1);
				do
				{
					p=esmtp_readline();
					if (!p)	return (-1);
				} while (!ISFINALLINE(p));
				return (-1);
			}
		}
		return (0);
	}

	/* DATA error */

	if (hasexdata && isfinal && *p == '5' && p[1] == '5'  && p[2] == '8')
		return (parseexdatareply(p, del, ctf, rcptok));
		/* Special logic for EXDATA extended replies */

	/* Fail the recipients that haven't been failed already */

	for (i=0; i<del->nreceipients; i++)
	{
		if (!rcptok[i])	continue;
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
			COMCTLFILE_DELINFO_SENT, "DATA");

		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
			COMCTLFILE_DELINFO_REPLYTYPE, "smtp");
	}

	for (;;)
	{
		if (line_num < 10)
		{
			for (i=0; i<del->nreceipients; i++)
			{
				if (!rcptok[i])	continue;

				ctlfile_append_connectioninfo(ctf,
					(unsigned)atol(
						del->receipients[i*2]),
					COMCTLFILE_DELINFO_REPLY, p);
			}
			++line_num;
		}
		if (ISFINALLINE(p))
		{
			for (i=0; i<del->nreceipients; i++)
			{
				if (!rcptok[i])	continue;

				if (SMTPREPLY_TYPE(p) == COMCTLFILE_DELFAIL)
					hard_error1(del, ctf, "", i);
				else
					soft_error1(del, ctf, "", i);
				rcptok[i]=0;
			}
			break;
		}
		if ((p=esmtp_readline()) == 0)
			return (-1);
	}
	return (-1);
}

/*
** Parse EXDATA 558 reply.
**
** See draft-varshavchik-exdata-smtpext.txt for more information.
*/

static int parseexdatareply(const char *p,
	struct moduledel *del, struct ctlfile *ctf, int *rcptok)
{
unsigned i;
char err_code=0;
unsigned line_num=0;

	for (i=0; i<del->nreceipients; i++)
		if (rcptok[i])	break;

	/* i is the next recipient that's getting an extended reply */

	for (;;p=esmtp_readline())
	{
		if (!p)	return (-1);

		if (!isdigit((int)(unsigned char)*p) ||
			!isdigit((int)(unsigned char)p[1]) ||
			!isdigit((int)(unsigned char)p[2]) || !p[3])
			continue;

		if (line_num == 0)
			err_code=p[4];

		if (i >= del->nreceipients)	/* Bad extended reply */
		{
			if (ISFINALLINE(p))	break;
			continue;
		}

		if (line_num == 0 &&
			SMTPREPLY_TYPE(&err_code) != COMCTLFILE_DELSUCCESS)
		{
			ctlfile_append_connectioninfo( ctf,
				(unsigned)atol(del->receipients[i*2]),
				COMCTLFILE_DELINFO_SENT, "DATA");
		}

		if (line_num == 0)
			ctlfile_append_connectioninfo( ctf,
				(unsigned)atol(del->receipients[i*2]),
				COMCTLFILE_DELINFO_REPLYTYPE, "smtp");


		if (line_num < 10)
		{
			ctlfile_append_connectioninfo(ctf,
				(unsigned)atol(del->receipients[i*2]),
				COMCTLFILE_DELINFO_REPLY, p+4);
			++line_num;
		}

		if (ISFINALLINE((p+4)))
		{
			switch (SMTPREPLY_TYPE( &err_code))	{
			case COMCTLFILE_DELFAIL:
				hard_error1(del, ctf, "", i);
				rcptok[i]=0;
				break;
			case COMCTLFILE_DELSUCCESS:
				{
				char	*p=logsuccessto();

				ctlfile_append_reply(ctf,
					(unsigned)atol( del->receipients[i*2]),
					p, COMCTLFILE_DELSUCCESS_NOLOG,
					(hasdsn ? "":" r"));

					free(p);
				}
				break;
			default:
				soft_error1(del, ctf, "", i);
				rcptok[i]=0;
				break;
			}

			/* Find next recipient that gets an extended reply */

			while (i < del->nreceipients &&
				!rcptok[++i])
				;
			line_num=0;
		}
		if (ISFINALLINE(p))	break;
	}

	while (i < del->nreceipients)
		if (rcptok[++i])
		{
			hard_error1(del, ctf,
				"Invalid 558 response from server.", i);
			rcptok[i]=0;
		}

	return (0);
}

static void call_rewrite_func(struct rw_info *p, void (*f)(struct rw_info *),
		void *arg)
{
	(*rewrite_func)(p, f);
}

/* Write out .\r\n, then wait for the DATA reply */

static int data_wait(struct moduledel *del, struct ctlfile *ctf, int *rcptok)
{
	esmtp_timeout(data_timeout);
	if (esmtp_dowrite(".\r\n", 3) || esmtp_writeflush())	return (-1);

	cork(0);

	(void)parsedatareply(del, ctf, rcptok, 0, 0, 1);

	if (esmtp_sockfd < 0)	return (-1);
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
	} ;

static int convert_to_crlf(const char *, unsigned, void *);

static void pushdsn(struct moduledel *del, struct ctlfile *ctf)
{
unsigned i;
int	*rcptok;
int	fd;
char	*mailfroms;
struct rfc2045 *rfcp=0;
int	is8bitmsg;

	if ((fd=open(qmsgsdatname(del->inum), O_RDONLY)) < 0)
	{
		connect_error1(del, ctf, -1);
		return;
	}

	is8bitmsg=ctlfile_searchfirst(ctf, COMCTLFILE_8BIT) >= 0;
	if ((mailfroms=mailfrom(del, ctf, fd, is8bitmsg)) == 0)
	{
		sox_close(fd);
		return;
	}

	talking(del, ctf);
	esmtp_timeout(cmd_timeout);

	if (esmtp_writestr(mailfroms) || esmtp_writeflush())
	{
		connect_error(del, ctf);
		sox_close(fd);
		free(mailfroms);
		quit();
		return;
	}

	/*
	** While waiting for MAIL FROM to come back, check if the message
	** needs to be converted to quoted-printable.
	*/

	if (!has8bitmime && is8bitmsg)
	{
		rfcp=rfc2045_alloc_ac();
		if (!rfcp)	clog_msg_errno();
		parserfc(fd, rfcp);

		rfc2045_ac_check(rfcp, RFC2045_RW_7BIT);
	}

	if (smtpreply(mailfroms, del, ctf, 1))	/* MAIL FROM rejected */
	{
		if (rfcp)	rfc2045_free(rfcp);
		sox_close(fd);

		free(mailfroms);
		return;
	}
	free(mailfroms);

	rcptok=courier_malloc(sizeof(int)*(del->nreceipients+1));

	if ( do_pipeline_rcpt(del, ctf, rcptok) )
	{
		if (rfcp)	rfc2045_free(rfcp);
		free(rcptok);
		sox_close(fd);
		return;
	}

	{
	struct rw_for_esmtp rfe;

		rfe.is_sol=1;
		rfe.byte_counter=0;

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
			|| data_wait(del, ctf, rcptok))
			for (i=0; i<del->nreceipients; i++)
				if (rcptok[i])
					connect_error1(del, ctf, i);
		free(rcptok);
		sox_close(fd);
		cork(0);
	}
	if (rfcp)	rfc2045_free(rfcp);
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

	if ( (ptr->byte_counter += l) >= BUFSIZ)
		ptr->byte_counter=0;

	if (ptr->byte_counter == 0)
		esmtp_timeout(data_timeout);

	for (i=j=0; i<l; i++)
	{
		if (ptr->is_sol && msg[i] == '.')
		{
			if ((rc=esmtp_dowrite(msg+j, i-j)) != 0 ||
				(rc=esmtp_dowrite(".", 1)) != 0)
				return (rc);
			j=i;
		}
		ptr->is_sol= msg[i] == '\n' ? 1:0;
	}

	return (esmtp_dowrite(msg+j, i-j));
}

/****************************************************************************/
/* Authenticated ESMTP client                                               */
/****************************************************************************/

static const char *start_esmtpauth(const char *, const char *, void *);
static const char *esmtpauth(const char *, void *);
static int final_esmtpauth(const char *, void *);
static int plain_esmtpauth(const char *, const char *, void *);

struct esmtpauthinfo {
	struct moduledel *del;
	struct ctlfile *ctf;
	int error_sent;
	} ;

static int authclient(struct moduledel *del, struct ctlfile *ctf,
		      const char *auth_key)
{
FILE	*configfile;
char	uidpwbuf[256];
char	*q;
const char *p;
struct authsaslclientinfo info;
struct esmtpauthinfo xinfo;
int	rc;

	q=config_localfilename("esmtpauthclient");
	configfile=fopen( q, "r");
	free(q);

	if (!configfile)	return (0);

	xinfo.del=del;
	xinfo.ctf=ctf;
	xinfo.error_sent=0;

	for (;;)
	{
		if (fgets(uidpwbuf, sizeof(uidpwbuf), configfile) == 0)
		{
			fclose(configfile);
			return (0);
		}
		q=strtok(uidpwbuf, " \t\r\n");

		if (!sockfdaddrname || !q)	continue;

#if	HAVE_STRCASECMP
		if (strcasecmp(q, auth_key) == 0)
			break;
#else
		if (stricmp(q, auth_key) == 0)
			break;
#endif
	}
	fclose(configfile);
	memset(&info, 0, sizeof(info));
	info.userid=strtok(0, " \t\r\n");
	info.password=strtok(0, " \t\r\n");

	info.sasl_funcs=authsasllist;

	info.start_conv_func= &start_esmtpauth;
	info.conv_func= &esmtpauth;
	info.final_conv_func= &final_esmtpauth;
	info.plain_conv_func= &plain_esmtpauth;
	info.conv_func_arg= &xinfo;

	rc=auth_sasl_client(&info);
	if (rc == AUTHSASL_NOMETHODS)
	{
		talking(del, ctf);
		hard_error(del, ctf,
			"Compatible SASL authentication not available.");
		return (-1);
	}

	if (rc && !xinfo.error_sent)
	{
		talking(del, ctf);
		soft_error(del, ctf, "Temporary SASL authentication error.");
	}

	if (rc)
		return (-1);

	if ((p=esmtp_readline()) == 0)
	{
		talking(del, ctf);
		connect_error(del, ctf);
		return (-1);
	}

	if (*p != '1' && *p != '2' && *p != '3') /* Some kind of an error */
	{
		talking(del, ctf);
		sent(del, ctf, "AUTH");
		smtp_msg(del, ctf);
		while (!ISFINALLINE(p))
		{
			reply(del, ctf, p);
			if ((p=esmtp_readline()) == 0)
			{
				connection_closed(del, ctf);
				return (-1);
			}
		}
		smtp_error(del, ctf, p, 0);
		quit();
		return (-1);
	}

	return (0);
}

static const char *getresp(struct esmtpauthinfo *x)
{
const char *p=esmtp_readline();

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
	x->error_sent=1;
	talking(x->del, x->ctf);
	sent(x->del, x->ctf, "AUTH");
	smtp_msg(x->del, x->ctf);

	if (!p)
	{
		connection_closed(x->del, x->ctf);
		return (0);
	}

	while (!ISFINALLINE(p))
	{
		reply(x->del, x->ctf, p);
		if ((p=esmtp_readline()) == 0)
		{
			connection_closed(x->del, x->ctf);
			return (0);
		}
	}
	smtp_error(x->del, x->ctf, p, 0);
	return (0);
}

static const char *start_esmtpauth(const char *method, const char *arg,
	void *voidp)
{
	if (arg && !*arg)
		arg="=";

	if (esmtp_writestr("AUTH ") || esmtp_writestr(method) ||
			(arg && (esmtp_writestr(" ") || esmtp_writestr(arg))) ||
			esmtp_writestr("\r\n") ||
		esmtp_writeflush())
		return (0);

	return (getresp((struct esmtpauthinfo *)voidp));
}

static const char *esmtpauth(const char *msg, void *voidp)
{
	if (esmtp_writestr(msg) || esmtp_writestr("\r\n") || esmtp_writeflush())
		return (0);
	return (getresp((struct esmtpauthinfo *)voidp));
}

static int final_esmtpauth(const char *msg, void *voidp)
{
	if (esmtp_writestr(msg) || esmtp_writestr("\r\n") || esmtp_writeflush())
		return (AUTHSASL_CANCELLED);
	return (0);
}

static int plain_esmtpauth(const char *method, const char *arg,
	void *voidp)
{
	if (arg && !*arg)
		arg="=";

	if (esmtp_writestr("AUTH ") || esmtp_writestr(method) ||
			(arg && (esmtp_writestr(" ") || esmtp_writestr(arg))) ||
			esmtp_writestr("\r\n") ||
		esmtp_writeflush())
		return (AUTHSASL_CANCELLED);

	return (0);
}
