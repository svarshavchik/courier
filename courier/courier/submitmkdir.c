/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"localstatedir.h"
#include	<stdlib.h>
#include	<stdio.h>
#if	HAVE_SYS_TYPES_H
#include	<sys/types.h>
#endif
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

int main(int argc, char **argv)
{
	umask(007);
	if (chdir(courierdir()) == 0 && chdir(TMPDIR) == 0 && argc > 1)
	{
	const char *p=argv[1];

		for ( ; *p; p++)
			if (*p < '0' || *p > '9')	exit(0);
		mkdir(argv[1], 0770);
	}
	exit(0);
}
