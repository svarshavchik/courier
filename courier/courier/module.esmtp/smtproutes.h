/*
** Copyright 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	smtproutes_h
#define	smtproutes_h

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

extern char *smtproutes(const char *, int *);

#define ROUTE_STARTTLS	1
#define ROUTE_TLS_REQUIRED	2
#define ROUTE_SMTPS	4
#endif
