/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"bindir.h"
#include	"sbindir.h"
#include	"maxlongsize.h"
#include	"numlib/numlib.h"
#include	<sys/types.h>
#include	<pwd.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdlib.h>
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if     HAVE_SYSEXITS_H
#include        <sysexits.h>
#else
#define EX_NOUSER       67
#define EX_TEMPFAIL     75
#define EX_NOPERM       77
#endif


/*
	-bs flag to sendmail should invoke courieresmtpd, however courieresmtpd
	does not have world execute permissions (for a good reason).

	Therefore, we install sendmail-esmtpd-wrapper as a setuid root
	program.  sendmail-esmtpd-wrapper makes a note of who's running
	it, then sets its uid and gid to courier, then execs bin/courieresmtpd
	with a fresh environment.

	The fresh environment will have fake TCPREMOTEHOST and TCPREMOTEIP.
	Furthermore, TCPREMOTEINFO will be set to the identity of the
	invoking user.  This way, we can read the sender's identity in the
	headers.

	Also RELAYCLIENT is set, so we can relay :-)

	Additional, we run addcr to automatically add the CRs before the NLs,
	which courieresmtpd expects (!).
*/

void	esmtpd()
{
uid_t orig_uid=getuid();
char	ubuf[NUMBUFSIZE];
char	remoteinfobuf[NUMBUFSIZE+30];
char	*environ[6];
char	*argv[2];
int	pipefd[2];
pid_t	pid;

	sprintf(remoteinfobuf, "TCPREMOTEINFO=uid %s",
		libmail_str_uid_t(orig_uid, ubuf));

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		exit(EX_TEMPFAIL);
	}

	if (chdir(courierdir()) < 0)
	{
		perror("chdir");
		exit(EX_TEMPFAIL);
	}

	pid=fork();
	if (pid < 0)
	{
		perror("fork");
		exit(EX_TEMPFAIL);
	}
	if (pid == 0)
	{
		dup2(pipefd[1], 1);
		close(pipefd[0]);
		close(pipefd[1]);
		execl(BINDIR "/addcr", "addcr", (char *)0);
		perror("exec");
		exit(EX_TEMPFAIL);
	}
	dup2(pipefd[0], 0);
	close(pipefd[0]);
	close(pipefd[1]);
	close(2); open("/dev/null", O_WRONLY); /* Pine temper tantrum */

	environ[0]="TCPREMOTEIP=127.0.0.1";
	environ[1]="TCPREMOTEHOST=localhost";
	environ[2]=remoteinfobuf;
	environ[3]="RELAYCLIENT=";
	environ[4]="FAXRELAYCLIENT=";
	environ[5]=0;
	argv[0]="sendmail";
	argv[1]=0;
	execve(SBINDIR "/courieresmtpd", argv, environ);
	perror("exec");
	exit(EX_TEMPFAIL);
}
