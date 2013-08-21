/*
** Copyright 2000-2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	"courier.h"
#include	"rw.h"
#include	"moduledel.h"
#include	"rfc822/rfc822.h"
#include	"numlib/numlib.h"
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<signal.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<pwd.h>
#include	<sys/wait.h>
#include	<sys/time.h>
#include	"comctlfile.h"
#include	"comqueuename.h"
#include	"comstrtotime.h"
#include	"comstrtimestamp.h"
#include	"comverp.h"
#include	<sys/stat.h>

static void uux(struct moduledel *);

static void (*rewrite_func)(struct rw_info *, void (*)(struct rw_info *));

static const char *uucp_is_disabled=0;

int main(int argc, char **argv)
{
struct moduledel *p;
struct passwd *pwd;

	clog_open_syslog("courieruucp");
	if (chdir(getenv("COURIER_HOME")))
		clog_msg_errno();

	pwd=getpwnam("uucp");
	if (!pwd)
	{
		uucp_is_disabled=
			"ERROR: no uucp user found, outbound UUCP is DISABLED!",
		clog_msg_start_err();
		clog_msg_str(uucp_is_disabled);
		clog_msg_send();

		libmail_changeuidgid(MAILUID, MAILGID);
	}
	else
		libmail_changeuidgid(pwd->pw_uid, MAILGID);


	rw_init_courier("uucp");
        rewrite_func=rw_search_transport("uucp")->rw_ptr->rewrite;

	/* Loop to receive messages from Courier */

	module_init(0);
	while ((p=module_getdel()) != NULL)
	{
	pid_t	pid;
	unsigned delid;

		delid=atol(p->delid);

		if ((pid=module_fork(delid, 0)) == -1)
		{
			clog_msg_prerrno();
			module_completed(delid, delid);
			continue;
		}

		if (pid == 0)
		{
			uux(p);
			exit(0);
		}
	}
	return (0);
}

static char	errbuf[512];
static char	*errbufptr;
static unsigned errbufleft;
static int	pid;
static int	uuxpipe;
static int	uuxerr;

static struct rfc822token *delhostt;

struct uucprwinfo {
	struct rfc822token *delhostt;
	void (*rewrite_func)(struct rw_info *, void (*)(struct rw_info *));
	} ;

static void call_rewrite_func(struct rw_info *i,
		void (*func)(struct rw_info *),
		void *voidarg)
{
struct uucprwinfo *arg= (struct uucprwinfo *)voidarg;

	i->host=arg->delhostt;
	(*arg->rewrite_func)(i, func);
}

static RETSIGTYPE alarm_sig(int n)
{
	n=n;
	kill(pid, SIGKILL);
#if	RETSIGTYPE != void
	return (0)
#endif
}

static void adderrbuf(const char *msg, unsigned l)
{
	if (l == 0)	l=strlen(msg);
	if (l > errbufleft)	l=errbufleft;
	if (l)
	{
		memcpy(errbufptr, msg, l);
		errbufptr += l;
		errbufleft -= l;
	}
}

static int readuuxerr()
{
char	buf[256];
int	n;

	while ((n=read(uuxerr, buf, sizeof(buf))) < 0 && errno == EINTR)
		;

	if (n <= 0)
		return (-1);

	adderrbuf(buf, n);
	return (0);
}

static void uux2(struct moduledel *, struct ctlfile *, unsigned *, unsigned);

static void uux(struct moduledel *p)
{
struct	ctlfile ctf;
unsigned *reciparray;
unsigned nreceipients=p->nreceipients;
struct	rfc822t *hostt;
unsigned i;

	if ((reciparray=malloc(sizeof(unsigned)*nreceipients)) == 0)
		clog_msg_errno();

	for (i=0; i<nreceipients; i++)
		reciparray[i]=i;

	if (ctlfile_openi(p->inum, &ctf, 0))
		clog_msg_errno();
	ctlfile_setvhost(&ctf);

	/* Save host we will be contacting, for rewriting */

	hostt=rw_rewrite_tokenize(p->host);
	delhostt=hostt->tokens;

	if (ctlfile_searchfirst(&ctf, COMCTLFILE_VERP) < 0 ||
		*p->sender == 0)
		/* No VERP */
		uux2(p, &ctf, reciparray, nreceipients);
	else
	{
	const char *save_sender=p->sender;

		for (i=0; i<nreceipients; i++)
		{
		/* The real recipient is host!recipient */

		const char *receipient=p->receipients[reciparray[i]*2+1];
		char	*recip=courier_malloc(strlen(p->host)
				+strlen(receipient)+2);
		char	*verp_sender=0;

			strcat(strcat(strcpy(recip, p->host), "!"), receipient);

			verp_sender=courier_malloc(strlen(save_sender)+
				verp_encode(recip, 0)+1);

			strcat(strcpy(verp_sender, save_sender), "-");
			verp_encode(recip, verp_sender+strlen(verp_sender));

			p->sender=verp_sender;
			uux2(p, &ctf, reciparray+i, 1);
			free(verp_sender);
		}
		p->sender=save_sender;
	}
	ctlfile_close(&ctf);
	free(reciparray);
}

