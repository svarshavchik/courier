/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier_lib_config.h"
#include	"courier.h"
#include	"comqueuename.h"
#include	"comctlfile.h"
#include	"comparseqid.h"
#include	"comctlfile.h"
#include	"commsgcancel.h"
#include	<sys/types.h>
#include	<sys/uio.h>
#include	<string.h>
#include	<signal.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if	HAVE_SYS_IOCTL_H
#include	<sys/ioctl.h>
#endif
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif

int msgcancel(const char *qid, const char **reason, int nreason,
	int chkuid)
{
ino_t	n;
struct	ctlfile ctf;
struct	stat	stat_buf;
uid_t	u=getuid();
int	c, d;
struct	iovec *iova;
static const char default_cancel_msg[]="Message cancelled.";
static const char *default_cancel_msgp[1]= { default_cancel_msg };

static const char x=COMCTLFILE_CANCEL_MSG;
char	*s;

	if (comparseqid(qid, &n))	return (-1);
        if (ctlfile_openi(n, &ctf, 0))	return (-1);
	if (fstat(ctf.fd, &stat_buf) || (chkuid && u && u != stat_buf.st_uid) ||
		ctf.cancelled)
	{
		ctlfile_close(&ctf);
		return (-1);
	}
	c=ctlfile_searchfirst(&ctf, COMCTLFILE_MSGID);
	if (c < 0 || strcmp(ctf.lines[c]+1, qid))
	{
		ctlfile_close(&ctf);
		return (-1);
	}

	if ((iova=(struct iovec *)malloc(sizeof(struct iovec) * 3 *
		(nreason == 0 ? 1:nreason))) == 0)
	{
		perror("enomem");
		exit(1);
	}

	if (nreason == 0)
	{
		reason=default_cancel_msgp;
		nreason=1;
	}

	c=0;
	while (nreason)
	{
		iova[c].iov_base=(caddr_t)&x;
		iova[c].iov_len=1;
		c++;
		iova[c].iov_base=(caddr_t) *reason;
		iova[c].iov_len=strlen( *reason );
		c++;
		iova[c].iov_base=(caddr_t)"\n";
		iova[c].iov_len=1;
		c++;
		++reason;
		--nreason;
	}

	s=malloc(sizeof(TRIGGER_FLUSHMSG)+strlen(qid)+3);
	if (!s)
	{
		perror("malloc");
		exit(1);
	}
	strcat(strcat(strcpy(s, TRIGGER_FLUSHMSG " "), qid), "\n");

	d=writev(ctf.fd, iova, c);
	ctlfile_close(&ctf);
	while (c)
	{
		--c;
		if (iova[c].iov_len > d)
		{
			perror("writev");
			free(iova);
			free(s);
			return (0);
		}
		d -= iova[c].iov_len;
	}
	free(iova);
	trigger(s);
	free(s);
	return (0);
}
