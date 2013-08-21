#ifndef	authldap_h
#define	authldap_h

/*
**
** Copyright 1998 - 2003 Double Precision, Inc.  See COPYING for
** distribution information.
*/

/* Based on code by Luc Saillard <luc.saillard@alcove.fr>. */

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif


struct authinfo;

int authldapcommon(const char *, const char *,
	const char *, int (*)(struct authinfo *, void *), void *);

void authldapclose();

#endif
