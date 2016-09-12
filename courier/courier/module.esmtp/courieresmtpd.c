/*
** Copyright 1998 - 2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<stdio.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/uio.h>
#include	<sys/time.h>
#include	<signal.h>
#include	<syslog.h>

#if	HAVE_LOCALE_H
#include	<locale.h>
#endif

#include	"courier.h"
#include	"rw.h"
#include	"comcargs.h"
#include	"comsubmitclient.h"
#include	"comreadtime.h"
#include	"rfc822/rfc822.h"
#include	"waitlib/waitlib.h"
#include	"esmtpiov.h"
#include	"esmtpconfig.h"
#include	"numlib/numlib.h"
#include	"tcpd/spipe.h"
#include	"tcpd/tlsclient.h"
#include	<courierauth.h>
#include	<courierauthsasl.h>


static char helobuf[256];
static char authuserbuf[256];
static char tlsbuf[128+NUMBUFSIZE];
static int extended;

static unsigned long sizelimit;
static int submit_started=0;
static int hasexdata;
static int hasverp;
static int hasstarttls;

static char *mailfroms=0;
extern time_t iovread_timeout;
extern time_t iovwrite_timeout;
static char *input_line="";
static void cancelsubmit();
static time_t data_timeout;

static const char *tcpremoteip, *tcpremotehost;

#define	INIT_TEERGRUBE	8
#define	MAX_TEERGRUBE	128

static time_t teergrube=INIT_TEERGRUBE;

extern const char *externalauth();

static const char *truncate_ipv6(const char *tcp)
{
	if (strncmp(tcp, "::ffff:", 7) == 0)
		tcp += 7;
	return tcp;
}

static const char *smtp_externalauth()
{
	const char *p;

	if ((p=getenv("ESMTP_TLS")) && atoi(p))
		return externalauth();

	return NULL;
}

static void tarpit()
{
	const char *p;
	if ((p=getenv("TARPIT")) && atoi(p))
	{
		sleep(teergrube);
		teergrube *= 2;
		if (teergrube > MAX_TEERGRUBE)
			teergrube=MAX_TEERGRUBE;
	}
}

void iov_logerror(const char *q, const char *p)
{
	const char *ident=getenv("TCPREMOTEINFO");

	if (strcmp(input_line, "QUIT") == 0)
		return;
	/* Do not log write errors for QUIT cmd -- broken clients */

	clog_msg_start_info();
	clog_msg_str("error,relay=");
	clog_msg_str(tcpremoteip);
	if (ident)
	{
		clog_msg_str(",ident=\"");
		clog_msg_str(ident);
		clog_msg_str("\"");
	}
	clog_msg_str(",msg=\"");
	if (q)
		clog_msg_str(q);
	clog_msg_str(p);
	clog_msg_str("\",cmd: ");
	clog_msg_str(input_line);
	clog_msg_send();
}

void addiovec_error(const char *p)
{
	addiovec(p, strlen(p));
	addiovec("\r\n", 2);

	iov_logerror(NULL, p);
	iovflush();

	if (*p == '5')
		tarpit();
}

static void showbanner()
{
const char *banner=config_esmtpgreeting();
const char *p;

	do
	{
		for (p=banner; *p; p++)
			if (*p == '\n')	break;
		addiovec((*p && p[1] ? "220-":"220 "), 4);
		addiovec(banner, p-banner);
		addiovec("\r\n", 2);
		if (*p)	p++;
		banner=p;
	} while (*banner);
}

