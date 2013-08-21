/*
** Copyright 1998 - 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	"courier.h"
#include	"sysconfdir.h"
#include	"moduledel.h"
#include	"maxlongsize.h"
#include	"comsubmitclient.h"
#include	"comfax.h"
#include	"rfc2045/rfc2045.h"
#include	"rfc2045/rfc2045charset.h"
#include	"rfc822/rfc822.h"
#include	"numlib/numlib.h"
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	"comctlfile.h"
#include	"comqueuename.h"
#include	"comstrtotime.h"
#include	"comstrtimestamp.h"
#include	<sys/stat.h>
#include	<sys/uio.h>

/*
  The DSN module is used to generate delivery status notifications.
  Messages dispatched to the DSN module result in a delivery status
  notification being sent to the sender.  The DSN is generated like
  a regular message, and is injected via submit.

  When the message is dispatched to this module with the host
  parameter set to "deferred", this module generated a delayed
  DSN, a DSN that's sent for all recipients who have not been delivered
  to, yet.  Otherwise, this is a permanent DSN, either a non-delivery
  report or a return receipt (or both!).

*/


/* Various bits and pieces of the DSN */

static char *dsnsubjectwarn;
static char *dsnsubjectnotice;
static char *dsnheader;
static char *dsnfooter;
static char *dsndelayed;
static char *dsnrelayed;
static char *dsndelivered;
static char *dsnfailed;

static const char *dsnfrom;
static size_t dsnlimit;

static void dsninit()
{
static struct { char **textptr, *filenameptr; } texts[]={
	{ &dsnsubjectwarn,	SYSCONFDIR "/dsnsubjectwarn.txt"},
	{ &dsnsubjectnotice,	SYSCONFDIR "/dsnsubjectnotice.txt"},
	{ &dsnheader,		SYSCONFDIR "/dsnheader.txt"},
	{ &dsnfooter,		SYSCONFDIR "/dsnfooter.txt"},
	{ &dsndelayed,		SYSCONFDIR "/dsndelayed.txt"},
	{ &dsnrelayed,		SYSCONFDIR "/dsnrelayed.txt"},
	{ &dsndelivered,	SYSCONFDIR "/dsndelivered.txt"},
	{ &dsnfailed,		SYSCONFDIR "/dsnfailed.txt"},
	} ;
int	i;

	for (i=0; i<sizeof(texts)/sizeof(texts[0]); i++)
		if ( (*texts[i].textptr=
			i < 2 ? config_read1l(texts[i].filenameptr):
			readfile(texts[i].filenameptr, 0)) == 0)
		{
			clog_msg_start_err();
			clog_msg_str("Missing ");
			clog_msg_str(texts[i].filenameptr);
			clog_msg_send();
			exit(1);
		}
}

static int dodsn(struct ctlfile *, FILE *, const char *, int);

static void defer(struct ctlfile *ctf)
{
struct	iovec	iov[3];
static const char completed[1]={COMCTLFILE_DELDEFERRED};
time_t	current_time;
const char	*t;

	iov[0].iov_base=(caddr_t)&completed;
	iov[0].iov_len=sizeof(completed);

	time(&current_time);
	t=strtimestamp(current_time);
	iov[1].iov_base=(caddr_t)t;
	iov[1].iov_len=strlen(t);
	iov[2].iov_base=(caddr_t)"\n";
	iov[2].iov_len=1;
	ctlfile_appendv(ctf, iov, 3);
}

