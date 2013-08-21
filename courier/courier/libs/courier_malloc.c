/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>

void *courier_malloc(unsigned n)
{
void *p=malloc(n);

	if (!p)	clog_msg_errno();
	return (p);
}
