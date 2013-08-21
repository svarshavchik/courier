/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdio.h>
#include	<string.h>

void submit_print_stdout(const char *p)
{
	if (p)
	{
		if (fwrite(p, strlen(p), 1, stdout) != 1)
			; /* muzzle gcc warning */
	}
	else	fflush(stdout);
}
