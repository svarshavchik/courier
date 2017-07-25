/*
** Copyright 1998 - 2017 Double Precision, Inc.
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

#define WANT_SECURITY(info) ((info)->smtproutes_flags & ROUTE_STARTTLS)

struct my_esmtp_info {
	struct moduledel *del;
	struct ctlfile *ctf;
	int log_line_num;
	int prev_rcpt_num;
};

extern struct rw_list *esmtp_rw_install(const struct rw_install_info *);
extern int isloopback(const char *);

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

	if (chdir(courierdir()))
		clog_msg_errno();

	/* Retrieve delivery request until courieresmtp closes the pipe */

	while ((del=module_getdel()) != 0)
	{
		fd_set	fdr;
		struct	timeval	tv;

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
		my_info.prev_rcpt_num=0;

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
			{
				info=libesmtp_init(del->host);
				info->rewrite_func=
					rw_search_transport("esmtp")->rw_ptr
					->rewrite;
			}

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

			if (esmtp_ping(info))
				break;
		}
	}

	if (info && esmtp_connected(info))
		esmtp_quit(info, &my_info);

	if (info)
		libesmtp_deinit(info);
}

static void connect_error1(struct moduledel *del, struct ctlfile *ctf, int n);

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
	}

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
		esmtp_ping(info);

	if (!esmtp_connected(info) && info->net_timeout)
	{
	time_t	t;

		time (&t);
		if (t < info->net_timeout)
		{
			errno=info->net_error;
			if (!errno)	errno=ENETDOWN;
			connect_error1(del, ctf, -1);
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

			if (!esmtp_connected(info))
			{
				connect_error1(del, ctf, i);
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
	if (errcode == '5')
		hard_error1(del, ctf, msg, -1);
	else
		soft_error1(del, ctf, msg, -1);
}

static void talking2(struct esmtp_info *info,
		     struct moduledel *del, struct ctlfile *ctf, int n)
{
	char	buf[RFC1035_NTOABUFSIZE];
	unsigned i;
	char	*p;

	esmtp_sockipname(info, buf);
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

/* Log reply received */

static void smtp_msg(struct moduledel *del, struct ctlfile *ctf,
		     int rcpt_num)
{
	unsigned	i;

	if (rcpt_num >= 0 && rcpt_num < del->nreceipients)
	{
		ctlfile_append_connectioninfo
			(ctf,
			 (unsigned)atol(del->receipients[rcpt_num*2]),
			 COMCTLFILE_DELINFO_REPLYTYPE, "smtp");
		return;
	}

	for (i=0; i<del->nreceipients; i++)
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
			COMCTLFILE_DELINFO_REPLYTYPE, "smtp");
}

static void reply(struct moduledel *del, struct ctlfile *ctf, const char *msg,
		  int rcpt_num)
{
	unsigned        i;

	if (rcpt_num >= 0 && rcpt_num < del->nreceipients)
	{
		ctlfile_append_connectioninfo
			(ctf,
			 (unsigned)atol(del->receipients[rcpt_num*2]),
			 COMCTLFILE_DELINFO_REPLY, msg);
		return;
	}

	for (i=0; i<del->nreceipients; i++)
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[i*2]),
			COMCTLFILE_DELINFO_REPLY, msg);
}

/* Log the command sent to remote server */

static void sent(struct moduledel *del, struct ctlfile *ctf, const char *msg,
		 int rcpt_num)
{
	unsigned        i;

	if (rcpt_num >= 0 && rcpt_num < del->nreceipients)
	{
		ctlfile_append_connectioninfo(ctf,
			(unsigned)atol(del->receipients[rcpt_num*2]),
				COMCTLFILE_DELINFO_SENT, msg);
		return;
	}

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

static void log_sent(struct esmtp_info *ingo, const char *str,
		     int rcpt_num,
		     void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	sent(my_info->del, my_info->ctf, str, rcpt_num);
	smtp_msg(my_info->del, my_info->ctf, rcpt_num);
}

static void log_reply(struct esmtp_info *info, const char *str,
		      int rcpt_num, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	if (rcpt_num != my_info->prev_rcpt_num)
		my_info->log_line_num=0;
	my_info->prev_rcpt_num=rcpt_num;

	if (my_info->log_line_num > 10)
		return; // Log only the first ten lines of a response.
	++my_info->log_line_num;
	reply(my_info->del, my_info->ctf, str, rcpt_num);
}

static void log_smtp_error(struct esmtp_info *info,
			   const char *msg,
			   int errcode,
			   void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	smtp_error(my_info->del, my_info->ctf, msg, errcode);
}

static void log_rcpt_error(struct esmtp_info *info, int n, int errcode,
			   void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	if (SMTPREPLY_TYPE(&errcode) == COMCTLFILE_DELFAIL)
	{
		hard_error1(my_info->del, my_info->ctf, 0, n);
	}
	else
	{
		soft_error1(my_info->del, my_info->ctf, 0, n);
	}
}

static void log_net_error(struct esmtp_info *info, int rcpt_num, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;

	connect_error1(my_info->del, my_info->ctf, rcpt_num);
}

static void log_success(struct esmtp_info *info,
			unsigned rcpt_num,
			const char *msg,
			int has_dsn, void *arg)
{
	struct my_esmtp_info *my_info=(struct my_esmtp_info *)arg;


	ctlfile_append_reply(my_info->ctf,
			     (unsigned)atol( my_info->del->receipients
					     [rcpt_num*2]),
			     msg, COMCTLFILE_DELSUCCESS_NOLOG,
			     (has_dsn ? "":" r"));
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
	int smtproutes_flags=0;
	char *smtproute=smtproutes(host,
				   &smtproutes_flags);
	char *q;
	struct esmtp_info *info=esmtp_info_alloc(host, smtproute,
						 smtproutes_flags);
	if (smtproute)
		free(smtproute);

	q=config_localfilename("esmtpauthclient");
	esmtp_setauthclientfile(info, q);
	free(q);

	info->helohost=config_esmtphelo();

	info->log_talking= &log_talking;
	info->log_sent= &log_sent;
	info->log_reply= &log_reply;
	info->log_smtp_error= &log_smtp_error;
	info->log_rcpt_error= &log_rcpt_error;
	info->log_net_error= &log_net_error;
	info->log_success= &log_success;
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
** Construct the RCPT TO information.
*/

static struct esmtp_rcpt_info *mk_rcpt_info(struct moduledel *del,
					    struct ctlfile *ctf)
{
	unsigned n=del->nreceipients;
	unsigned i;

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

	return p;
}

static void pushdsn(struct esmtp_info *info, struct my_esmtp_info *my_info)
{
	struct moduledel *del=my_info->del;
	struct ctlfile *ctf=my_info->ctf;
	struct esmtp_rcpt_info *rcpt_info;

	int fd;
	struct esmtp_mailfrom_info mf_info;

	if ((fd=open(qmsgsdatname(del->inum), O_RDONLY)) < 0)
	{
		connect_error1(del, ctf, -1);
		return;
	}

	memset(&mf_info, 0, sizeof(mf_info));

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

	mf_info.sender=del->sender;

	rcpt_info=mk_rcpt_info(del, ctf);

	esmtp_send(info, &mf_info, rcpt_info, del->nreceipients, fd,
		   my_info);

	free(rcpt_info);
	close(fd);
}
