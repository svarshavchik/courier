#ifndef	ldapaliasdrc_h
#define ldapaliasdrc_h

#ifdef __cplusplus
extern "C" {
#endif

/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
**
*/

#include	"config.h"
#include	<stdio.h>

const char *ldapaliasd_config(const char *);
FILE *ldapaliasd_connect();

void ldapaliasd_configchanged();

#define	LDAPALIASDCONFIGFILE	SYSCONFDIR	"/ldapaliasrc"
#define	SOCKETFILE		LOCALSTATEDIR	"/tmp/ldapaliasd"
#define	LOCKFILE		LOCALSTATEDIR	"/tmp/ldapaliasd.lock"
#define	PIDFILE			LOCALSTATEDIR	"/tmp/ldapaliasd.pid"

#ifdef __cplusplus
}
#endif

#endif
