/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	esmtpconfig_h
#define	esmtpconfig_h

#include	<time.h>

#ifdef	__cplusplus
extern "C" {
#endif


time_t config_time_esmtptimeout();
time_t config_time_esmtpkeepalive();
time_t config_time_esmtpkeepaliveping();
time_t config_time_esmtphelo();
time_t config_time_esmtpquit();
time_t config_time_esmtpconnect();
time_t config_time_esmtpdata();
time_t config_time_esmtpdelay();

#ifdef	__cplusplus
}
#endif

#endif