int main(int argc, char **argv)
{
struct moduledel *p;

	clog_open_syslog("courierdsn");
	dsninit();
	if (chdir(getenv("COURIER_HOME")))
		clog_msg_errno();

	libmail_changeuidgid(MAILUID, MAILGID);

	dsnfrom=config_dsnfrom();
	dsnlimit=config_dsnlimit();

	if (argc > 1)	/* For testing only */
	{
	struct	ctlfile ctf;

		if (ctlfile_openi(atol(argv[1]), &ctf, 0))
			clog_msg_errno();
		ctlfile_setvhost(&ctf);
		dodsn(&ctf, stdout, "sender", 
			argc > 2 ? atoi(argv[2]):0);
		ctlfile_close(&ctf);
		exit(0);
	}

	if (atol(getenv("MAXRCPT")) > 1)
	{
		clog_msg_start_err();
		clog_msg_str("Invalid configuration in config, set MAXRCPT=1");
		clog_msg_send();
		exit(1);
	}

	/* Loop to receive messages from Courier */

	module_init(0);
	while ((p=module_getdel()) != NULL)
	{
	pid_t	pid;
	unsigned delid;

		if (p->nreceipients != 1)
		{
			clog_msg_start_err();
			clog_msg_str("Internal error - invalid # of receipients.");
			clog_msg_send();
			exit(1);
		}

		/*
		** Each DSN is handled by a subproc, whose output
		** is piped to submit.
		*/

		delid=atol(p->delid);

		if ((pid=module_fork(delid, 0)) == -1)
		{
			clog_msg_prerrno();
			module_completed(delid, delid);
			continue;
		}

		if (pid == 0)
		{
			char	*args[10];

			/* Arguments to submit to initialize message
			** metadata */

			struct	ctlfile ctf;
			int	rc;
			int	isdeferred=strcmp(p->host, "deferred") == 0;

			const char *sec_level_orig;
			char *envp[4];
			int nenvp=0;
			int i;
			int argc;

			char msgsource[80];


			if (ctlfile_openi(p->inum, &ctf, 0))
				clog_msg_errno();


			argc=0;


			args[argc++]="submit";
			args[argc++]="-delay=0";

			strcpy(msgsource, "-src=");

			if ((i=ctlfile_searchfirst(&ctf,
						   COMCTLFILE_MSGSOURCE)) >= 0)
			{
				strncat(msgsource, ctf.lines[i]+1, 20);
				strcat(msgsource, "/");
			}
			strcat(msgsource, "dsn");
			args[argc++]=msgsource;
			args[argc++]="dsn";
			args[argc++]="dns; localhost (localhost [127.0.0.1])";
			args[argc++]="ftp://ftp.isi.edu/in-notes/rfc1894.txt";
			args[argc++]=NULL;

			sec_level_orig=ctlfile_security(&ctf);
			if (sec_level_orig && *sec_level_orig)
			{
				envp[nenvp]
					=courier_malloc(strlen(sec_level_orig)
							+30);
				strcat(strcat(strcpy(envp[nenvp],
						     "DSNRET=S{"),
					      sec_level_orig), "}");
				++nenvp;
			}
			envp[nenvp]=0;

			if (submit_fork(args, envp, 0))
			{
				clog_msg_start_err();
				clog_msg_str("Problems injecting bounce - running submit.");
				clog_msg_send();
				exit(1);
			}

			if (ctf.sender[0] == '\0')
				submit_write_message(TRIPLEBOUNCE);
				/*
				** This was a doublebounce, set env sender to
				** TRIPLEBOUNCE magic.
				*/
			else
				submit_write_message("");

			if (submit_readrc())
			{
				clog_msg_start_err();
				clog_msg_str("Problems injecting bounce - sender rejected.");
				clog_msg_send();
				defer(&ctf);
				exit(1);
			}

			/* Courier figures out the return address, and gives
			** it to me as the receipient!!!
			*/

			submit_write_message(p->receipients[1]);
			if (submit_readrc())
			{
				clog_msg_start_err();
				clog_msg_str("Problems injecting bounce - receipient ");
				clog_msg_str(p->receipients[1]);
				clog_msg_str(" rejected.");
				clog_msg_send();
				defer(&ctf);
				exit(1);
			}
			submit_write_message("");

			rc=dodsn(&ctf, submit_to, p->receipients[1],
				strcmp(p->host, "deferred") == 0);

			if (rc)
			{
				clog_msg_start_err();
				clog_msg_str("Problems injecting bounce - unable to generate message.");
				clog_msg_send();
			}

			if (fflush(submit_to) || ferror(submit_to))
			{
				clog_msg_start_err();
				clog_msg_str("Problems injecting bounce - unable to flush message.");
				clog_msg_send();
				rc=1;
			}
			if (fclose(submit_to))
			{
				clog_msg_start_err();
				clog_msg_str("Problems injecting bounce - unable to close message.");
				clog_msg_send();
				rc=1;
			}

			if (rc)
				submit_cancel();
			else if (submit_wait())
			{
				clog_msg_start_err();
				clog_msg_str("Problems injecting bounce - submit failed.");
				clog_msg_send();
				rc=1;
			}

			if (rc == 0)
			{
				time_t	t;

				time(&t);

				/*
				** After sending a delayed DSN, mark the
				** message so it doesn't ever get sent again!
				*/

				if (isdeferred)
				{
				char	buf[MAXLONGSIZE+2];

					buf[0]=COMCTLFILE_WARNINGSENT;
					strcat(strcpy(buf+1,
						strtimestamp(t)), "\n");
					ctlfile_append(&ctf, buf);
					ctlfile_close(&ctf);
				}
				else
				{
				/*
				** For PERMANENT dsns, it is my responsibility
				** to remove the original message when done!!!
				*/

					ctlfile_close(&ctf);
					qmsgunlink(p->inum, t);
					unlink(qmsgsdatname(p->inum));
					unlink(qmsgsctlname(p->inum));
				}
			}
			else
			{
				defer(&ctf);
			}
			exit(rc);
		}
	}
	return (0);
}