static void ehlo(const char *heloname, int hastls, int tls)
{
static const char e[]=
COURIER_EXTENSIONS
EHLO_VERP_EXTENSION
EHLO_EXDATA_EXTENSION
EHLO_SECURITY_EXTENSION
"250-PIPELINING\r\n"
"250-8BITMIME\r\n"
"250-SIZE\r\n"
"250 DSN\r\n";
const char *me=config_me();
const char *p, *q;

	if (helobuf[0] == 0)
	{
		char *p;

		strncat(helobuf, heloname, sizeof(helobuf)-1);

		for (p=helobuf; *p; p++)
			if (isspace((int)(unsigned char)*p) ||
			    (int)(unsigned char)*p < ' ')
				*p='_';
	}

        putenv(strcat(strcpy(courier_malloc(sizeof("ESMTPHELO=") +
					    strlen(helobuf)),
			     "ESMTPHELO="), helobuf));

	if (!extended)
	{
		addiovec("250 ", 4);
		addiovec(me, strlen(me));
		addiovec(" Ok.\r\n", 6);
		return;
	}

	addiovec("250-", 4);
	addiovec(me, strlen(me));
	addiovec(" Ok.\r\n", 6);

	if (tls && (p=getenv("ESMTPAUTH_TLS")) != 0 && *p)
		;
	else
		p=getenv("ESMTPAUTH");

	if (!p)
		p="";

	q=smtp_externalauth() ? "EXTERNAL":"";

	if (*p || *q)
	{
		addiovec("250-AUTH ", 9);
		addiovec(p, strlen(p));
		if (*p && *q)
			addiovec(" ", 1);
		addiovec(q, strlen(q));
		addiovec("\r\n", 2);
#if 0
		/*
		** Netscape.
		*/

		addiovec("250-AUTH=", 9);
		addiovec(p, strlen(p));
		if (*p && *q)
			addiovec(" ", 1);
		addiovec(q, strlen(q));
		addiovec(" X-NETSCAPE-HAS-BUGS\r\n", 22);
#endif
	}

	if (hastls)
	{
		static const char starttls_msg[]=
			"250-STARTTLS\r\n";

		addiovec(starttls_msg, sizeof(starttls_msg)-1);
	}

	addiovec(e, sizeof(e)-1);
}

extern char **environ;

static void badfork()
{
	addiovec_error("432 Service temporarily unavailable.");
}

static void expnvrfy(const char *line, const char *cmd)
{
struct	rfc822t *t=rfc822t_alloc_new(line, NULL, NULL);
struct	rfc822a *a;
char	*addr;
char	*argv[5];
int	rc;

	if (!t)	clog_msg_errno();

	a=rfc822a_alloc(t);
	if (!a)	clog_msg_errno();

	if (a->naddrs != 1 || a->addrs[0].tokens == 0)
	{
		addiovec_error("502 EXPN syntax error.");
		rfc822a_free(a);
		rfc822t_free(t);
		return;
	}

	addr=rfc822_getaddr(a, 0);
	rfc822a_free(a);
	rfc822t_free(t);
	if (!addr)	clog_msg_errno();

	argv[0]="submit";
	argv[1]=strcat(strcpy(courier_malloc(strlen(cmd)+strlen(addr)+1),
		cmd), addr);
	free(addr);
	argv[2]="local";	/* Use the LOCAL rewrite module */
	argv[3]="unknown; unknown";
	argv[4]=0;
	if (submit_fork(argv, environ, submit_print_stdout))
	{
		badfork();
		free(argv[1]);
		return;
	}
	free(argv[1]);
	fclose(submit_to);
	rc=submit_readrcprintcrlf();
	(void)submit_wait();
	if (rc == 0)
		teergrube=INIT_TEERGRUBE;
	fclose(fromsubmit);
}

static void startsubmit(int tls)
{
	char	*argv[14];
	const char *ident;
	char	*identbuf=0;
	int	n, exid_ndx=0;
	const	char *host;
	char	*buf;
	char rfc3848_buf[32];

	if (submit_started)	return;
	if (helobuf[0] == '\0')	return;

	argv[0]="submit";
	argv[1]=getenv("RELAYCLIENT") ? "-src=authsmtp":"-src=smtp";
	n=2;

	if (authuserbuf[0])
	{
		char *p;

		static char authbuf[sizeof(authuserbuf)+sizeof("-auth=")];

		p=strchr(authuserbuf, ' ');
		if (p)
		{
			strcat(strcpy(authbuf, "-auth="), p+1);
			argv[n++]=authbuf;
		}
		exid_ndx=1;
	}

	strcpy(rfc3848_buf, "-rfc3848=");

	if (extended)
	{
		static char *exid[] =
			{"ESMTP",
			 "ESMTPA",
			 "ESMTPS",
			 "ESMTPSA"};

		if (tls) exid_ndx += 2;
		strcat(rfc3848_buf, exid[exid_ndx]);

	}
	else
	{
		strcat(rfc3848_buf, "SMTP");
	}
	argv[n++]=rfc3848_buf;

	argv[n++]="esmtp";

	host=tcpremotehost;

	if (!host)	host="";
	argv[n]=buf=courier_malloc(strlen(host)+strlen(tcpremoteip)+strlen(
		helobuf)+sizeof("dns;  ( [])"));

	strcat(strcat(strcpy(buf, "dns; "), helobuf), " (");
	if (*host)
		strcat(strcat(buf, host), " ");
	strcat(strcat(strcat(buf, "["), tcpremoteip), "])");

	++n;

	if ((ident=getenv("TCPREMOTEINFO")) == 0)
		ident="";

	if (*ident || authuserbuf[0] || tls)
	{
		argv[n]=identbuf=courier_malloc(sizeof("IDENT: , AUTH: , ")+
						strlen(tlsbuf)+
						strlen(ident)+
						strlen(authuserbuf));
		++n;
		*identbuf=0;
		if (*ident)
			strcat(strcat(identbuf, "IDENT: "), ident);
                if (authuserbuf[0])
                {
                        if (*identbuf)  strcat(identbuf, ", ");
                        strcat(strcat(identbuf, "AUTH: "), authuserbuf);
			putenv(strcat(
				 strcpy(
				   courier_malloc(sizeof("SENDERAUTH=") +
						  strlen(authuserbuf)),
				   "SENDERAUTH="), authuserbuf));
		}

		if (tls)
		{
			if (*identbuf)	strcat(identbuf, ", ");
			strcat(identbuf, tlsbuf);
		}
	}

	argv[n]=0;

	if (submit_fork(argv, environ, submit_print_stdout) == 0)
		submit_started=1;

	if (identbuf)	free(identbuf);
	free(buf);
}

