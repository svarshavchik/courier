/*
** Copyright 2001-2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	"courierauthsasl.h"
#include	<string.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	"sha1/sha1.h"
#include	"auth.h"


int authcheckpasswordsha1(const char *password, const char *encrypted_password)
{
	if (strncasecmp(encrypted_password, "{SHA}", 5) == 0)
	{
		return (strcmp(encrypted_password+5, sha1_hash(password)));
	}
	if (strncasecmp(encrypted_password, "{SHA256}", 8) == 0)
	{
		return (strcmp(encrypted_password+8, sha256_hash(password)));
	}
	if (strncasecmp(encrypted_password, "{SHA512}", 8) == 0)
	{
		return (strcmp(encrypted_password+8, sha512_hash(password)));
	}
	if (strncasecmp(encrypted_password, "{SSHA}", 6) == 0)
	{
		char *code = NULL; 
		int i;
		SSHA_RAND rand;

		code = strdup(encrypted_password+6);

		if(code == NULL)
		{
			return (-1);
		}

		i = authsasl_frombase64(code);

		if(i == -1 || i < sizeof(SSHA_RAND))
		{
			free(code);
			return (-1);
		}

		memcpy((char *)rand, code+i-sizeof(SSHA_RAND),
		       sizeof(SSHA_RAND));

		i=strcmp(encrypted_password+6, ssha_hash(password, rand));

		free(code);
		return i;

	}
	return (-1);
}
