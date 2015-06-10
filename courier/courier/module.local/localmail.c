/*
** Copyright 1998 - 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<signal.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<pwd.h>
#include	<grp.h>
#include	<courierauth.h>
#if TIME_WITH_SYS_TIME
#include	<sys/time.h>
#include	<time.h>
#else
#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#else
#include	<time.h>
#endif
#endif
#include	"courier.h"
#include	"libexecdir.h"
#include	"comctlfile.h"
#include	"comqueuename.h"
#include	"comsubmitclient.h"
#include	"moduledel.h"
#include	"comreadtime.h"
#include	"waitlib/waitlib.h"
#include	"numlib/numlib.h"
#include	"comverp.h"

#if	HAVE_SYS_FILE_H
#include	<sys/file.h>
#endif

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

#if HAVE_SYSEXITS_H
#include	<sysexits.h>
#else
#define	EX_TEMPFAIL	75
#define	EX_NOUSER	67
#define	EX_CANTCREAT	73
#define	EX_SOFTWARE	70
#endif

static const char *courier_home;

static void wait_delivery(pid_t, struct ctlfile *, unsigned, int, int,
	const char *, const char *);

int main(int argc, char **argv)
{
struct moduledel *p;

	clog_open_syslog("courierlocal");

	if (chdir(courier_home=getenv("COURIER_HOME")))
		clog_msg_errno();

	if (atol(getenv("MAXRCPT")) > 1)
	{
		clog_msg_start_err();
		clog_msg_str("Invalid configuration in config, set MAXRCPT=1");
		clog_msg_send();
		exit(1);
	}

	module_init(0);

	module_delivery_timeout(config_readtime("localtimeout", 15*60));

	while ((p=module_getdel()) != NULL)
	{
	unsigned delid=atol(p->delid);
	unsigned rcptnum;
	struct ctlfile ctf;
	int	pipe1[2], pipe2[2];
	pid_t	pid;
	int datfd;
	struct stat stat_buf;

		pid=module_fork(delid, 0);
		if (pid < 0)
		{
			clog_msg_prerrno();
			module_completed(delid, delid);
			continue;
		}

		if (pid)	continue;

#if     HAVE_SETPGRP
#if     SETPGRP_VOID
		setpgrp();
#else
		setpgrp(0, 0);
#endif
#endif

		if (p->nreceipients != 1)
		{
			clog_msg_start_err();
			clog_msg_str("Invalid message from courierd.");
			clog_msg_send();
			_exit(0);
		}

		rcptnum=atol(p->receipients[0]);
		if (ctlfile_openi(p->inum, &ctf, 0))
		{
			clog_msg_errno();
			_exit(0);
		}
		ctlfile_setvhost(&ctf);

		if (pipe(pipe1) < 0 || pipe(pipe2) < 0
		    || (pid=fork()) < 0)
		{
			clog_msg_prerrno();
			ctlfile_append_reply(&ctf, rcptnum,
				"Can't run courierdeliver.",
				COMCTLFILE_DELDEFERRED, 0);
			ctlfile_close(&ctf);
			_exit(0);
			return (0);
		}

		if ((datfd=open(qmsgsdatname(p->inum), O_RDONLY)) < 0)
		{
			clog_msg_prerrno();
			ctlfile_append_reply(&ctf, rcptnum,
					     "Unable to read message file.",
					     COMCTLFILE_DELDEFERRED, 0);
			ctlfile_close(&ctf);
			_exit(0);
			return (0);
		}

		if (pid == 0)
		{
		const char *host, *homedir, *maildir, *quota;
		char	*buf, *s;

		char	*username, *ext;
		uid_t	u;
		gid_t	g;

			close(pipe1[0]);
			close(pipe2[0]);
			dup2(pipe2[1], 2);
			close(pipe2[1]);
			dup2(pipe1[1], 1);
			close(pipe1[1]);
			close(0);
			if (dup(datfd) != 0)
			{
				clog_msg_start_err();
				clog_msg_str("Unable to read message file.");
				clog_msg_send();
				_exit(EX_TEMPFAIL);
			}
			close(ctf.fd);
			close(datfd);

			/* Contents of host: */
			/* acct!ext!uid!gid!homedir!maildir!quota */

			host=p->host;

			buf=strdup(host);
			if (!buf)
			{
				clog_msg_prerrno();
				_exit(EX_TEMPFAIL);
			}

			s=strchr(buf, '!');
			username=buf;
			if (s)	*s++=0;
			ext=s;
			if (s)	s=strchr(s, '!');
			if (s)	*s++=0;

			u=0;
			while (s && *s != '!')
			{
				if (isdigit((int)(unsigned char)*s))
					u=u*10 + (*s - '0');
				++s;
			}
			if (s)	*s++=0;
			g=0;
			while (s && *s != '!')
			{
				if (isdigit((int)(unsigned char)*s))
					g=g*10 + (*s - '0');
				++s;
			}

			if (s)	*s++=0;

			homedir=s;

			if (s)	s=strchr(s, '!');
			if (s)	*s++=0;

			maildir=s;

			if (s)	s=strchr(s, '!');
			if (s)	*s++=0;

			quota=s;

			if (!s)
			{
				clog_msg_start_err();
				clog_msg_str("Invalid local recipient address.");
				clog_msg_send();
				_exit(EX_TEMPFAIL);
			}

			{
				struct authinfo ainfo;

				memset(&ainfo, 0, sizeof(ainfo));

				ainfo.address="-";
				ainfo.sysuserid=&u;
				ainfo.sysgroupid=g;
				ainfo.homedir=homedir;

				if (auth_mkhomedir(&ainfo))
				{
					clog_msg_str(homedir);
					clog_msg_str(": ");
					clog_msg_prerrno();
					_exit(EX_TEMPFAIL);
				}
			}

			if (chdir(homedir))
			{
				clog_msg_str(homedir);
				clog_msg_str(": ");
				clog_msg_prerrno();
				_exit(EX_TEMPFAIL);
			}

			libmail_changeuidgid(u, g);

			execl( MODULEDIR "/local/courierdeliver",
				"courierdeliver",
				username,
				homedir,
				ext,
				ctf.receipients[rcptnum],
				verp_getsender(&ctf, ctf.receipients[rcptnum]),
				quota,
				maildir,
				(const char *)0);

			clog_msg_start_err();
			clog_msg_prerrno();
			clog_msg_send();
			_exit(EX_TEMPFAIL);
		}

		close(pipe1[1]);
		close(pipe2[1]);

		libmail_changeuidgid(MAILUID, MAILGID);
		if (fstat(datfd, &stat_buf) == 0)
			ctf.msgsize=stat_buf.st_size;

		close(datfd);
		wait_delivery(pid, &ctf, rcptnum, pipe2[0], pipe1[0],
				p->host, p->receipients[1]);
		ctlfile_close(&ctf);
		_exit(0);
	}
	return (0);
}

