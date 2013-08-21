/*
** Copyright 1998 - 2003 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	creadtime_h
#define	creadtime_h

#include	"courier.h"
#include	<time.h>
#include	<stdlib.h>
#include	<ctype.h>

#ifdef	__cplusplus
extern "C" {
#endif

time_t config_readtime(const char *, time_t);

time_t config_time_queuetime();
time_t config_time_faxqueuetime();
time_t config_time_warntime();
time_t config_time_respawnlo();
time_t config_time_respawnhi();

time_t config_time_retryalpha();
time_t config_time_retrygamma();
time_t config_time_submitdelay();

time_t config_time_queuefill();

#ifdef	__cplusplus
}
#endif

#endif
