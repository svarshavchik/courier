/*
** Copyright 1998 - 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comctlfile.h"
#include	"comqueuename.h"
#include	"comstrtimestamp.h"
#include	"comtrack.h"
#include	"maxlongsize.h"
#include	<sys/types.h>
#include	<sys/uio.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>


int ctlfile_openi(ino_t i, struct ctlfile *p, int ro)
{
	return (ctlfile_openfn(qmsgsctlname(i), p, ro, 1));
}

int ctlfile_openit(ino_t i, time_t t, struct ctlfile *p, int ro)
{
	return (ctlfile_openfn(qmsgqname(i, t), p, ro, 1));
}

int ctlfile_openfn(const char *n, struct ctlfile *p, int ro, int chk)
{
struct	stat	stat_buf;
unsigned nlines=2, nreceipients=1;
char	*cp;
 unsigned i, j, rcnt, orcnt, dsncnt;
char	*q;
static char deferred_status[2]={COMCTLFILE_DELDEFERRED, 0};

	p->cancelled=0;
	if ((p->fd=open(n, ro ? O_RDONLY:O_RDWR | O_APPEND)) < 0)
		return (-1);

	p->msgsize=0;

#ifdef	FD_CLOEXEC
	fcntl(p->fd, F_SETFD, FD_CLOEXEC);
#endif
	if (fstat(p->fd, &stat_buf) < 0)
	{
		close (p->fd);
		return (-1);
	}

	if (chk && stat_buf.st_nlink != 2)
	{
		if (ro)
		{
			close(p->fd);
			errno=0;
			return (-1);
		}

		if (stat_buf.st_nlink == 1)
			/*
			** Happens all the time - delivery complete, hanging
			** link
			*/
		{
			unlink(n);
			close(p->fd);
			errno=0;
			return (-1);
		}
		clog_msg_start_err();
		clog_msg_str("Invalid number of links for ");
		clog_msg_str(n);
		clog_msg_send();
		close(p->fd);
		errno=EINVAL;
		return (-1);
	}
	p->contents=q=courier_malloc(stat_buf.st_size+1);

	i=stat_buf.st_size;
	while (i)
	{
		j=read(p->fd, q, i);
		if (j <= 0)
		{
			close(p->fd);
			free(p->contents);
			return (-1);
		}
		q += j;
		i -= j;
	}
	*q=0;

	p->n=stat_buf.st_ino;
	p->mtime=stat_buf.st_mtime;
	p->starttime=0;
	for (cp=p->contents; *cp; cp++)
	{
		if (*cp == '\r')
			*cp=' ';
		if (*cp == '\n')	++nlines;
	}
	p->lines=(char **)courier_malloc(nlines * sizeof(char *));

	for (nlines=0, cp=p->contents; (cp=strtok(cp, "\n")) != 0; cp=0)
	{
		p->lines[nlines++]=cp;
		if (*cp == COMCTLFILE_RECEIPIENT)	++nreceipients;
		if (*cp == COMCTLFILE_CANCEL_MSG)	p->cancelled=1;
	}
	p->lines[nlines]=0;

	p->sender="";
	p->receipients=(char **)courier_malloc(nreceipients*sizeof(char *));
	p->oreceipients=(char **)courier_malloc(nreceipients*sizeof(char *));
	p->dsnreceipients=(char **)courier_malloc(nreceipients*sizeof(char *));
	p->delstatus=(char **)courier_malloc(nreceipients*sizeof(char *));

	for (i=0; i<nreceipients; i++)
	{
		p->receipients[i]=p->oreceipients[i]=p->dsnreceipients[i]=0;
		p->delstatus[i]=deferred_status;
	}

	for (i=0, rcnt=0, orcnt=0, dsncnt=0; p->lines[i]; i++)
	{
		switch (p->lines[i][0])	{
		case COMCTLFILE_SENDER:
			p->sender=p->lines[i]+1;
			break;
		case COMCTLFILE_RECEIPIENT:
			p->receipients[rcnt++]=p->lines[i]+1;
			break;
		case COMCTLFILE_ORECEIPIENT:
			if (orcnt < nreceipients)
				p->oreceipients[orcnt++]=p->lines[i]+1;
			break;
		case COMCTLFILE_DSN:
			if (dsncnt < nreceipients)
				p->dsnreceipients[dsncnt++]=p->lines[i]+1;
			break;
		case COMCTLFILE_DELSUCCESS:
		case COMCTLFILE_DELFAIL:
			j=atoi(p->lines[i]+1);
			if (j < nreceipients)
				p->delstatus[j]=p->lines[i];
			break;
		}
	}
	p->nreceipients=rcnt;

	return (0);
}

