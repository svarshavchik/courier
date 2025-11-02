/*
** Copyright 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comfax_h
#define	comfax_h

#include	"courier.h"
#include	<string_view>
#include	<stdlib.h>
#include	<ctype.h>

#define FAX_LOWRES	1
#define FAX_OKUNKNOWN	2
#define	FAX_COVERONLY	4

bool comgetfaxopts(std::string_view, int *);

#endif
