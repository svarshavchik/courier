#ifndef	cramlib_h
#define	cramlib_h

#include	"auth.h"

/*
** Copyright 1998 - 2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/
struct cram_callback_info {
	struct hmac_hashinfo *h;
	char *user;
	char *challenge;
	char *response;
	int (*callback_func)(struct authinfo *, void *);
	void *callback_arg;
	};

extern int auth_cram_callback(struct authinfo *a, void *vp);
/*
** auth_get_cram parses out an authentication request.  It checks whether
** we have the requisite hash function installed, and, if so, base64decodes
** the challenge and the response.
*/

struct hmac_hashinfo;

int auth_get_cram(const char *authtype,			/* authtype */
		  char *authdata,			/* authdata */

		  struct cram_callback_info *craminfo);
		/* Initializes craminfo */
/*
** auth_verify_cram attempts to verify the secret cookie.
*/

int auth_verify_cram(struct hmac_hashinfo *,	/* The hash function */
	const char *,				/* The challenge */
	const char *,				/* The response */
	const char *);				/* Hashed secret, in hex */

#endif