void ctlfile_close(struct ctlfile *p)
{
	free( p->oreceipients );
	free( p->dsnreceipients );
	free( p->receipients );
	free( p->lines );
	free( p->contents );
	free( p->delstatus );

#if	EXPLICITSYNC
	fsync(p->fd);
#endif
	close(p->fd);
}

int ctlfile_searchfirst(struct ctlfile *p, char c)
{
int	i;

	for (i=0; p->lines[i]; i++)
		if (p->lines[i][0] == c)	return (i);
	return (-1);
}

static void notatomic()
{
	clog_msg_start_err();
	clog_msg_str("Write to control file FAILED - not atomic.");
	clog_msg_send();
	exit(1);
}

void ctlfile_append(struct ctlfile *p, const char *q)
{
int	l=strlen(q);

	if (write(p->fd, q, l) != l)	notatomic();
}

void ctlfile_appendv(struct ctlfile *p, const struct iovec *iov, int iovcnt)
{
	ctlfile_appendvfd(p->fd, iov, iovcnt);
}

void ctlfile_appendvfd(int fd, const struct iovec *iov, int iovcnt)
{
int	l=0;
int	i;

	for (i=0; i<iovcnt; i++)
		l += iov[i].iov_len;

	if (writev(fd, iov, iovcnt) != l)	notatomic();
}

void ctlfile_append_info(struct ctlfile *cf, unsigned rcptnum, const char *info)
{
	ctlfile_append_connectioninfo(cf, rcptnum, COMCTLFILE_DELINFO_REPLY,
		info);
}

void ctlfile_append_connectioninfo(struct ctlfile *cf,
	unsigned rcptnum, char infotype, const char *info)
{
	if (infotype == COMCTLFILE_DELINFO_CONNECTIONERROR
		|| infotype == COMCTLFILE_DELINFO_REPLY)
	{
		clog_msg_start_info();
		clog_msg_str("id=");
		clog_msg_msgid(cf);
		clog_msg_str(",from=<");
		clog_msg_str(cf->sender);
		clog_msg_str(">,addr=<");
		clog_msg_str(cf->receipients[rcptnum]);
		clog_msg_str(">: ");
		clog_msg_str(info);
		clog_msg_send();
	}
	ctlfile_append_connectioninfofd(cf->fd, rcptnum, infotype, info);
}

void ctlfile_append_infofd(int fd, unsigned rcptnum, const char *info)
{
	ctlfile_append_connectioninfofd(fd, rcptnum, COMCTLFILE_DELINFO_REPLY,
		info);
}

void ctlfile_append_connectioninfofd(int fd, unsigned rcptnum,
		char infotype, const char *info)
{
char	fmtbuf[MAXLONGSIZE+10];
struct iovec iov[4];
char	buf2[2];

	sprintf(fmtbuf, "%c%u ", COMCTLFILE_DELINFO, rcptnum);
	iov[0].iov_base=(caddr_t)fmtbuf;
	iov[0].iov_len=strlen(fmtbuf);

	buf2[0]=infotype;
	buf2[1]=' ';
	iov[1].iov_base=(caddr_t)buf2;
	iov[1].iov_len=2;
	iov[3].iov_base=(caddr_t)"\n";
	iov[3].iov_len=1;

	while (info && *info)
	{
	unsigned	i;

		for (i=0; info[i] && info[i] != '\n'; i++)
			;

		iov[2].iov_base=(caddr_t)info;
		iov[2].iov_len=i;

		if (i && info[i-1] == '\r')	--iov[2].iov_len;
		ctlfile_appendvfd(fd, iov, 4);
		info += i;
		if (*info)	++info;
	}
}

/* Record delivery completion status into the log file */

static void ctlfile_append_msgsize(struct ctlfile *);

