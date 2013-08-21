/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comqueuename.h"
#include	"commsgcancel.h"
#include	"comctlfile.h"
#include	"comparseqid.h"
#include	"comctlfile.h"
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

/*
**	NOTE - this program must be setuid and setgid to daemon.
**	setuid is required - queue file is read-only by owner.
*/

int main(int argc, char **argv)
{
const	char *qid;

	if (chdir(courierdir()))
	{
		perror("chdir");
		return (1);
	}
	if (argc < 2)	return (0);

	qid=argv[1];
	if (msgcancel(qid, (const char **)argv+2, argc-2, 1))
	{
		fprintf(stderr, "Message not found.\n");
		exit(1);
	}
	printf("Message scheduled for cancellation.\n");
	return (0);
}