/*
** When building a DSN, we need to find the last delivery info for each
** recipient.  What we do is search for the first first COMCTLFILE_DELINFO
** after the last COMCTLFILE_DELDEFERRED record, that is the first info
** record after the last message deferred marker.
**
** HOWEVER, we always prefer to take the last record with a
** COMCTLFILE_DELINFO_PEER, which is generated after we succesfully connect
** to a remote peer, because we usually get a better error message in that
** situation (for a flaky peer we're not interested in the last connection
** refused message, we want to know why the peer deferred us the last time
** we managed to contact it).
*/

static int find_delivery_info(struct ctlfile *ctf, unsigned nreceipient)
{
int	last_delivery=-1, last_connected_delivery= -1;
int	isnewdelivery=1;
int	i;

	for (i=0; ctf->lines[i]; i++)
	{
	char	c=ctf->lines[i][0];
	const char *p;
	unsigned	n;

		if (c != COMCTLFILE_DELDEFERRED && c != COMCTLFILE_DELINFO)
			continue;

		p=ctf->lines[i]+1;
		n=0;
		while (*p >= '0' && *p <= '9')
			n = n * 10 + (*p++ - '0');
		if (n != nreceipient)	continue;

		if (c != COMCTLFILE_DELINFO)
		{
			isnewdelivery=1;
			continue;
		}
		if (isnewdelivery)	last_delivery=i;
		isnewdelivery=0;

		while ( *p == ' ')	++p;
		if (*p == COMCTLFILE_DELINFO_PEER)
			last_connected_delivery=last_delivery;
	}

	if (last_connected_delivery < 0)
		last_connected_delivery=last_delivery;
	return (last_connected_delivery);
}

/*
** print_del_info is called twice, once to print out a verbose delivery
** status in the verbose part of the DSN, and the second time to generate
** the recipient headers in the message/delivery-status.  It picks up after
** the record returned by find_delivery_info, then for each record for this
** recipient, up until the next SUCCESS, FAIL, or DEFERRED record, calls
** a separate function to print this kind of a record.
*/


static void print_del_info(struct ctlfile *ctf, unsigned nreceipient,
	int start,
	void (*handler_func)(const char *, const char, void *), void *arg)
{
	for ( ; ctf->lines[start] &&
		((ctf->lines[start][0] != COMCTLFILE_DELSUCCESS &&
		ctf->lines[start][0] != COMCTLFILE_DELFAIL &&
		ctf->lines[start][0] != COMCTLFILE_DELDEFERRED)
		|| atol(ctf->lines[start]+1) != nreceipient); start++)
	{
	const char *p;
	unsigned	n;
	char	type;

		if (ctf->lines[start][0] != COMCTLFILE_DELINFO)	continue;

		p=ctf->lines[start]+1;
		n=0;
		while (*p >= '0' && *p <= '9')
			n = n * 10 + (*p++ - '0');
		if (n != nreceipient)	continue;

		while ( *p == ' ')	++p;
		type= *p;

		if (*p)	p++;
		while ( *p == ' ')	++p;
		(*handler_func)(p, type, arg);
	}
}

/*
** To properly set Content-Transfer-Encoding on our DSN we need to know
** whether message contents or metadata includes 8bit characters.
*/




static void check8bit(const char *str, const char type, void *flag)
{
	for ( ; *str; str++)
		if ( (unsigned char)*str & 0x80 )
			*(int *)flag=1;
}

