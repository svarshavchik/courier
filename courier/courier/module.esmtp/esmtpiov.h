/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	esmtpiov_h
#define	esmtpiov_h

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	<time.h>
#include	<stdlib.h>
#include	<sys/types.h>

extern time_t iovread_timeout, iovwrite_timeout;
extern void iovflush();
extern int iovwaitfordata(time_t *, int);
extern char *iovreadline();
extern void addiovec(const char *, size_t);
extern void iovreset();

extern void iov_logerror(const char *, const char *);

#endif