static void cancelsubmit()
{
	if (submit_started)
	{
		fclose(submit_to);
		fclose(fromsubmit);
		(void)submit_wait();
		submit_started=0;
	}
}

static char *log_error_sender=0;
static char *log_error_to=0;

static void print_submit_log_fix(void);

static void set_submit_error(const char *r, int rl)
{
char	*p;

	if (log_error_sender)	free(log_error_sender);
	if (log_error_to)	free(log_error_to);

	log_error_sender=0;
	if (mailfroms && (log_error_sender=strdup(mailfroms)) == 0)
		clog_msg_errno();

	log_error_to=0;
	if (r && rl)
	{
		if ((log_error_to=malloc(rl+1)) == 0)
			clog_msg_errno();
		memcpy(log_error_to, r, rl);
		log_error_to[rl]=0;
	}

	if (log_error_sender && *log_error_sender)
		for (p=log_error_sender+1; p[1]; p++)
			if (*p == '<' || *p == '>' || *p == ':')
				*p=' ';

	if (log_error_to && *log_error_to)
		for (p=log_error_to+1; p[1]; p++)
			if (*p == '<' || *p == '>' || *p == ':')
				*p=' ';

	if (log_error_sender && *log_error_sender)
		submit_log_error_prefix(print_submit_log_fix);
	else
		submit_log_error_prefix(0);
}

static void print_submit_log_fix(void)
{
const char *p;

	clog_msg_str("error,relay=");
	clog_msg_str(tcpremoteip);
	if ((p=getenv("TCPREMOTEINFO")) != 0 && *p)
	{
	char	*q=strdup(p), *r;

		if (!q)	clog_msg_errno();
		for (r=q; *r; r++)
			if (*r == ',' || *r == ':')	*r=' ';
		clog_msg_str(",ident=");
		clog_msg_str(q);
		free(q);
	}

	if (log_error_sender)
	{
		clog_msg_str(",from=");
		clog_msg_str(log_error_sender);
	}
	if (log_error_to)
	{
		clog_msg_str(",to=");
		clog_msg_str(log_error_to);
	}
	clog_msg_str(": ");
}

/* Skip the E-mail address in the MAIL FROM:/RCPT TO: command */

static const char *skipaddress(const char **ptr)
{
const char *p;
int inquote=0;

	while ( **ptr && isspace((int)(unsigned char)**ptr) )
		++*ptr;

	if (**ptr != '<')	return (0);
	p= ++*ptr;

	while (*p)
	{
		if (*p == '>' && !inquote)	return (p);
		if (*p == '"')	inquote ^= 1;
		if (*p == '\\' && p[1])	++p;
		++p;
	}
	return (0);
}

/**************************************************************************/

/*                          MAIL FROM                                     */

/**************************************************************************/

static int domailfrom(const char *, const char *);

static int mailfrom(const char *p)
{
const char *q=skipaddress(&p);

	set_submit_error(0, 0);
	if (q)
	{
		/* Save <address> in mailfroms */

		if (mailfroms)	free(mailfroms);
		mailfroms=courier_malloc(q-p+3);
		memcpy(mailfroms, p-1, q-p+2);
		mailfroms[q-p+2]=0;
		set_submit_error(0, 0);
		return (domailfrom(p, q));
	}
	addiovec_error("554 Syntax error - your mail software violates RFC 821.");
	return (-1);
}