static int hexconvert(char a, char b)
{
	if ( a >= 'A' && a <= 'F')
		a -= 'A'-10;
	if ( b >= 'A' && b <= 'F')
		b -= 'B'-10;

	return ( (a & 15) * 16 + (b & 15));
}

static int is8bitreceipient(struct ctlfile *ctf, unsigned nreceipient)
{
int	flag=0;
const char *p=ctf->oreceipients[nreceipient];
int	infoptr;

	if (p && *p)
		while (*p)
		{
			if (*p == '+' && p[1] && p[2] &&
				hexconvert(p[1], p[2]) & 0x80)
			return (1);
			p++;
		}
	else
	{
		check8bit(ctf->receipients[nreceipient], 0, &flag);
		if (flag)	return (1);
	}
	infoptr=find_delivery_info(ctf, nreceipient);
	if (infoptr >= 0)
		print_del_info(ctf, nreceipient, infoptr, &check8bit, &flag);
	return (flag);
}

static int is8bitdsn(struct ctlfile *ctf, int isdelayeddsn)
{
unsigned	n;
int	i, flag=0;

	if ((i=ctlfile_searchfirst(ctf, COMCTLFILE_ENVID)) >= 0)
	{
		check8bit(ctf->lines[i], 0, &flag);
		if (flag)	return (1);
	}

	for (n=0; n<ctf->nreceipients; n++)
	{
		if (!dsn_sender(ctf, n, isdelayeddsn))	continue;
		if (is8bitreceipient(ctf, n))	return (1);
	}
	return (0);
}

static void print_xtext(FILE *fp, const char *xtext)
{
	while (*xtext)
	{
		if (*xtext != '+' || xtext[1] == 0 || xtext[2] == 0)
		{
			putc(*xtext, fp);
			xtext++;
		}
		else
		{
			putc( hexconvert(xtext[1], xtext[2]), fp);
			xtext += 3;
		}
	}
}

static void print_receipient(FILE *fp, struct ctlfile *ctf, unsigned n)
{
	fputs(" <", fp);
	if (ctf->oreceipients[n] &&
#if	HAVE_STRNCASECMP
		strncasecmp(ctf->oreceipients[n], "rfc822;", 7)
#else
		strnicmp(ctf->oreceipients[n], "rfc822;", 7)
#endif
		== 0)
		print_xtext(fp, ctf->oreceipients[n]+7);
	else
		fprintf(fp, "%s", ctf->receipients[n]);
	putc('>', fp);
}

/* Format delivery info in the plain text portion of the response. */

static void format_del_info(const char *p, const char type, void *q)
{
	switch (type)	{
	case COMCTLFILE_DELINFO_PEER:
		if (*p)
			fprintf((FILE *)q, "     %s:\n", p);
		break;
	case COMCTLFILE_DELINFO_CONNECTIONERROR:
		fprintf((FILE *)q, " *** %s\n", p);
		break;
	case COMCTLFILE_DELINFO_SENT:
		fprintf((FILE *)q, " >>> %s\n", p);
		break;
	case COMCTLFILE_DELINFO_REPLY:
		fprintf((FILE *)q, " <<< %-1.500s\n", p);
		break;
	}
}

/* Include original message contents in the DSN */

static int print_message(FILE *datfile, FILE *fp, int fullmsg)
{
char	buf[BUFSIZ];

	if (fseek(datfile, 0L, SEEK_SET) == -1)	return (-1);

	while (fgets(buf, sizeof(buf), datfile))
	{
		if (!fullmsg && (
			buf[0] == '\n' || (buf[0] == '\r' && buf[1] == '\n')))
			break;
		fprintf(fp, "%s", buf);
		if (strchr(buf, '\n') == 0)
		{
		int	c;

			while ((c=getc(datfile)) >= 0)
			{
				putc(c, fp);
				if (c == '\n')	break;
			}
		}
	}
	return (0);
}

/*
** Since the DSN is a MIME message, we must create a MIME boundary, and I
** do it right by making sure that the MIME boundary is not to be found in
** the message.
*/

