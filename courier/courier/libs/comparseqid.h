/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comparseqid_h
#define	comparseqid_h

#include	"config.h"
#include	<sys/types.h>
#include	<time.h>

#ifdef	__cplusplus
extern "C" {
#endif

int comparseqid(const char *, ino_t *);

#ifdef	__cplusplus
}
#endif
#endif
