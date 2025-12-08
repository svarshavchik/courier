/*
** Copyright 1998 - 2018 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>

void *courier_malloc(unsigned n)
{
	void *p=malloc(n);

	if (!p)	clog_msg_errno();
	return (p);
}

char *courier_strdup(const char *c)
{
	char *s=strdup(c);

	if (!s)	clog_msg_errno();
	return (s);
}