static int dowrite(const char *, unsigned, void *);

/*
**	In order to properly quote a non-file argument to uux:
**
**	Surround the argument with ().
**
**	This sux.
*/

static char *uucp_quote(const char *arg)
{
char	*s;

	s=courier_malloc(strlen(arg)+3);
	strcat(strcat(strcpy(s, "("), arg), ")");
	return (s);
}

/*
** Now, for those recipients that have NOTIFY=NEVER, we fudge the envelope
** sender to <>
*/

static void uux3(struct moduledel *p, struct ctlfile *ctf,
	unsigned *reciparray, unsigned nreceipients);

static void uux2(struct moduledel *p, struct ctlfile *ctf,
	unsigned *reciparray, unsigned nreceipients)
{
int	pass;
const char *sender=p->sender;
unsigned *savearray;
unsigned i, j;

	if (nreceipients == 0)	return;

	savearray=courier_malloc(nreceipients * sizeof(*reciparray));

        for (pass=0; pass<2; pass++)
        {
                if (pass)       p->sender="";

		j=0;

                for (i=0; i<nreceipients; i++)
                {
                const char *dsnptr=ctf->dsnreceipients[
			atoi(p->receipients[reciparray[i]*2])];

                        if (dsnptr && strchr(dsnptr, 'N'))
                        {
                                if (pass == 0)  continue;
                        }
                        else
                        {
                                if (pass == 1)  continue;
                        }
			savearray[j++]= reciparray[i];
		}

		if (j)
			uux3(p, ctf, savearray, j);
	}
	p->sender=sender;
	free(savearray);
}