static int domailfrom(const char *p, const char *q)
{
const char *r, *s;
char	retformat=0;
const char *envid=0;
size_t	envidlen=0;
char	*buf;
int	rc;

	hasexdata=0;
	hasverp=0;
	hasstarttls=0;

	for (r=q+1; *r; r++)
	{
		if (isspace((int)(unsigned char)*r))	continue;
		for (s=r; *s && !isspace((int)(unsigned char)*s); s++)
			;
		if (s - r == 4 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "VERP", 4)
#else
			strnicmp(r, "VERP", 4)
#endif
			== 0)
		{
			hasverp=1;
		}
		else if (s - r == 17 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "SECURITY=STARTTLS", 17)
#else
			strnicmp(r, "SECURITY=STARTTLS", 17)
#endif
			== 0)
		{
			hasstarttls=1;
		}
		else if (s - r == 6 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "EXDATA", 6)
#else
			strnicmp(r, "EXDATA", 6)
#endif
			== 0)
		{
			hasexdata=1;
		}
		else if (s - r >= 4 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "RET=", 4)
#else
			strnicmp(r, "RET=", 4)
#endif
			== 0)
		{
			switch (r[4])	{
			case 'f':
			case 'F':
				retformat='F';
				break;
			case 'h':
			case 'H':
				retformat='H';
				break;
			}
		}
		else if (s - r >= 6 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "ENVID=", 6)
#else
			strnicmp(r, "ENVID=", 6)
#endif
			== 0)
		{
			envid=r+6;
			envidlen=s-envid;
		}
		else if (s - r >= 5 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "SIZE=", 5)
#else
			strnicmp(r, "SIZE=", 5)
#endif
			== 0)
		{
		unsigned long l=0, n;

			for (r += 5; *r >= '0' && *r <= '9'; r++)
			{
				if ( (n=l * 10) < l ||
					(n += *r-'0') < l ||
					(sizelimit && n > sizelimit))
				{
					addiovec_error("534 SIZE=Message too big.");
					return (-1);
				}
				l=n;
			}
		}
		r= s-1;
	}

	buf=courier_malloc(q-p+envidlen+80);
	if (q > p)
		memcpy(buf, p, q-p);
	strcpy(buf + (q-p), "\t");
	if (retformat)
	{
	char b[2];

		b[0]=retformat;
		b[1]=0;
		strcat(buf, b);
	}
	if (hasverp)
		strcat(buf, "V");

	if (hasstarttls)
	{
		strcat(buf, "S{STARTTLS}");
	}

	if (envidlen)
	{
	char	*t;

		strcat(buf, "\t");
		t=buf+strlen(buf);
		memcpy(t, envid, envidlen);
		t[envidlen]=0;
	}
	submit_write_message(buf);
	free(buf);
	rc=submit_readrcprintcrlf();
	if (rc)
	{
		iovflush();
	}
	return (rc);
}

/**************************************************************************/

/*                            RCPT TO                                     */

/**************************************************************************/

static int dorcptto(const char *, const char *);

static int rcptto(const char *p)
{
const char *q=skipaddress(&p);

	if ( q )
	{
		set_submit_error(p-1, q-p+2);
		return (dorcptto(p, q));
	}
	addiovec_error("554 Syntax error - your mail software violates RFC 821.");
	return (-1);
}

static int dorcptto2(const char *p, const char *q);
static int rcpttolocal(const char *p, const char *r, const char *q);

static int dorcptto(const char *p, const char *q)
{
	const char *r;
	const char *tcp=getenv("TCPLOCALIP");

	/* foo@[our.ip.address] -> foo@default */

	for (r=p; r != q; r++)
	{
		if (*r == '@' && r[1] == '[' && tcp)
		{

			if (strncasecmp(r+2, tcp, strlen(tcp)) == 0 &&
			    strncasecmp(r+2+strlen(tcp), "]>", 2) == 0)
			{
				return rcpttolocal(p, r+1, q);
			}

			if (strncmp(tcp, "::ffff:", 7) == 0)
			{
				tcp += 7;
				if (strncasecmp(r+2, tcp, strlen(tcp)) == 0 &&
				    strncasecmp(r+2+strlen(tcp), "]>", 2) == 0)
				{
					return rcpttolocal(p, r+1, q);
				}
			}
		}
	}

	return dorcptto2(p, q);
}

static int rcpttolocal(const char *p, const char *r, const char *q)
{
	const char *d=config_defaultdomain();
	char *buf=courier_malloc(r-p + strlen(d)+strlen(q)+1);
	int rc;

	memcpy(buf, p, r-p);
	strcpy(buf+(r-p), d);

	p=buf+strlen(buf);
	strcat(buf, q);

	rc=dorcptto2(buf, p);
	free(buf);
	return rc;
}

