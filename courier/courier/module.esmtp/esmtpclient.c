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

#ifdef	TCP_CORK

static int esmtp_cork;
static int corked;

#define	cork(n) \
	{\
	int flag=(n);\
\
		if (esmtp_cork && esmtp_connected(info) && corked != flag) \
		{ \
			setsockopt(info->esmtp_sockfd, SOL_TCP, TCP_CORK, &flag, \
							sizeof(flag));\
		} \
		corked=flag;\
	}
#else
#define	cork(n)
#endif

#define WANT_SECURITY(info) ((info)->smtproutes_flags & ROUTE_STARTTLS)

struct my_esmtp_info {
	struct moduledel *del;
	struct ctlfile *ctf;
	int log_line_num;
};

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

static struct esmtp_info *libesmtp_init(const char *host);

static void libesmtp_deinit(struct esmtp_info *info);

static void sendesmtp(struct esmtp_info *, struct my_esmtp_info *);

/* We get here as a new child process of the courieresmtp driver */

void esmtpchild(unsigned childnum)
{
	struct	moduledel *del;
	struct	ctlfile	ctf;
	unsigned long mypid=(unsigned long)getpid();
	struct my_esmtp_info my_info;
	struct esmtp_info *info=0;

	signal(SIGPIPE, SIG_IGN);
	srand(time(NULL));
	rw_init_courier("esmtp");
	rewrite_func=rw_search_transport("esmtp")->rw_ptr->rewrite;

	if (chdir(courierdir()))
		clog_msg_errno();

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
		my_info.del=del;
		my_info.ctf=&ctf;
		my_info.log_line_num=0;

		/*
		** Open the message control file, send the message, close
		** the control file, we're done.
		*/

		if (ctlfile_openi(del->inum, &ctf, 0) == 0)
		{
			int changed_vhosts=ctlfile_setvhost(&ctf);
			const char *sec;

			if (changed_vhosts)
			{
				if (info)
				{
					esmtp_quit(info, &my_info);
					libesmtp_deinit(info);
					info=NULL;
				}
			}

			if (!info)
				info=libesmtp_init(del->host);

			info->net_error=0;

			sec=ctlfile_security(&ctf);

			if (sec && strcmp(sec, "STARTTLS") == 0)
				info->smtproutes_flags |= ROUTE_STARTTLS;

			sendesmtp(info, &my_info);
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

		while (info && info->esmtpkeepaliveping &&
		       esmtp_connected(info))
		{
			FD_ZERO(&fdr);
			FD_SET(0, &fdr);
			tv.tv_sec=info->esmtpkeepaliveping;
			tv.tv_usec=0;

			if ( sox_select(1, &fdr, 0, 0, &tv) > 0)
				break;
			esmtp_timeout(info, info->data_timeout);
			if (esmtp_writestr(info, "RSET\r\n") ||
			    esmtp_writeflush(info))
				break;

			while ( (p=esmtp_readline(info)) != 0 && !ISFINALLINE(p))
				;

			if (p == 0)
			{
				esmtp_quit(info, &my_info);
				break;
			}
		}
	}
	if (info && esmtp_connected(info))
		esmtp_quit(info, &my_info);

	if (info)
		libesmtp_deinit(info);
}

static void connect_error(struct moduledel *, struct ctlfile *);

static int rset(struct esmtp_info *, struct my_esmtp_info *);
static void push(struct esmtp_info *, struct my_esmtp_info *);

static int get_sourceaddr(struct esmtp_info *info,
			  const RFC1035_ADDR *dest_addr,
			  RFC1035_ADDR *source_addr,
			  void *arg)
{
	int rc;
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
		rc = rfc1035_aton(buf, source_addr);
		if (rc != 0)
		{
			errno=EINVAL;
			return rc;
		}
	} else
		*source_addr = RFC1035_ADDRANY;

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

static void talking2(struct esmtp_info *info,
		     struct moduledel *del, struct ctlfile *ctf, int n);

static void talking(struct esmtp_info *info,
		    struct moduledel *del, struct ctlfile *ctf)
{
	talking2(info, del, ctf, -1);
}

