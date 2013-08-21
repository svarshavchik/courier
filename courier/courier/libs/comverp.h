/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comverp_h
#define	comverp_h

#if	HAVE_CONFIG_H
#include	"libs/courier_lib_config.h"
#endif

#include	<stdlib.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct ctlfile;

extern char *verp_getsender(struct ctlfile *, const char *);
extern size_t verp_encode(const char *, char *);

#ifdef	__cplusplus
}
#endif

#endif