static int dorcptto2(const char *p, const char *q)
{
int	notifyn=0,notifyf=0,notifys=0,notifyd=0;
const char *orcpt=0;
int	orcptlen=0;
const char *r, *s;
char	*t, *buf;
const char *relayclient=getenv("RELAYCLIENT");
int	relayclientl= relayclient ? strlen(relayclient):0;
int	rc;

	for (r=q+1; *r; r++)
	{
		if (isspace((int)(unsigned char)*r))	continue;
		for (s=r; *s && !isspace((int)(unsigned char)*s); s++)
			;
		if (s - r > 7 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "NOTIFY=", 7)
#else
			strnicmp(r, "NOTIFY=", 7)
#endif
			== 0)
		{
			r += 7;
			while (r < s)
			{
				switch (toupper(*r))	{
				case 'N':
					notifyn=1;
					break;
				case 'S':
					notifys=1;
					break;
				case 'D':
					notifyd=1;
					break;
				case 'F':
					notifyf=1;
					break;
				}
				while (r < s)
					if (*r++ == ',')	break;
			}


			hasverp=1;
		}
		else if (s - r > 6 &&
#if HAVE_STRNCASECMP
			strncasecmp(r, "ORCPT=", 6)
#else
			strnicmp(r, "ORCPT=", 6)
#endif
			== 0)
		{
			r += 6;
			orcpt=r;
			orcptlen=s-r;
		}
	}

	buf=courier_malloc(q-p + relayclientl + orcptlen + 10);
	memcpy(buf, p, q-p);
	t=buf + (q-p);
	if (relayclientl)
		memcpy(t, relayclient, relayclientl);
	t += relayclientl;

	*t++ = '\t';
	if (notifyn)
		*t++ = 'N';
	else
	{
		if (notifys)
			*t++ = 'S';
		if (notifyf)
			*t++ = 'F';
		if (notifyd)
			*t++ = 'D';
	}
	*t++ = '\t';
	if (orcptlen)
		memcpy(t, orcpt, orcptlen);
	t[orcptlen]=0;

	submit_write_message(buf);
	free(buf);
	rc=submit_readrcprintcrlf();
	if (rc)
	{
		iovflush();
	}
	return (rc);
}

static void getmoredata(char *p, int *l)
{
time_t	t;

	time(&t);
	t += data_timeout;
	if (iovwaitfordata(&t, 0) || (*l=read(0, p, *l)) <= 0)
	{
		submit_cancel();
		_exit(0);
	}
}

void data()
{
char	databuf[BUFSIZ];
char *p;
int l;
int	rc;

	l=0;
	p=databuf;
	for (;;)
	{
		if (l == 0)
		{
			p=databuf;
			l=sizeof(databuf);
			getmoredata(p, &l);
		}

		if ( *p == '.' )
		{
			++p; --l;
			if (l == 0)
			{
				p=databuf;
				l=sizeof(databuf);
				getmoredata(p, &l);
			}
			if (*p == '\r')
			{
				++p;
				--l;
				if (l == 0)
				{
					p=databuf;
					l=sizeof(databuf);
					getmoredata(p, &l);
				}
				if (*p == '\n')
				{
					++p;
					--l;
					break;
				}
				putc('\r', submit_to);
			}
		}
		for (;;)
		{
			if (l == 0)
			{
				p=databuf;
				l=sizeof(databuf);
				getmoredata(p, &l);
			}
			if (*p == '\r')
			{
				++p;
				--l;
				if (l == 0)
				{
					p=databuf;
					l=sizeof(databuf);
					getmoredata(p, &l);
				}
				if (*p == '\n')	break;
				putc('\r', submit_to);
				continue;
			}
			putc(*p, submit_to);
			++p;
			--l;
		}
		putc('\n', submit_to);
		++p;
		--l;
	}
	fclose(submit_to);
	rc=submit_readrcprintcrlf();
	if (submit_wait() && rc == 0)
	{
		clog_msg_start_err();
		clog_msg_str("submit terminated with a non-zero exit code.");
		clog_msg_send();
		exit(0);
	}

	if (rc == 0)
		teergrube=INIT_TEERGRUBE;
	fclose(fromsubmit);
}

static int bofh(const char *bofh)
{
const char *p=getenv(bofh);

	if (p && *p == '1')	return (1);
	return (0);
}