static void smtp_error(struct moduledel *del, struct ctlfile *ctf,
		       const char *msg, int errcode);

/* Attempt to deliver a message */

static void sendesmtp(struct esmtp_info *info, struct my_esmtp_info *my_info)
{
	struct moduledel *del=my_info->del;
	struct ctlfile *ctf=my_info->ctf;
	int cn;

	/* Sanity check */

	if (strcmp(info->host, del->host))
	{
		clog_msg_start_err();
		clog_msg_str("Internal failure in courieresmtp - daemon mixup.");
		clog_msg_send();
		_exit(1);
	}

	/* If we're connected, send a RSET to make sure the socket is working */

	if (esmtp_connected(info))
	{
		esmtp_timeout(info, info->helo_timeout);
		if (esmtp_writestr(info, "RSET\r\n") == 0 &&
		    esmtp_writeflush(info) == 0)
		{
			if (esmtp_parsereply(info, "RSET", my_info))
			{
				esmtp_quit(info, my_info);
				return;
			}
		}
	}

	if (!esmtp_connected(info) && info->net_timeout)
	{
	time_t	t;

		time (&t);
		if (t < info->net_timeout)
		{
			errno=info->net_error;
			if (!errno)	errno=ENETDOWN;
			connect_error(del, ctf);
			return;
		}
		info->net_timeout=0;
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
					     (info->hasdsn ? "":" r"));
		}
		return;
	}

	/*
	** If the message wants a secured connection, and the current
	** connection has not been secured, close it, so it can be reopened.
	*/

	if (esmtp_connected(info) && WANT_SECURITY(info) && !info->is_secure_connection)
		esmtp_quit(info, my_info);


	if (esmtp_connect(info, my_info))
		return;

	/*
	** Ok, we now have a connection.  We want to call push() to deliver
	** this message, but if the VERP flag is set but the remote server
	** does not grok VERPs, we need to do a song-n-dance routine.
	*/

	if (info->hasverp || ctlfile_searchfirst(ctf, COMCTLFILE_VERP) < 0)
	{
		push(info, my_info);	/* ... but not this time */
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

			if (i && esmtp_connected(info))	/* Call RSET in between */
			{
				if (rset(info, my_info))
				{
					esmtp_quit(info, my_info);
					continue;
				}
			}
			if (!esmtp_connected(info))
			{
				connect_error(del, ctf);
				continue;
			}

			verp_sender=verp_getsender(ctf, del->receipients[1]);

			del->sender=verp_sender;
			push(info, my_info);
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

static void sockipname(struct esmtp_info *info, char *buf)
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

static void talking2(struct esmtp_info *info,
		     struct moduledel *del, struct ctlfile *ctf, int n)
{
	char	buf[RFC1035_NTOABUFSIZE];
	unsigned i;
	char	*p;

	sockipname(info, buf);
	p=courier_malloc(strlen(info->sockfdaddrname ?
				info->sockfdaddrname:"")+strlen(buf)+
		sizeof(" []"));
	strcat(strcat(strcat(strcpy(p,
				    info->sockfdaddrname ?
				    info->sockfdaddrname:""), " ["),
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

/* Try an EHLO */

static void log_talking(struct esmtp_info *info, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	talking(info, my_info->del, my_info->ctf);
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

	if (my_info->log_line_num > 10)
		return; // Log only the first ten lines of a response.
	++my_info->log_line_num;
	reply(my_info->del, my_info->ctf, str);
}

static void log_smtp_error(struct esmtp_info *info,
			   const char *msg, int errcode, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	smtp_error(my_info->del, my_info->ctf, msg, errcode);
}


static int lookup_broken_starttls(struct esmtp_info *info,
				  const char *hostname,
				  void *arg)
{
	time_t timestamp;

	return track_find_broken_starttls(hostname, &timestamp);
}

static void do_report_broken_starttls(struct esmtp_info *info,
				      const char *hostname,
				      void *arg)
{
	track_save_broken_starttls(hostname);
}

static int is_local_or_loopback(struct esmtp_info *info,
				const char *hostname,
				const char *ipaddr,
				void *arg)
{
	return config_islocal(hostname, 0) || isloopback(ipaddr);
}

static struct esmtp_info *libesmtp_init(const char *host)
{
	struct esmtp_info *info=esmtp_info_alloc(host);

	info->log_talking= &log_talking;
	info->log_sent= &log_sent;
	info->log_reply= &log_reply;
	info->log_smtp_error= &log_smtp_error;
	info->lookup_broken_starttls= &lookup_broken_starttls;
	info->report_broken_starttls= &do_report_broken_starttls;
	info->get_sourceaddr= &get_sourceaddr;
	info->is_local_or_loopback= &is_local_or_loopback;


	info->esmtpkeepaliveping=config_time_esmtpkeepaliveping();
	info->quit_timeout=config_time_esmtpquit();
	info->connect_timeout=config_time_esmtpconnect();
	info->helo_timeout=config_time_esmtphelo();
	info->data_timeout=config_time_esmtpdata();
	info->cmd_timeout=config_time_esmtptimeout();
	info->delay_timeout=config_time_esmtpdelay();

	return info;
}

static void libesmtp_deinit(struct esmtp_info *info)
{
	esmtp_info_free(info);
}

/*
	Try EHLO then HELO, and see what the other server says.
*/

/* Send an SMTP command that applies to all recipients, then wait for a reply */

static int smtpcommand(struct esmtp_info *info,
		       struct my_esmtp_info *my_info,
		       const char *cmd)
{
	if (esmtp_sendcommand(info, cmd, my_info))
		return -1;

	return esmtp_parsereply(info, cmd, my_info);
}


static int rset(struct esmtp_info *info, struct my_esmtp_info *my_info)
{
	esmtp_timeout(info, info->helo_timeout);
	return (smtpcommand(info, my_info, "RSET\r\n"));
}

static void pushdsn(struct esmtp_info *, struct my_esmtp_info *);

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

static void push(struct esmtp_info *info, struct my_esmtp_info *my_info)
{
	struct moduledel *del=my_info->del;
	struct ctlfile *ctf=my_info->ctf;

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

	if (info->hasdsn || real_sender == 0 || *real_sender == '\0')
	{
		pushdsn(info, my_info);
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
		pushdsn(info, my_info);
	}
	free(del->receipients);
	del->receipients=real_receipients;
	del->nreceipients=real_nreceipients;
	del->sender=real_sender;
}

/*
** Construct the RCPT TO command along the same lines.
*/

static char *mk_rcpt_data(struct esmtp_info *info,
			  struct moduledel *del,
			  struct ctlfile *ctf)
{
	unsigned n=del->nreceipients;
	unsigned i;
	char *buf;

	struct esmtp_rcpt_info *p=malloc(n*sizeof(struct esmtp_rcpt_info));

	if (!p)
		abort();

	memset(p, 0, n*sizeof(*p));

	for (i=0; i<n; ++i)
	{
		unsigned n=atol(del->receipients[i*2]);

		p[i].address=del->receipients[i*2+1];
		p[i].dsn_options=ctf->dsnreceipients[n];
		p[i].orig_receipient=ctf->oreceipients[n];
	}

	buf=esmtp_rcpt_data_create(info, p, n);

	free(p);

	return buf;
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
			  struct my_esmtp_info *my_info,
			  int *, char **, size_t *, int);

static int do_pipeline_rcpt_2(struct esmtp_info *info,
			    struct my_esmtp_info *my_info,
			    int *rcptok,
			    char *rcpt_data_cmd);

static int do_pipeline_rcpt(struct esmtp_info *info,
			    struct my_esmtp_info *my_info,
			    int *rcptok)
{
	char *buf=mk_rcpt_data(info, my_info->del, my_info->ctf);

	int rc=do_pipeline_rcpt_2(info, my_info, rcptok, buf);

	free(buf);
	return rc;
}

static int do_pipeline_rcpt_2(struct esmtp_info *info,
			      struct my_esmtp_info *my_info,
			      int *rcptok,
			      char *rcpt_data_cmd)
{
	const char *p;
	struct moduledel *del=my_info->del;
	struct ctlfile *ctf=my_info->ctf;
	size_t l=strlen(rcpt_data_cmd);
	size_t i;
	int	rc=0;

	char	*orig_rcpt_data_cmd=rcpt_data_cmd;

	if (info->haspipelining)	/* One timeout */
		esmtp_timeout(info, info->cmd_timeout);

	/* Read replies for the RCPT TO commands */

	for (i=0; i<del->nreceipients; i++)
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

			if ( SMTPREPLY_TYPE(&err_code) ==
				COMCTLFILE_DELSUCCESS)
				continue;

			if (line_num >= 10)	continue;
			/* Ignore SMTP replies longer than 10 lines */

			if (line_num == 0)
			{
				char save=this_rcpt_to_cmd[this_rcpt_to_len];
				// Should be the \r

				this_rcpt_to_cmd[this_rcpt_to_len]=0;
				ctlfile_append_connectioninfo(ctf,
					(unsigned)atol(del->receipients[i*2]),
					COMCTLFILE_DELINFO_SENT,
					this_rcpt_to_cmd);
				this_rcpt_to_cmd[this_rcpt_to_len]=save;
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

		if (info->haspipelining)
		{
			l=write_cnt;
		}
		else
		{
			rcpt_data_cmd=next_rcpt_data_cmd;
			l=next_cnt;
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

	if (esmtp_connected(info))
	{
		if (!info->haspipelining)	/* DATA hasn't been sent yet */
		{
			for (i=0; i<del->nreceipients; i++)
				if (rcptok[i])	break;

			if (i >= del->nreceipients)	return (-1);
					/* All RCPT TOs failed */
		}
		rc=parsedatareply(info, my_info, rcptok, &rcpt_data_cmd, &l, 0);
			/* One more reply */
	}

	if (!esmtp_connected(info))
	{
		for (i=0; i<del->nreceipients; i++)
		{
			if (!rcptok[i])	continue;
			connect_error1(del, ctf, i);
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

static int parseexdatareply(struct esmtp_info *, struct my_esmtp_info *,
			    const char *,
			    int *);

static char *logsuccessto(struct esmtp_info *info)
{
	char	buf[RFC1035_NTOABUFSIZE];
	char	*p;

	sockipname(info, buf);
	p=courier_malloc(sizeof("delivered:  []")+
		(info->sockfdaddrname ?
			strlen(info->sockfdaddrname):0)+strlen(buf));

	strcpy(p, "delivered: ");
	if (info->sockfdaddrname && *info->sockfdaddrname)
		strcat(strcat(p, info->sockfdaddrname), " ");
	strcat(p, "[");
	strcat(p, buf);
	strcat(p, "]");
	return (p);
}

static int parsedatareply(struct esmtp_info *info,
			  struct my_esmtp_info *my_info,
			  int *rcptok,
			  char **write_buf,
			  size_t *write_l,
			  int isfinal)
{
	struct moduledel *del=my_info->del;
	struct ctlfile *ctf=my_info->ctf;
	const char *p;
	unsigned line_num=0;
	unsigned i;

	p=readpipelinercpt(info, write_buf, write_l);

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

			for (i=0; i<del->nreceipients; i++)
			{
				if (!rcptok[i])	continue;

				ctlfile_append_reply(ctf,
					(unsigned)atol(del->receipients[i*2]),
					p, COMCTLFILE_DELSUCCESS_NOLOG,
					(info->hasdsn ? "":" r"));
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
		return (parseexdatareply(info, my_info, p, rcptok));
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
			    struct my_esmtp_info *my_info,
			    const char *p,
			    int *rcptok)
{
	struct moduledel *del=my_info->del;
	struct ctlfile *ctf=my_info->ctf;
	unsigned i;
	char err_code=0;
	unsigned line_num=0;

	for (i=0; i<del->nreceipients; i++)
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
					char	*p=logsuccessto(info);

				ctlfile_append_reply(ctf,
					(unsigned)atol( del->receipients[i*2]),
					p, COMCTLFILE_DELSUCCESS_NOLOG,
					(info->hasdsn ? "":" r"));

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

static int data_wait(struct esmtp_info *info, struct my_esmtp_info *my_info,
		     int *rcptok)
{
	esmtp_timeout(info, info->data_timeout);
	if (esmtp_dowrite(info, ".\r\n", 3) ||
	    esmtp_writeflush(info))	return (-1);

	cork(0);

	(void)parsedatareply(info, my_info, rcptok, 0, 0, 1);

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
	struct my_esmtp_info *my_info;
	} ;

static int convert_to_crlf(const char *, unsigned, void *);

static void pushdsn(struct esmtp_info *info, struct my_esmtp_info *my_info)
{
	struct moduledel *del=my_info->del;
	struct ctlfile *ctf=my_info->ctf;

	unsigned i;
	int	*rcptok;
	int	fd;
	char	*mailfroms;
	struct rfc2045 *rfcp=0;

	struct	stat stat_buf;
	struct esmtp_mailfrom_info mf_info;

	memset(&mf_info, 0, sizeof(mf_info));

	if ((fd=open(qmsgsdatname(del->inum), O_RDONLY)) < 0)
	{
		connect_error1(del, ctf, -1);
		return;
	}

	mf_info.is8bitmsg=ctlfile_searchfirst(ctf, COMCTLFILE_8BIT) >= 0;
	mf_info.verp=ctlfile_searchfirst(ctf, COMCTLFILE_VERP) >= 0;

	{
		int n=ctlfile_searchfirst(ctf, COMCTLFILE_DSNFORMAT);

		if (n >= 0)
		{
			if (strchr(ctf->lines[n]+1, 'F'))
				mf_info.dsn_format='F';
			else if (strchr(ctf->lines[n]+1, 'F'))
				mf_info.dsn_format='H';
		}
		n=ctlfile_searchfirst(ctf, COMCTLFILE_ENVID);

		if (n >= 0 && ctf->lines[n][1])
		{
			mf_info.envid=ctf->lines[n]+1;
		}
	}

	if (fstat(fd, &stat_buf) == 0)
		mf_info.msgsize=stat_buf.st_size;

	mf_info.sender=del->sender;

	mailfroms=esmtp_mailfrom_cmd(info, &mf_info);

	talking(info, del, ctf);
	esmtp_timeout(info, info->cmd_timeout);

	if (esmtp_sendcommand(info, mailfroms, my_info))
	{
		close(fd);
		free(mailfroms);
		return;
	}

	/*
	** While waiting for MAIL FROM to come back, check if the message
	** needs to be converted to quoted-printable.
	*/

	if (!info->has8bitmime && mf_info.is8bitmsg)
	{
		rfcp=rfc2045_alloc_ac();
		if (!rfcp)	clog_msg_errno();
		parserfc(fd, rfcp);

		rfc2045_ac_check(rfcp, RFC2045_RW_7BIT);
	}

	if (esmtp_parsereply(info, mailfroms, my_info))	/* MAIL FROM rejected */
	{
		if (rfcp)	rfc2045_free(rfcp);
		close(fd);

		free(mailfroms);
		return;
	}
	free(mailfroms);

	rcptok=courier_malloc(sizeof(int)*(del->nreceipients+1));

	if ( do_pipeline_rcpt(info, my_info, rcptok) )
	{
		if (rfcp)	rfc2045_free(rfcp);
		free(rcptok);
		close(fd);
		return;
	}

	{
	struct rw_for_esmtp rfe;

		rfe.is_sol=1;
		rfe.byte_counter=0;
		rfe.info=info;
		rfe.my_info=my_info;

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
		    || data_wait(info, my_info, rcptok))
			for (i=0; i<del->nreceipients; i++)
				if (rcptok[i])
					connect_error1(del, ctf, i);
		free(rcptok);
		close(fd);
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
