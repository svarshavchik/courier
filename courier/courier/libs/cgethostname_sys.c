/*
** Copyright 2025 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"libs/courier_lib_config.h"
#endif
#include	"courier.h"
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#if     HAS_GETHOSTNAME
#else
int gethostname(const char *, size_t);
#endif

int gethostname_sys(char *buf, size_t l)
{
	return gethostname(buf, l);
}