static void uux3(struct moduledel *p, struct ctlfile *ctf,
	unsigned *reciparray, unsigned nreceipients)
{
char	*s, *last;
int	pipefd[2];
int	pipefd2[2];
int	fp;
int	j;
int	waitstat;
pid_t	pid2;
char	*saveerrbuf;
unsigned i;
struct uucprwinfo uucprwinfo_s;
struct stat stat_buf;
const char *sec;

	if (p->nreceipients == 0)	return;

	if (uucp_is_disabled)
	{
		for (i=0; i<nreceipients; i++)
		{
			ctlfile_append_reply(ctf,
				(unsigned)atol(p->receipients[reciparray[i]*2]),
				uucp_is_disabled,
				COMCTLFILE_DELDEFERRED, 0);
 		}
		ctlfile_close(ctf);
		exit(0);
		return;
	}

	sec=ctlfile_security(ctf);
	if (strncmp(sec, "STARTTLS", 8) == 0)
	{
		for (i=0; i<nreceipients; i++)
		{
			ctlfile_append_reply(ctf,
				(unsigned)atol(p->receipients[reciparray[i]*2]),
				"Unable to set minimum security level.",
				COMCTLFILE_DELFAIL_NOTRACK, 0);
 		}
		ctlfile_close(ctf);
		exit(0);
		return;
	}

	if (pipe(pipefd) || pipe(pipefd2))
		clog_msg_errno();

	if ((fp=open(qmsgsdatname(p->inum), O_RDONLY)) < 0)
	{
		for (i=0; i<nreceipients; i++)
		{
			ctlfile_append_reply(ctf,
				(unsigned)atol(p->receipients[reciparray[i]*2]),
				"Cannot open message file.",
				COMCTLFILE_DELFAIL_NOTRACK, 0);
 		}
		ctlfile_close(ctf);
		exit(0);
		return;
	}

	if (fstat(fp, &stat_buf) == 0)
		ctf->msgsize=stat_buf.st_size;

	pid=fork();
	if (pid == -1)
		clog_msg_errno();

	if (pid == 0)
	{
	char *argenv, *argenvcopy, *s;
	unsigned	nargs_needed;
	const char **argvec;

		close(fp);
		close(pipefd[1]);
		dup2(pipefd[0], 0);
		close(pipefd[0]);
		close(pipefd2[0]);
		dup2(pipefd2[1], 1);
		dup2(pipefd2[1], 2);
		close(pipefd2[1]);

		argenv=getenv("UUXFLAGS");
		if (!argenv)	argenv="";
		argenvcopy=courier_malloc(strlen(argenv)+1);
		strcpy(argenvcopy, argenv);
		nargs_needed=9+nreceipients;
			/* uux -p -z -a sender path!rmail -f sender (null) */

		for (s=argenvcopy; (s=strtok(s, " \t")) != 0; s=0)
			++nargs_needed;

		argvec=courier_malloc(nargs_needed * sizeof(char *));
		argvec[0]=UUX;
		argvec[1]="-p";
		nargs_needed=2;

		if (*p->sender)
		{
			argvec[nargs_needed++]="-z";
			argvec[nargs_needed++]="-a";
			argvec[nargs_needed++]=p->sender;
		}
		else
		{
			argvec[nargs_needed++]="-n";
		}

		strcpy(argenvcopy, argenv);
		for (s=argenvcopy; (s=strtok(s, " \t")) != 0; s=0)
			argvec[nargs_needed++]=s;

		s=malloc(strlen(p->host)+sizeof("!rmail"));

		if (!s)	clog_msg_errno();
		strcat(strcpy(s, p->host), "!rmail");
		argvec[nargs_needed++]=s;

		if (*p->sender)
		{
			argvec[nargs_needed++]="-f";
			argvec[nargs_needed++]=uucp_quote(p->sender);
		}

		for (i=0; i<nreceipients; i++)
		{
		const char *s=p->receipients[reciparray[i]*2+1];

			argvec[nargs_needed++]=uucp_quote(s);
		}
		argvec[nargs_needed]=0;

		execv(UUX, (char **)argvec);
		perror("exec");
		exit(1);
		return;
	}
	close(pipefd2[1]);
	close(pipefd[0]);

	uuxpipe=pipefd[1];
	uuxerr=pipefd2[0];

	errbufptr=errbuf;
	errbufleft=sizeof(errbuf)-1;

	adderrbuf("uux: ", 0);
	saveerrbuf=errbufptr;

	signal(SIGPIPE, SIG_IGN);

	uucprwinfo_s.delhostt=delhostt;
	uucprwinfo_s.rewrite_func= rewrite_func;

	j=rw_rewrite_msg(fp, &dowrite, &call_rewrite_func, &uucprwinfo_s);

	if (j < 0)
	{
		clog_msg_prerrno();
		kill(pid, SIGTERM);
		signal(SIGALRM, alarm_sig);
		while (readuuxerr() == 0)
			;
		while ((pid2=wait(&waitstat)) != pid)
			;

		close(uuxpipe);
		waitstat=1;
	}
	else
	{
		close(uuxpipe);

		while (readuuxerr() == 0)
			;

		while ((pid2=wait(&waitstat)) != pid)
			;
	}
	close(uuxerr);
	close(fp);

	*errbufptr=0;
	if (waitstat)
	{
		if (saveerrbuf == errbufptr)
			strcpy(errbuf,
				"uux terminated with a non-0 exit code.");
	}
	else
	{
		if (saveerrbuf == errbufptr)
			strcat(errbuf, " message accepted.");
	}

	last="";
	for (s=errbuf; (s=strtok(s, "\n")) != 0; s=0)
	{
		if (*last)
			for (i=0; i<nreceipients; i++)
			{
				ctlfile_append_info(ctf,
					(unsigned)atol(p->receipients[
					reciparray[i]*2]), last);
			}
		last=s;
	}

	for (i=0; i<nreceipients; i++)
	{
		ctlfile_append_reply(ctf,
			(unsigned)atol(p->receipients[reciparray[i]*2]),
			last,
			waitstat ? COMCTLFILE_DELFAIL_NOTRACK:
				     COMCTLFILE_DELSUCCESS,
			waitstat == 0 ? "r":0);
	}
}

static int dowrite(const char *p, unsigned l, void *arg)
{
	while (l)
	{
	fd_set	fdr, fdw;
	int	m;

		FD_ZERO(&fdr);
		FD_ZERO(&fdw);
		FD_SET(uuxerr, &fdr);
		FD_SET(uuxpipe, &fdw);

		m=uuxerr;
		if (uuxpipe > m)	m=uuxpipe;
		if (select(m, &fdr, &fdw, 0, 0) <= 0)
		{
			clog_msg_prerrno();
			continue;
		}

		if (FD_ISSET(uuxerr, &fdr))
			readuuxerr();
		if (FD_ISSET(uuxpipe, &fdw))
		{
		int	n;

			n=write(uuxpipe, p, l);
			if (n <= 0)	return (-1);
			p += n;
			l -= n;
		}
	}
	return (0);
}