static int search_boundary(FILE *f, const char *b, int *bit8flag)
{
char	buf[BUFSIZ];
size_t	l=strlen(b);
const char *p;
int	flag=0;
int	cr;

	if (fseek(f, 0L, SEEK_SET))	return (-1);

	while (fgets(buf, sizeof(buf), f))
	{
		if (buf[0] == '-' && buf[1] == '-' &&

#if	HAVE_STRNCASECMP
			strncasecmp(buf+2, b, l)
#else
			strnicmp(buf+2, b, l)
#endif
			 == 0)	flag=1;

		cr=0;
		for (p=buf; *p; p++)
			if (*p == '\n')	cr=1;
			else if ((int)(unsigned char)*p >= 127)
				*bit8flag=1;

		if (!cr)
		{
		int	c;

			while ((c=getc(f)) >= 0 && c != '\n')
				if ((int)(unsigned char)c >= 127)
					*bit8flag=1;
		}
	}
	return (flag);
}

struct dsndiagnosticinfo {
	FILE *fp;
	int flag;
	char prefix[70];
	} ;

static void printremotemta(const char *p, const char type, void *q)
{
struct dsndiagnosticinfo *i=(struct dsndiagnosticinfo *)q;

	if (type == COMCTLFILE_DELINFO_PEER && i->flag == 0)
	{
		fprintf(i->fp, "Remote-MTA: dns; %s\n", p);
		i->flag=1;
	}
}

static void print_network_error(const char *p, const char type, void *q)
{
struct dsndiagnosticinfo *i=(struct dsndiagnosticinfo *)q;

	if (type == COMCTLFILE_DELINFO_CONNECTIONERROR)
	{
		i->flag=1;
		fprintf(i->fp, "X-Network-Error: %s\n", p);
	}
}

static void set_msg_type(struct dsndiagnosticinfo *p, const char *t)
{
	strcpy(p->prefix, "Diagnostic-Code: ");
	strncat(p->prefix, t, 20);
	strcat(p->prefix, "; ");
}

static void print_smtp_info(const char *p, const char type, void *q)
{
struct dsndiagnosticinfo *i=(struct dsndiagnosticinfo *)q;

	/* First REPLYTYPE we see becomes the Diagnostic Code reply type */

	if (type == COMCTLFILE_DELINFO_REPLYTYPE && i->prefix[0])
		set_msg_type(i, p);

	if (type == COMCTLFILE_DELINFO_REPLY)
	{
		fprintf(i->fp, "%s%s\n",
			( i->prefix[0] ? i->prefix:"        "), p);
		i->prefix[0]=0;	/* Kill any stray REPLYTYPEs */
	}
}

static int isfax(const char *p)
{
	int dummy;

	if ((p=strrchr(p, '@')) == NULL)
		return (0);

	if (comgetfaxopts(p+1, &dummy) == 0)
		return (1);
	return (0);
}

static void print_header(FILE *f, char *template, const char *me,
			 const char *from_time, const char *from_mta)
{
        unsigned char c, c2;
        int i = 0;

        while ((c=template[i++]) > 0)
	{
		char        kw[64];
		if (c != '[')
		{
			putc(c, f);
                        continue;
		}

		if (template[i] != '#')
		{
			putc('[', f);
			continue;
		}

		++i;

		c2=0;
		while ((c=template[i]) != 0 && (isalnum(c) || c == '_'))
		{
                        if (c2 < sizeof(kw)-1)
                                kw[c2++]=c;
			++i;
		}
		kw[c2]=0;

		if (c != '#')
		{
			fprintf(f, "[#%s", kw);
			continue;
		}

		++i;
		if (template[i] != ']')
		{
			fprintf(f, "[#%s#", kw);
			continue;
		}
		++i;
		if (strcmp(kw, "ME") == 0)
		{
			fprintf(f, "%s", me);
		}
		else if (strcmp(kw, "FROMTIME") == 0)
		{
			fprintf(f, "%s", from_time);
		}
		else if (strcmp(kw, "FROMMTA") == 0)
		{
			fprintf(f, "%s", from_mta);
		}
        }
}

