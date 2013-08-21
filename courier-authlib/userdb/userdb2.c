/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"dbobj.h"
#include	"userdb.h"
#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<errno.h>


extern int userdb_debug_level;

char	*userdbshadow(const char *sh, const char *u)
{
struct dbobj d;
char	*p,*q;
size_t	l;

	dbobj_init(&d);

	if (dbobj_open(&d, sh, "R"))
	{
		if (userdb_debug_level)
			fprintf(stderr,
				"DEBUG: userdbshadow: unable to open %s\n", sh);
		return (0);
	}

	q=dbobj_fetch(&d, u, strlen(u), &l, "");
	dbobj_close(&d);
	if (!q)
	{
		if (userdb_debug_level)
			fprintf(stderr,
				"DEBUG: userdbshadow: entry not found\n");
		errno=ENOENT;
		return(0);
	}

	p=malloc(l+1);
	if (!p)
	{
		free(q);
		return (0);
	}

	if (l)	memcpy(p, q, l);
	free(q);
	p[l]=0;
	return (p);
}