static char msgbuf[4096];
static char fwdbuf[1024];
static char errbuf[1024];
static char *errptr;
static size_t errleft;

unsigned msglen, fwdlen;


static void save_submit_errmsg(const char *p)
{
	size_t l=strlen(p ? p:"");

	if (l > errleft)
		l=errleft;

	if (l > 0)
	{
		strcpy(errptr, p);
		errptr += l;
		errleft -= l;
	}
}

static void savemsg(const char *p, unsigned l)
{
	if (l > sizeof(msgbuf)-1-msglen)
		l=sizeof(msgbuf)-1-msglen;

	if (l)	memcpy(msgbuf+msglen, p, l);
	msglen += l;
}

int	submit_started=0, submit_err=0;

static void dofwd(const char *addr, const char *from, const char *fromuser,
	const char *origuser)
{
	if (!submit_started)
	{
	char	*args[5];

	static const char *envvars[]={
		"DSNNOTIFY",
		"DSNRET",
		"NOADDDATE",
		"NOADDMSGID",
		"NOADDRREWRITE",
		"MIME",
		0};

	char	*envs[sizeof(envvars)/sizeof(envvars[0])];
	int	i, j;

		for (i=j=0; envvars[i]; i++)
		{
		const char *p=getenv(envvars[i]);
		char	*q;

			if (!p)	continue;
			q=strcat(strcat(strcpy(courier_malloc(
					strlen(envvars[i])+strlen(p)+2),
					envvars[i]), "="), p);
			envs[j]=q;
			j++;
		}
		envs[j]=0;


		args[0]="submit";
		args[1]="local";
		args[2]="dns; localhost (localhost [127.0.0.1])";
		args[3]=strcat(strcpy(courier_malloc(
				sizeof("forwarded by ")+strlen(fromuser)),
					"forwarded by "), fromuser);
		args[4]=0;

		errptr=errbuf;
		errleft=sizeof(errbuf)-1;

		save_submit_errmsg("The following error occured when trying to forward this message: \n");
		strcpy(errptr, "UNKNOWN ERROR - no further description is available");

		if (submit_fork(args, envs, save_submit_errmsg) ||
			(submit_write_message(from), submit_readrcprinterr()))
		{
			submit_err=1;
			return;
		}
		while (j)
		{
			free(envs[--j]);
		}
		submit_err=0;
		submit_started=1;
	}

	if (!submit_err)
	{
	char	*p;

		p=strcat(strcat(strcpy(courier_malloc(
			strlen(addr)+strlen(origuser)+4), addr),
				"\tF\t"), origuser);
		submit_write_message(p);
		free(p);
		if (submit_readrcprinterr())
		{
			submit_err=1;
		}
	}
}