static int dodsn(struct ctlfile *ctf, FILE *fp, const char *sender,
		int dodelayed)
{
int	dsn8flag=is8bitdsn(ctf, dodelayed);
const char *datfilename;
FILE	*datfile;
struct	stat	stat_buf;
unsigned i;
int	j;
int	returnmsg=0;
char	boundary[MAXLONGSIZE+40];
int	msg8flag=0;
const	char *from_mta;
const	char *from_mta_p;
const	char *from_time;

	datfilename=qmsgsdatname(ctf->n);
	if ((datfile=fopen(datfilename, "r")) == 0 ||
		fstat(fileno(datfile), &stat_buf))
	{
		if (datfile)	fclose(datfile);
		clog_msg_start_err();
		clog_msg_str("Cannot open ");
		clog_msg_str(datfilename);
		clog_msg_send();
		return (0);
	}

	/*
	** Decide if we want to return the entire message, or just the
	** headers.  The entire message is returned only if:
	**
	** A) This is a double bounce, i.e., a bounce where the envelope
	**    sender is null.  We want to know everything about the double
	**    bounce.
	**
	**    OR
	**
	** B) This is NOT a delayed DSN, AND, the message size is below
	**    a certain limit, and RET=HDRS was NOT specified, AND
	**    this message does not include return receipts.
	**
	**    ( No need to include the whole kit and kaboodle for return
	**      receipts )
	**
	*/

	if (ctf->sender[0] == '\0')
		returnmsg=1;
	else if (!dodelayed && stat_buf.st_size < dsnlimit &&
		(j=ctlfile_searchfirst(ctf, COMCTLFILE_DSNFORMAT)) >= 0 &&
		strchr(ctf->lines[j], 'H') == 0)
	{
		for (i=0; i<ctf->nreceipients; i++)
		{
			if (ctf->delstatus[i][0] == COMCTLFILE_DELSUCCESS)
				continue; /* never full msg for receipts */

			if (dsn_sender(ctf, i, 0))
			{
				returnmsg=1;
				break;
			}
		}
	}

	i=0;
	do
	{
		sprintf(boundary, "=_courier_%u", i++);
	} while (search_boundary(datfile, boundary, &msg8flag));

	fprintf(fp, "From: %s\nTo: %s\nSubject: %s\n", dsnfrom, sender,
		dodelayed ? dsnsubjectwarn:dsnsubjectnotice);
	fprintf(fp, "Mime-Version: 1.0\nContent-Type: multipart/report; report-type=delivery-status;\n    boundary=\"%s\"\nContent-Transfer-Encoding: %s\n\n"
		RFC2045MIMEMSG "\n--%s\nContent-Transfer-Encoding: %s\n",
		boundary,
		dsn8flag || msg8flag ? "8bit":"7bit",
		boundary,
		dsn8flag ? "8bit":"7bit");

	if ((j=ctlfile_searchfirst(ctf, COMCTLFILE_FROMMTA)) >= 0)
		from_mta=ctf->lines[j]+1;
	else
		from_mta=0;

	if (from_mta && (from_mta_p=strchr(from_mta, ';')) != 0)
	{
		while (*++from_mta_p == ' ')
			;
	}
	else	from_mta_p=0;

	from_time=rfc822_mkdate(stat_buf.st_mtime);

	print_header(fp, dsnheader, config_me(), from_time, 
		from_mta_p && *from_mta_p ? from_mta_p:"unknown");

	if (dodelayed)
	{
		/* This is a delayed DSN */

		fprintf(fp, dsndelayed, config_me());
		for (i=0; i<ctf->nreceipients; i++)
		{
		int	infoptr;

			if (!dsn_sender(ctf, i, 1))	continue;
			fprintf(fp, "\n");
			print_receipient(fp, ctf, i);
			fprintf(fp, ":\n");
			infoptr=find_delivery_info(ctf, i);
			if (infoptr >= 0)
				print_del_info(ctf, i, infoptr,
					format_del_info, fp);
		}
	}
	else
	{
	int	printed_header;
	int	pass;

		/* A final DSN can have up to three parts:
		**
		**    0. Return receipts.
		**    1. Mail relayed to a non-DSN gateway, no receipt possible.
		**    2. Failed recipients.
		**
		** Do each one in turn.  For each pass, search for recipients
		** that fall into this category.  The first time one is found,
		** generate a header.
		*/

		for (pass=0; pass<3; pass++)
		{
			printed_header=0;
			for (i=0; i<ctf->nreceipients; i++)
			{
			int	infoptr;
			const	char *status=ctf->delstatus[i];

				if (pass == 2)
				{
					if (*status == COMCTLFILE_DELSUCCESS)
						continue;
				}
				else if (*status != COMCTLFILE_DELSUCCESS
					|| strchr(status+1,
						(pass ? 'r':'l')) == 0)
					continue;
				
				if (!dsn_sender(ctf, i, 0))	continue;
				if (!printed_header)
				{
					fprintf(fp, "%s",
						(pass == 2 ? dsnfailed:
							pass ? dsnrelayed:
								dsndelivered));
					printed_header=1;
				}
				fprintf(fp, "\n");
				print_receipient(fp, ctf, i);
				fprintf(fp, ":\n");
				infoptr=find_delivery_info(ctf, i);
				if (infoptr >= 0)
					print_del_info(ctf, i, infoptr,
						format_del_info, fp);
			}
		}
	}

	fprintf(fp, "%s\n\n--%s\nContent-Type: message/delivery-status\nContent-Transfer-Encoding: %s\n\nReporting-MTA: dns; %s\nArrival-Date: %s\n",
		dsnfooter,
		boundary,
		dsn8flag ? "8bit":"7bit",
		config_me(), from_time);

	if (  (j=ctlfile_searchfirst(ctf, COMCTLFILE_ENVID)) >= 0 &&
		ctf->lines[j][1])
	{
		fprintf(fp, "Original-Envelope-Id: ");
		print_xtext(fp, ctf->lines[j]+1);
		fprintf(fp, "\n");
	}

	if (from_mta && strchr(from_mta, ';') != 0)
		fprintf(fp, "Received-From-MTA: %s\n", from_mta);

	for (i=0; i<ctf->nreceipients; i++)
	{
	const char *action;
	const char *status;
	const char *p;
	int infoptr;

		if (!dsn_sender(ctf, i, dodelayed))	continue;

		p=ctf->delstatus[i];
		if (*p == COMCTLFILE_DELSUCCESS)
		{
			action="delivered";
			status="2.0.0";
			for ( ++p; *p; ++p)
			{
				if (*p == 'l')
					break;
				if (*p == 'r')
				{
					action="relayed";
					break;
				}
			}
			if (!*p)	continue;
		}
		else
		{
			if (dodelayed)
			{
				action="delayed";
				status="4.0.0";
			}
			else
			{
				action="failed";
				status="5.0.0";
			}
		}

		fprintf(fp, "\nFinal-Recipient: rfc822; %s\nAction: %s\n",
			ctf->receipients[i], action);

		if (ctf->oreceipients[i] && ctf->oreceipients[i][0])
		{
		const char *c;

			fprintf(fp, "Original-Recipient: ");
			for (c=ctf->oreceipients[i]; *c; c++)
			{
				putc(*c, fp);
				if (*c == ';')
				{
					putc(' ', fp);
					c++;
					break;	
				}
			}
			print_xtext(fp, c);
			fprintf(fp, "\n");
		}
		fprintf(fp, "Status: %s\n", status);

		if (!dodelayed)
		{
			infoptr=find_delivery_info(ctf, i);
			if (infoptr >= 0)
			{
			struct dsndiagnosticinfo info;

				info.fp=fp;
				info.flag=0;
				print_del_info(ctf, i, infoptr,
					&printremotemta, &info);

				info.flag=0;
				set_msg_type(&info, "unknown");

				print_del_info(ctf, i, infoptr,
					&print_network_error, &info);

				if (info.flag == 0)
					print_del_info(ctf, i, infoptr,
						&print_smtp_info, &info);
			}
		}

		if (dodelayed &&
			(j=ctlfile_searchfirst(ctf,
					       isfax(ctf->receipients[i]) ?
					       COMCTLFILE_FAXEXPIRES:
					       COMCTLFILE_EXPIRES)) >= 0)
			fprintf(fp, "Will-Retry-Until: %s\n",
				rfc822_mkdate(strtotime(ctf->lines[j]+1)));
	}
	fprintf(fp, "\n--%s\n", boundary);

	fprintf(fp, "Content-Type: %s\nContent-Transfer-Encoding: %s\n\n",
		returnmsg ? "message/rfc822":"text/rfc822-headers; charset=us-ascii",
		msg8flag ? "8bit":"7bit");

	if (print_message(datfile, fp, returnmsg))
	{
		fclose(datfile);
		return (-1);
	}

	fprintf(fp, "\n--%s--\n", boundary);
	fclose(datfile);
	return (0);
}
