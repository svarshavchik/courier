/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"comstrtotime.h"
#include	<ctype.h>

time_t strtotime(const char *p)
{
time_t	n=0;

	while (isdigit((int)(unsigned char)*p))
	{
		n *= 10;
		n += *p++ - '0';
	}
	return (n);
}