static void fwdmsg(const char *p, unsigned l, const char *from,
	const char *fwduser, const char *origuser)
{
	while (l)
	{
		if (fwdlen < sizeof(fwdbuf))
			fwdbuf[fwdlen++]= *p;
		if (*p == '\n')
		{
			fwdbuf[--fwdlen]=0;
			dofwd(fwdbuf, from, fwduser, origuser);
			fwdlen=0;
		}
		++p;
		--l;
	}
}


static void wait_delivery(pid_t pid,
	struct ctlfile *ctf, unsigned rcptnum, int msgpipe, int fwdpipe,
	const char *user, const char *ext)
{
fd_set	fds;
int	maxfd= msgpipe > fwdpipe ? msgpipe:fwdpipe;
char	buf[BUFSIZ];
int	waitstat;
char	*sender=verp_getsender(ctf, ctf->receipients[rcptnum]);

const	char *oreceipient;
char	*oreceipients=0;

	if ( ctf->oreceipients[rcptnum] == 0 ||
			ctf->oreceipients[rcptnum][0])
	{
		oreceipient=ctf->oreceipients[rcptnum];
	}
	else
	{
		oreceipient=oreceipients=dsnencodeorigaddr(
			ctf->receipients[rcptnum]);
	}

	++maxfd;
	msglen=0;
	fwdlen=0;
	while (msgpipe >= 0 && fwdpipe >= 0)
	{
	int	l;

		FD_ZERO(&fds);
		if (msgpipe >= 0)
			FD_SET(msgpipe, &fds);
		if (fwdpipe >= 0)
			FD_SET(fwdpipe, &fds);

		if (select(maxfd, &fds, 0, 0, 0) < 0)
		{
			clog_msg_prerrno();
			continue;
		}

		if (msgpipe >= 0 && FD_ISSET(msgpipe, &fds))
		{
			if ( (l=read(msgpipe, buf, sizeof(buf))) <= 0)
			{
				close(msgpipe);
				msgpipe=-1;
			}
			else
				savemsg(buf, l);
		}
		if (fwdpipe >= 0 && FD_ISSET(fwdpipe, &fds))
		{
			if ( (l=read(fwdpipe, buf, sizeof(buf))) <= 0)
			{
				close(fwdpipe);
				fwdpipe=-1;
			}
			else
				fwdmsg(buf, l, sender,
					ctf->receipients[rcptnum],
					oreceipient);
		}
	}

	free(sender);
	while ( wait(&waitstat) != pid)
		;

	if (msglen)
	{
	char	*p, *q;

		msgbuf[msglen]=0;
		for (p=msgbuf; *p; )
		{
			for (q=p; *q; )
			{
				if (*q == '\n')
				{
					*q++=0;
					break;
				}
				q++;
			}
			ctlfile_append_info(ctf, rcptnum, p);
			p=q;
		}
	}

	if (submit_started && !submit_err)
	{
	FILE	*f=fopen(qmsgsdatname(ctf->n), "r");

		if (!f)
			submit_err=1;
		else
		{
		int	c;

			fprintf(submit_to, "\nDelivered-To: %s\n",
				ctf->receipients[rcptnum]);
			while ((c=getc(f)) != EOF)
				putc(c, submit_to);
			if (ferror(f) || fflush(submit_to) || fclose(submit_to))
			{
				submit_cancel();
				submit_err=1;
			}
			else if (submit_readrcprinterr())
			{
				submit_wait();
				submit_err=1;
			}
			else if (submit_wait())
				submit_err=1;
			fclose(f);
		}
	}

	if (WIFEXITED(waitstat))
		switch (WEXITSTATUS(waitstat))	{
		case 0:
		case 99:
			if (submit_err)
			{
				ctlfile_append_reply(ctf, rcptnum,
						     errbuf,
						     COMCTLFILE_DELFAIL, 0);
			}
			else
			{
				ctlfile_append_reply(ctf, rcptnum,
					"Message delivered.",
					COMCTLFILE_DELSUCCESS, " l");
			}
			if (oreceipients)
				free (oreceipients);
			return;
		case EX_SOFTWARE:
			ctlfile_append_reply(ctf, rcptnum,
					"", COMCTLFILE_DELFAIL_NOTRACK, 0);
			if (oreceipients)
				free (oreceipients);
			return;


		case 64:
		case 65:
		case 67:
		case 68:
		case 69:
		case 76:
		case 77:
		case 78:
		case 100:
		case 112:
			break;
		default:
			ctlfile_append_reply(ctf, rcptnum,
					"", COMCTLFILE_DELDEFERRED, 0);
			if (oreceipients)
				free (oreceipients);
			return;
		}

	ctlfile_append_reply(ctf, rcptnum, "", COMCTLFILE_DELFAIL, 0);
	if (oreceipients)
		free (oreceipients);
}