static char *sasl_conv_func(const char *s, void *dummy)
{
char	*p;

	addiovec("334 ", 4);
	addiovec(s, strlen(s));
	addiovec("\r\n", 2);
	iovflush();

	p=iovreadline();
	if (!p)	exit(0);
	if ((p=strdup(p)) == 0)
	{
		perror("malloc");
		exit(0);
	}
	return (p);
}

static int auth_callback_func(struct authinfo *a, void *va)
{
	char *p;

	strcpy(authuserbuf, "");
	strncat(authuserbuf, (const char *)va, 20);
	strcat(authuserbuf, " ");
	strncat(authuserbuf, a->address,
		sizeof(authuserbuf)-10-strlen(a->address));

	for (p=authuserbuf; *p; p++)
		if ((int)(unsigned char)*p < ' ' || *p >= 127)
			*p=' ';
	return 0;
}

static char *rtrim(char *s)
{
	char *end = s + strlen(s);
	while (s < end)
		if (isspace(*(unsigned char*)--end))
			*end = 0;
		else
			break;
	return s;
}


/*****************************************************************************

courieresmtpd can be started in two situations:

* No command line arguments: courier is not configured to support the ESMTP
  AUTH extension

* Command line arguments are present: Courier is configured to support the
  ESMTP AUTH extension

*****************************************************************************/

