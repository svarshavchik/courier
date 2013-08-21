/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"comparseqid.h"
#include	<string.h>

static const char hex[]="0123456789ABCDEF";

int comparseqid(const char *qid, ino_t *n)
{
const	char *p;

	*n=0;
	for ( ; *qid; qid++)
	{
		if (*qid == '.')	return (0);

		if ((p=strchr(hex, *qid)) == 0)	return (-1);
		*n= (*n << 4) + (p-hex);
	}
	return (-1);
}
