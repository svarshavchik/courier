/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comstrtotime_h
#define	comstrtotime_h

#include	"config.h"
#include	<sys/types.h>
#include	<time.h>

#ifdef	__cplusplus
extern "C" {
#endif

time_t strtotime(const char *p);

#ifdef	__cplusplus
}
#endif
#endif
