/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comqueuename_h
#define	comqueuename_h

#include	<sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

const char *qmsgsdir(ino_t);
const char *qmsgsctlname(ino_t);
const char *qmsgsdatname(ino_t);

const char *qmsgqdir(time_t);
const char *qmsgqname(ino_t, time_t);

void qmsgunlink(ino_t, time_t);

#ifdef	__cplusplus
}
#endif

#endif
