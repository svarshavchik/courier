/*
** Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_CRYPT_H
#include	<crypt.h>
#endif
#include	"auth.h"
#include	"courierauthdebug.h"


#if HAVE_CRYPT
#if NEED_CRYPT_PROTOTYPE
extern char *crypt(const char *, const char *);
#endif
#endif

extern int authcheckpasswordmd5(const char *, const char *);
extern int authcheckpasswordsha1(const char *, const char *);

static int safe_strcmp(const char *a, const char *nullable_b)
{
	if (!nullable_b)
		return -1;
	return strcmp(a, nullable_b);
}

static int do_authcheckpassword(const char *password, const char *encrypted_password)
{
	char *cpass;
	if (strncmp(encrypted_password, "$1$", 3) == 0
	    || strncasecmp(encrypted_password, "{MD5}", 5) == 0
	    || strncasecmp(encrypted_password, "{MD5RAW}", 8) == 0
	    )
		return (authcheckpasswordmd5(password, encrypted_password));

	if (strncasecmp(encrypted_password, "{SHA}", 5) == 0 ||
	    strncasecmp(encrypted_password, "{SHA256}", 8) == 0 ||
	    strncasecmp(encrypted_password, "{SHA512}", 8) == 0 ||
	    strncasecmp(encrypted_password, "{SSHA}", 6) == 0)
		return (authcheckpasswordsha1(password, encrypted_password));


#if	HAVE_CRYPT
	if (strncasecmp(encrypted_password, "{CRYPT}", 7) == 0)
		encrypted_password += 7;
#endif

#if	HAVE_CRYPT

	cpass = crypt(password, encrypted_password);
	if (cpass == NULL) {
		return 1;
	} else {
		return safe_strcmp(encrypted_password, cpass);
	}
#else
	return safe_strcmp(encrypted_password, password)
#endif
}

int authcheckpassword(const char *password, const char *encrypted_password)
{
int rc;

	rc=do_authcheckpassword(password, encrypted_password);
	if (rc == 0)
	{
		DPRINTF("password matches successfully");
	}
	else if (courier_authdebug_login_level >= 2)
	{
		DPRINTF("supplied password '%s' does not match encrypted password '%s'",
			password, encrypted_password);
	}
	else
	{
		DPRINTF("supplied password does not match encrypted password");
	}
	return rc;
}
