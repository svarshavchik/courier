/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"comstrinode.h"
#include	"maxlongsize.h"

const char *strinode(ino_t n)
{
static char buf[MAXLONGSIZE];
char *p=buf+sizeof(buf)-1;

	*p=0;
	do
	{
		*--p='0' + (n % 10);
		n=n / 10;
	} while (n);
	return (p);
}

