/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"

#include	<stdlib.h>


char *dsnencodeorigaddr(const char *address)
{
int l=0;
char *p=0, *q=0;
const char *r;
int pass;

static const char x[16]="0123456789ABCDEF";

	for (pass=0; pass<2; pass++)
	{
		if (pass)	p=q=courier_malloc(l);
		l=1;

		for (r="rfc822;"; *r; r++)
		{
			if (pass)	*q++= *r;
			++l;
		}

		for (r=address; *r; r++)
		{
			if ( *r < '!' || *r > '~' || *r == '+' || *r == '=')
			{
				if (pass)
				{
					*q++='+';
					*q++= x[ (int) (((*r) >> 4) & 0x0F) ];
					*q++= x[ (int)((*r) & 0x0F) ];
				}
				l += 3;
			}
			else
			{
				if (pass) *q++= *r;
				++l;
			}
		}
	}
	*q=0;
	return (p);
}