int main(int argc, char **argv)
{
int	seenmailfrom;
int	seenrcptto;
char	*line;
int	starttls=0;
int	tls=0;
int	tls_required=0;
int	auth_required=0;
int	authenticated=0;

#if HAVE_SETLOCALE
	setlocale(LC_ALL, "C");
#endif
	/*
	** When called via -bs to sendmail, dump log to /dev/null via stderr,
	** else record everything via syslog.
	*/

	if (chdir(courierdir()))
		clog_msg_errno();

	if (fcntl(0, F_SETFL, O_NONBLOCK) ||
	    fcntl(1, F_SETFL, O_NONBLOCK))
	{
		perror("fcntl");
		exit(1);
	}

	helobuf[0]=0;
	submit_set_teergrube(tarpit);

	if (strcmp(argv[0], "sendmail") == 0)
		clog_open_stderr("courieresmtpd");
		/* We are being called by the sendmail command line wrapper */

	else
	{
		const char *p=getenv("TCPLOCALIP");

		if (p)
			p=truncate_ipv6(p);

		if (p && *p && config_has_vhost(p))
			config_set_local_vhost(p);

		p=getenv("SYSLOGNAME");

		if (!p || !*p)
			p="courieresmtpd";

		clog_open_syslog(p);

		if ((p=getenv("ESMTP_TLS")) && atoi(p))
		{
			p=getenv("TLS_CONNECTED_PROTOCOL");

			tls=1;
			/* This is the smtps service, and we don't need to
			 * initialize the STARTTLS feature. */

			if (p && *p)
			{
				strcpy(tlsbuf, "SSL: ");
				strncat(tlsbuf, p, sizeof(tlsbuf)-6);
			}
		}
		else
		{
			if ((p=getenv("COURIERTLS")) != 0 && *p
				&& access(p, X_OK) == 0 &&
				(p=getenv("TLS_CERTFILE")) != 0 && *p &&
				access(p, R_OK) == 0)
				starttls=1;

			if ((p=getenv("ESMTP_TLS_REQUIRED")) && atoi(p))
			{
				tls_required=1;
				if (!starttls)
				{
					addiovec_error("440 TLS not available,  but"
						" it's required for this connection.");
					return (1);
				}
			}
		}
	}


	{
		const char *p=getenv("AUTH_REQUIRED");

		if (p)
			auth_required=atoi(p);
	}

	iovread_timeout=config_time_esmtptimeout();
	iovwrite_timeout=data_timeout=config_time_esmtpdata();

	tcpremoteip=getenv("TCPREMOTEIP");
	tcpremotehost=getenv("TCPREMOTEHOST");

	if (!tcpremoteip)
	{
		fprintf(stderr, "%s: don't know your IP address, no dice.\n",
			argv[0]);
		exit(1);
	}

	showbanner();
	iovflush();

	signal(SIGPIPE, SIG_IGN);
	/* Some house keeping, while waiting for a reply */

	if (rw_init_courier("esmtp"))	return (1);

	clog_msg_start_info();
	clog_msg_str("started,ip=[");
	clog_msg_str(tcpremoteip);
	clog_msg_str("]");
	clog_msg_send();

	seenmailfrom=0;
	seenrcptto=0;
	sizelimit=config_sizelimit();

	for (;;)
	{
	char	*p;

		input_line=line=iovreadline();

		/* Optionally log the esmtp dialog */
		if ((p=getenv("ESMTP_LOG_DIALOG")) && *p == '1')
		{
			clog_msg_start_info();
			clog_msg_str(input_line);
			clog_msg_str("\n");
			clog_msg_send();
		}

		for (p=line; *p && *p != ':'; p++)
		{
			if (*p == ' ' && strncmp(line, "MAIL", 4) &&
				strncmp(line, "RCPT", 4))
				break;

			*p=toupper(*p);
		}

		rtrim(line);
		if (strcmp(line, "QUIT") == 0)	break;
		if ((strncmp(line, "EHLO ", 5) == 0 ||
			strncmp(line, "HELO ", 5) == 0) &&
			line[5])
		{
			extended=line[0] == 'E';
			ehlo(line+5, starttls, tls);
			iovflush();
			cancelsubmit();
			startsubmit(tls);
			continue;
		}

		if (strcmp(line, "STARTTLS") == 0)
		{
		int	pipefd[2];
		struct couriertls_info cinfo;
		char	*argvec[4];
		char	buf1[NUMBUFSIZE+40];
		char	buf2[NUMBUFSIZE];

		        if (!starttls ||
			    ((p=getenv("TLS_DEBUG_FAIL_STARTTLS_HARD")) &&
			     *p == '1'))
			{
				addiovec_error("540 TLS not available.");
				continue;
			}

		        if ((p=getenv("TLS_DEBUG_FAIL_STARTTLS_SOFT")) &&
			    *p == '1')
			{
				addiovec_error("440 TLS not available.");
				continue;
			}
			if (libmail_streampipe(pipefd))
			{
				addiovec_error("432 libmail_streampipe() failed.");
				exit(1);
			}

			couriertls_init(&cinfo);

			strcat(strcpy(buf1, "-localfd="),
			       libmail_str_size_t(pipefd[1], buf2));

			argvec[0]=buf1;
			argvec[1]="-tcpd";
			argvec[2]="-server";
			argvec[3]=NULL;
			fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);

			addiovec("220 Ok\r\n", 8);
			iovflush();
			iovreset();

			if (couriertls_start(argvec, &cinfo))
			{
				close(pipefd[0]);
				close(pipefd[1]);
				syslog(LOG_MAIL|LOG_ERR,
				       "courieresmtpd: STARTTLS failed: %s\n",
				       cinfo.errmsg);
				couriertls_destroy(&cinfo);
				exit(1);
			}

			close(pipefd[1]);
			strcpy(tlsbuf, "TLS: ");
			strncat(tlsbuf, cinfo.version, 40);
			if (cinfo.bits > 0)
				strcat(strcat(strcat(tlsbuf, ","),
					      libmail_str_size_t(cinfo.bits,
							 buf2)),
				       "bits");
			strncat(strcat(tlsbuf, ","), cinfo.cipher, 40);

			couriertls_export_subject_environment(&cinfo);
			couriertls_destroy(&cinfo);

			tls=1;
			starttls=0;
			tls_required=0;
			close(pipefd[1]);
			close(0);
			close(1);
			if (dup(pipefd[0]) != 0 || dup(pipefd[0]) != 1)
			{
				perror("dup");
				exit(1);
			}
			close(pipefd[0]);
			cancelsubmit();
			startsubmit(tls);	/* Mark */

			setenv("ESMTP_TLS", "1", 1);
			continue;
		}

		if (tls_required && strcmp(line, "RSET"))
		{
			addiovec_error("400 STARTTLS is required first.");
			continue;
		}

		if (strncmp(line, "AUTH ", 5) == 0 && mailfroms == 0
			&& authuserbuf[0] == 0)
		{
		char	*authmethod;
		char	*initreply;
		const char *q;
		char	*buf=strcpy(courier_malloc(strlen(line)+1), line);
		char	*authtype;
		char	*authdata;
		char    fakecmd[80];

		        strcpy(fakecmd, "AUTH ");

			starttls=0;
			(void)strtok(buf, " \t\r\n");
			authmethod=strtok(0, " \t\r\n");

			strncat(fakecmd, authmethod, 20);
			initreply=strtok(0, " \t\r\n");

			if (initreply)
			{
				strcat(fakecmd, " ");
				strncat(fakecmd, initreply, 20);
			}

			if (tls && (q=getenv("ESMTPAUTH_TLS")) != 0 && *q)
				;
			else
				q=getenv("ESMTPAUTH");

			if (q == 0 || *q == 0)	authmethod=0;

			if (authmethod == 0 || *authmethod == 0)
			{
				input_line=fakecmd;
				addiovec_error("535 Authentication rejected");
				free(buf);
				continue;
			}

			if (initreply && *initreply)
			{
				if (strcmp(initreply, "=") == 0)
					initreply="";
			}
			else
			{
				initreply=0;
			}

			if (auth_sasl_ex(authmethod, initreply,
					 smtp_externalauth(),
					 sasl_conv_func,
					 NULL,
					 &authtype,
					 &authdata) != AUTHSASL_OK)
			{
				/* TODO - better error messages */

				strcpy(authuserbuf, "");
			}
			else
			{
				int rc=auth_generic("esmtp", authtype,
						    authdata,
						    auth_callback_func,
						    authmethod);

				if (rc)
				{
					char *p=auth_sasl_extract_userid(authtype, authdata);

					if (p)
					{
						strcat(fakecmd, " ");
						strncat(fakecmd, p,
							sizeof(fakecmd)
							-strlen(fakecmd)-1);
						free(p);
					}
					strcpy(authuserbuf, "");
				}
				free(authtype);
				free(authdata);
			}

			if (authuserbuf[0] == 0)
			{
				input_line=fakecmd;
				addiovec_error("535 Authentication failed.");
				iovflush();
			}
			else
			{
			const char *rc;
			char *p;

				addiovec("235 Ok\r\n", 8);
				iovflush();
				cancelsubmit();
				putenv("BLOCK=");
				rc=getenv("AUTHRELAYCLIENT");
				if (!rc)	rc="";
				p=courier_malloc(sizeof("RELAYCLIENT=")+
					strlen(rc));
				strcat(strcpy(p, "RELAYCLIENT="), rc);
				putenv(p);
				putenv("FAXRELAYCLIENT=");
				startsubmit(tls);
				authenticated=1;
			}
			free(buf);
			continue;
		}

		if (strncmp(line, "NOOP", 4) == 0)
		{
			addiovec("250 Ok\r\n", 8);
			iovflush();
			continue;
		}

		if (auth_required && !authenticated && strcmp(line, "RSET"))
		{
			addiovec_error("535 Authentication required.");
			continue;
		}

		if (strncmp(line, "EXPN ", 5) == 0)
		{
			starttls=0;
			if (bofh("BOFHNOEXPN") == 0 && seenmailfrom == 0)
			{
				cancelsubmit();
				expnvrfy(line+5, "-expn=");
				startsubmit(tls);
			}
			else	addiovec_error("252 EXPN disabled");
			continue;
		}
		if (strncmp(line, "VRFY ", 5) == 0)
		{
			starttls=0;
			if (bofh("BOFHNOVRFY") == 0 && seenmailfrom == 0)
			{
				cancelsubmit();
				expnvrfy(line+5, "-vrfy=");
				startsubmit(tls);
			}
			else	addiovec_error("252 VRFY disabled");
			continue;
		}
		if (strcmp(line, "RSET") == 0)
		{
			starttls=0;
			addiovec("250 Ok\r\n", 8);
			iovflush();
			cancelsubmit();
			startsubmit(tls);
			seenmailfrom=0;
			if (mailfroms)	free(mailfroms);
			mailfroms=0;
			set_submit_error(0, 0);
			continue;
		}
		if (strncmp(line, "MAIL", 4) == 0 && (p=strchr(line, ':')) != 0
			&& !seenmailfrom)
		{
			starttls=0;
			if (!helobuf[0])
			{
				addiovec_error("502 Polite people say HELO first");
				continue;
			}
			if (!submit_started)
			{
				badfork();
				continue;
			}
			if (mailfrom(p+1) == 0)
			{
				seenmailfrom=1;
				seenrcptto=0;
			}
			else
			{
				cancelsubmit();
				startsubmit(tls);
			}
			continue;
		}

		if (strncmp(line, "RCPT", 4) == 0 && (p=strchr(line, ':')) != 0
				&& seenmailfrom)
		{
			if (rcptto(p+1) == 0)
				seenrcptto=1;
			continue;
		}
		if (strcmp(line, "DATA") == 0 && seenmailfrom && seenrcptto)
		{
			set_submit_error(0, 0);
			addiovec("354 Ok.\r\n", 9);
			iovflush();
			putc('\n', submit_to);
			data();
			seenmailfrom=0;
			seenrcptto=0;
			submit_started=0;
			if (mailfroms)	free(mailfroms);
			mailfroms=0;
			startsubmit(tls);
			continue;
		}
		addiovec_error("502 ESMTP command error");
	}
	addiovec("221 Bye.\r\n", 10);
	iovflush();
	return (0);
}
