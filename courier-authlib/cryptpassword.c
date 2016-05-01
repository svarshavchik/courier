/*
** Copyright 2001-2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdlib.h>
#if	HAVE_CRYPT_H
#include	<crypt.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include	"auth.h"
#include        "md5/md5.h"
#include	"sha1/sha1.h"
#include	"random128/random128.h"


#if HAVE_CRYPT
#if NEED_CRYPT_PROTOTYPE
extern char *crypt(const char *, const char *);
#endif
#endif

static const char crypt_salt[65]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";

static const char *ssha_hash_int(const char *pw)
{
	random128binbuf randbuf;

	random128_binary(&randbuf);

	return ssha_hash(pw, randbuf);
}

static const char *crypt_md5_wrapper(const char *pw)
{
	struct timeval tv;
	char salt[10];
	int i;

	gettimeofday(&tv, NULL);

	tv.tv_sec |= tv.tv_usec;
	tv.tv_sec ^= getpid();

	strcpy(salt, "$1$");

	for (i=3; i<8; i++)
	{
		salt[i]=crypt_salt[ tv.tv_sec % 64 ];
		tv.tv_sec /= 64;
	}

	strcpy(salt+i, "$");

	return (md5_crypt(pw, salt));
}

char *authcryptpasswd(const char *password, const char *encryption_hint)
{
	const char *(*hash_func)(const char *)=0;
	const char *pfix=0;
	const char *p;
	char *pp;

	if (!encryption_hint || strncmp(encryption_hint, "$1$", 3) == 0)
	{
		pfix="";
		hash_func=crypt_md5_wrapper;
	}

	if (!encryption_hint || strncasecmp(encryption_hint, "{MD5}", 5) == 0)
	{
		hash_func= &md5_hash_courier;
		pfix="{MD5}";
	}

	if (!encryption_hint || strncasecmp(encryption_hint, "{MD5RAW}", 5)
	    == 0)
	{
		hash_func= &md5_hash_raw;
		pfix="{MD5RAW}";
	}

	if (!encryption_hint || strncasecmp(encryption_hint, "{SHA}", 5) == 0)
	{
		hash_func= &sha1_hash;
		pfix="{SHA}";
	}

	if (!encryption_hint || strncasecmp(encryption_hint, "{SSHA}", 6) == 0)
	{
		hash_func= &ssha_hash_int;
		pfix="{SSHA}";
	}

	if (!encryption_hint ||
	    strncasecmp(encryption_hint, "{SHA256}", 8) == 0)
	{
		hash_func= &sha256_hash;
		pfix="{SHA256}";
	}

	if (!encryption_hint ||
	    strncasecmp(encryption_hint, "{SHA512}", 8) == 0)
	{
		hash_func= &sha512_hash;
		pfix="{SHA512}";
	}

	if (!hash_func)
	{
		hash_func= &ssha_hash_int;
		pfix="{SSHA}";
	}

	p= (*hash_func)(password);
	if (!p || (pp=malloc(strlen(pfix)+strlen(p)+1)) == 0)
		return (0);

	return (strcat(strcpy(pp, pfix), p));
}
