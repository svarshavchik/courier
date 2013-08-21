/*
** Copyright 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comfax_h
#define	comfax_h

#include	"courier.h"
#include	<stdlib.h>
#include	<ctype.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define FAX_LOWRES	1
#define FAX_OKUNKNOWN	2
#define	FAX_COVERONLY	4

int comgetfaxopts(const char *, int *);
int comgetfaxoptsn(const char *, int, int *);

#ifdef	__cplusplus
}
#endif

#endif
