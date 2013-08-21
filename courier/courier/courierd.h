/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	courierd_h
#define	courierd_h

#if	HAVE_CONFIG_H
#undef	PACKAGE
#undef	VERSION
#include	"config.h"
#endif

#include	<sys/types.h>
#include	<time.h>

extern pid_t couriera, courierb;
extern unsigned queuelo, queuehi;

#endif
