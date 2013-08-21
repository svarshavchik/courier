/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	MAXLONGSIZE

#include	<limits.h>

#ifdef	ULLONG_MAX
#define	MAXLONGSIZE	MAXLONGSIZE1(ULLONG_MAX)
#else
#define	MAXLONGSIZE	MAXLONGSIZE1(ULONG_MAX)
#endif

#define	MAXLONGSIZE1(x)	MAXLONGSIZE2(x)
#define	MAXLONGSIZE2(x)	sizeof( # x )

#endif
