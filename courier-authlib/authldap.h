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

void auth_ldap_enumerate( void(*cb_func)(const char *name,
					 uid_t uid,
					 gid_t gid,
					 const char *homedir,
					 const char *maildir,
					 const char *options,
						void *void_arg),
			  void *void_arg);

int auth_ldap_changepw(const char *, const char *, const char *,
		       const char *);


#endif
