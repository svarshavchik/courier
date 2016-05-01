#ifndef	authlib_auth_h
#define	authlib_auth_h

/*
** Copyright 1998 - 2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<sys/types.h>
#include	"courierauth.h"

#ifdef	__cplusplus
extern "C" {
#endif



/* authcheckpassword is the general password validation routine.
** It returns 0 if the password matches the encrypted password.
*/

int authcheckpassword(const char *,	/* password */
		      const char *);	/* encrypted password */

	/*
	** authcryptpasswd is a password hashing function, used to create
	** new password.  password is the cleartext password.
	** encryption_hint is a hint to the type of hashing to be used
	** (NULL means use a default hash function).
	*/

char *authcryptpasswd(const char *password,
		      const char *encryption_hint);


int auth_sys_common( int (*auth_pre_func)(const char *,
					  const char *,
					  int (*)(struct authinfo *,
						  void *),
					  void *),
		     const char *user,
		     const char *pass,
		     const char *service,
		     int (*callback_func)(struct authinfo *, void *),
		     void *callback_arg);


#ifdef	__cplusplus
}
#endif

#endif