void ctlfile_append_reply(struct ctlfile *cf, unsigned rcptnum,
	const char *errmsg, char type, const char *delextra)
{
	char type2;

	if (errmsg && *errmsg)
	{
		clog_msg_start_info();
		clog_msg_str("id=");
		clog_msg_msgid(cf);
		clog_msg_str(",from=<");
		clog_msg_str(cf->sender);
		clog_msg_str(">,addr=<");
		clog_msg_str(cf->receipients[rcptnum]);
		clog_msg_str(">");
		if (type == COMCTLFILE_DELSUCCESS
		    || type == COMCTLFILE_DELSUCCESS_NOLOG)
		{
			if (cf->msgsize != 0)
				ctlfile_append_msgsize(cf);
			clog_msg_str(",success");
		}
		clog_msg_str(": ");
		clog_msg_str(errmsg);
		clog_msg_send();
		if (type == COMCTLFILE_DELSUCCESS)
			goto ugly_hack;
	}
	clog_msg_start_info();
	clog_msg_str("id=");
	clog_msg_msgid(cf);
	clog_msg_str(",from=<");
	clog_msg_str(cf->sender);
	clog_msg_str(">,addr=<");
	clog_msg_str(cf->receipients[rcptnum]);
	clog_msg_str(">");

	if ((type == COMCTLFILE_DELSUCCESS
	     || type == COMCTLFILE_DELSUCCESS_NOLOG)
	    && cf->msgsize != 0)
		ctlfile_append_msgsize(cf);

	clog_msg_str(type == COMCTLFILE_DELSUCCESS ||
		     type == COMCTLFILE_DELSUCCESS_NOLOG
		     ? ",status: success":
		     type == COMCTLFILE_DELFAIL ||
		     type == COMCTLFILE_DELFAIL_NOTRACK ? ",status: failure":
		     ",status: deferred");
	clog_msg_send();

ugly_hack:

	if (type == COMCTLFILE_DELSUCCESS_NOLOG)
	{
		type=COMCTLFILE_DELSUCCESS;
		errmsg=0;
	}

	type2=type;

	if (type2 == COMCTLFILE_DELFAIL_NOTRACK)
		type2=COMCTLFILE_DELFAIL;

	ctlfile_append_replyfd(cf->fd, rcptnum, errmsg, type2, delextra);

	if (rcptnum < cf->nreceipients && /* Sanity check */
	    ctlfile_searchfirst(cf, COMCTLFILE_TRACK) >= 0)
	{
		time_t timestamp;
		int results;

		/* A succesfull delivery marks the address as deliverable,
		** if previously it was marked as failed.
		*/

		results=track_find_email(cf->receipients[rcptnum], &timestamp);

		switch (type) {
		case COMCTLFILE_DELFAIL_NOTRACK:
			break;

		case COMCTLFILE_DELSUCCESS:
			if (results == TRACK_ADDRDEFERRED ||
			    results == TRACK_ADDRFAILED)
				track_save_email(cf->receipients[rcptnum],
						 TRACK_ADDRACCEPTED);
			break;

		case COMCTLFILE_DELFAIL:
		case COMCTLFILE_DELDEFERRED:

			if (results == TRACK_ADDRDEFERRED ||
			    results == TRACK_ADDRFAILED)
			{
				if (timestamp >= time(NULL) -
				    (TRACK_NHOURS * 60 * 60)/2)
					break;
				/* If already had a failed attempt within
				** half the tracking window, no need to
				** add another record.
				*/
			}
			track_save_email(cf->receipients[rcptnum],
					 type == COMCTLFILE_DELFAIL ?
					 TRACK_ADDRFAILED:
					 TRACK_ADDRDEFERRED);
			break;
		default:
			break;
		}
	}
}

static void ctlfile_append_msgsize(struct ctlfile *cf)
{
	clog_msg_str(",size=");
	clog_msg_ulong(cf->msgsize);
}

