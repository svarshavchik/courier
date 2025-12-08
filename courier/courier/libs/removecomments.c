/*
** Copyright 1998 - 1999 S. Varshavchik.
** See COPYING for distribution information.
*/

/* Remove #+comments from file read into memory. */

void removecomments(char *p)
{
char *q;

	for (q=p; *p; )
	{
		if (*p == '#')
			while (*p && *p != '\n')
				p++;
		*q++= *p;
		if (*p)	p++;
	}
	*q=0;
}