void ctlfile_append_replyfd(int fd, unsigned rcptnum,
	const char *errmsg, char type, const char *delextra)
{
const char *cp;
time_t	timestamp;
char	numbuf[1+MAXLONGSIZE];
int	numbuflen;
struct iovec iov[12];
int	n;
static const char delinfo=COMCTLFILE_DELINFO;
char	replybuf[2];

	time(&timestamp);

	sprintf(numbuf, "%u ", rcptnum);
	numbuflen=strlen(numbuf);

	replybuf[0]=COMCTLFILE_DELINFO_REPLY;
	replybuf[1]=' ';

	/*
	** errmsg can a multiline response.  Each line is logged.  The
	** last line of the response is logged together with the final
	** success/fail/deferred entry, so in most cases where there's
	** a one-line response, this results in only one write call.
	*/

	n=0;

	while (errmsg && *errmsg)
	{
		for (cp=errmsg; *cp && *cp != '\n'; cp++)
			;

		iov[0].iov_base=(caddr_t)&delinfo;
		iov[0].iov_len=1;
		iov[1].iov_base=(caddr_t)numbuf;
		iov[1].iov_len=numbuflen;

		iov[2].iov_base=(caddr_t)replybuf;
		iov[2].iov_len=2;

		iov[3].iov_base=(caddr_t)errmsg;

		/* Fix cruft like CRLF, or messages without trailing LFs */

		if (cp > errmsg && cp[-1] == '\r')
		{
			iov[3].iov_len= cp - errmsg - 1;
			iov[4].iov_base=(caddr_t)"\n";
			iov[4].iov_len=1;
			n=5;
		}
		else if (*cp == '\n')
		{
			iov[3].iov_len= cp - errmsg + 1;
			n=4;
		}
		else
		{
			iov[3].iov_len= cp - errmsg;
			iov[4].iov_base=(caddr_t)"\n";
			iov[4].iov_len=1;
			n=5;
		}
		if (*cp == '\n')	++cp;
		errmsg=cp;
		if (*errmsg == '\0')
			break;	/* Last line, attach it to status record */

		ctlfile_appendvfd(fd, iov, n);
		n=0;
	}

	iov[n].iov_base= (caddr_t)&type;
	iov[n].iov_len=1;
	++n;
	iov[n].iov_base= (caddr_t)numbuf;
	iov[n].iov_len= numbuflen;
	++n;
	cp=strtimestamp(timestamp);
	iov[n].iov_base=(caddr_t)cp;
	iov[n].iov_len=strlen(cp);
	++n;

	if (delextra)
	{
		iov[n].iov_base=(caddr_t)delextra;
		iov[n].iov_len=strlen(delextra);
		++n;
	}

	iov[n].iov_base=(caddr_t)"\n";
	iov[n].iov_len=1;
	++n;
	ctlfile_appendvfd(fd, iov, n);
}

void ctlfile_nextattempt(struct ctlfile *ctf, time_t t)
{
	ctlfile_nextattemptfd(ctf->fd, t);
}


void ctlfile_nextattemptfd(int fd, time_t t)
{
const char *cp=strtimestamp(t);
struct iovec iov[3];
static const char a=COMCTLFILE_NEXTATTEMPT;

	iov[0].iov_base=(caddr_t)&a;
	iov[0].iov_len=1;
	iov[1].iov_base=(caddr_t)cp;
	iov[1].iov_len=strlen(cp);
	iov[2].iov_base="\n";
	iov[2].iov_len=1;

	ctlfile_appendvfd(fd, iov, 3);
#if	EXPLICITSYNC
	fsync(fd);
#endif
}

time_t ctlfile_getnextattempt(struct ctlfile *c)
{
unsigned i;
time_t	t;
const char *cp;
struct	stat	stat_buf;

	for (i=0; c->lines[i]; i++)
		;

	while (i)
	{
		if (c->lines[--i][0] != COMCTLFILE_NEXTATTEMPT)
			continue;
		for (t=0, cp=c->lines[i]+1; *cp >= '0' && *cp <= '9'; ++cp)
			t=t*10 + (*cp-'0');
		if (stat(qmsgqname(c->n, t), &stat_buf) == 0)
			return (t);
	}
	return (0);
}

int ctlfile_setvhost(struct ctlfile *c)
{
	/* The buffer for the currently-set vhost */
	static char *previous_vhost_buf=0;
	const char *current_vhost=config_get_local_vhost();

	/* Set vhost to the given message's vhost */

	char *vhost=0;
	int n=ctlfile_searchfirst(c, COMCTLFILE_VHOST);
	int res;

	if (n >= 0)
	{
		const char *p=c->lines[n]+1;

		vhost=strcpy(courier_malloc(strlen(p)+1), p);
	}

	/* Compare new vhost against the previous one */

	res=strcmp((current_vhost ? current_vhost:""),
		   (vhost ? vhost:""));

	/*
	** Set the current vhost from the control file, save it in the vhost
	** buffer, free any previously-set vhost.
	*/

	config_set_local_vhost(vhost);

	if (previous_vhost_buf)
		free(previous_vhost_buf);
	previous_vhost_buf=vhost;

	return res;
}
