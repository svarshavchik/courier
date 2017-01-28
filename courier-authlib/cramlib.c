/*
** Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	"courierauthsasl.h"
#include	"courierauth.h"
#include	"courierauthdebug.h"
#include	"libhmac/hmac.h"

static int nybble(int c)
{
	if (c >= '0' && c <= '9')	return (c-'0');
	if (c >= 'a' && c <= 'f')	return (c-'a'+10);
	if (c >= 'A' && c <= 'F')	return (c-'A'+10);
	return (-1);
}

static int do_auth_verify_cram(struct hmac_hashinfo *hash,
	const char *challenge, const char *response,
	const char *hashsecret)
{
unsigned char *context;
unsigned i;

	if (strlen(hashsecret) != hash->hh_L*4 ||
		strlen(response) != hash->hh_L*2)
		return (-1);

	if ((context=malloc(hash->hh_L*3)) == 0)	return (-1);

	for (i=0; i<hash->hh_L*2; i++)
	{
	int	a=nybble(hashsecret[i*2]), b=nybble(hashsecret[i*2+1]);

		if (a < 0 || b < 0)
		{
			free(context);
			return (-1);
		}
		context[i]= a*16 + b;
	}

	hmac_hashtext(hash, challenge, strlen(challenge),
		context, context+hash->hh_L,
		context+hash->hh_L*2);

	for (i=0; i<hash->hh_L; i++)
	{
	int	a=nybble(response[i*2]), b=nybble(response[i*2+1]);

		if ( (unsigned char)(a*16+b) !=
			context[hash->hh_L*2+i])
		{
			free(context);
			return (-1);
		}
	}
	free(context);
	return (0);
}

int auth_verify_cram(struct hmac_hashinfo *hash,
	const char *challenge, const char *response,
	const char *hashsecret)
{
int rc;

	rc = do_auth_verify_cram(hash, challenge, response, hashsecret);
	DPRINTF(rc ? "cram validation failed" : "cram validation succeeded");
	return rc;
}

static int do_auth_get_cram(const char *authtype, char *authdata,
			    struct cram_callback_info *craminfo, int logerr)
{
int	i;
int	challenge_l;
int	response_l;

	if (strncmp(authtype, "cram-", 5) ||
		(craminfo->challenge=strtok(authdata, "\n")) == 0 ||
		(craminfo->response=strtok(0, "\n")) == 0)
	{
		if (logerr)
		{
			DPRINTF("Unsupported authentication type: %s", authtype);
		}
		errno=EPERM;
		return (-1);
	}

	for (i=0; hmac_list[i]; i++)
		if (strcmp(hmac_list[i]->hh_name, authtype+5) == 0)
			break;

	if (logerr)
	{
		DPRINTF("cram: challenge=%s, response=%s", craminfo->challenge,
			craminfo->response);
	}

	if (hmac_list[i] == 0
		|| (challenge_l=authsasl_frombase64(craminfo->challenge)) < 0
		|| (response_l=authsasl_frombase64(craminfo->response)) < 0)
	{
		if (logerr)
		{
			DPRINTF("cram: invalid base64 encoding, or unknown method: %s",
				authtype);
		}
		errno=EACCES;
		return (-1);
	}
	craminfo->h=hmac_list[i];

	for (i=response_l; i > 0; )
	{
		if (craminfo->response[i-1] == ' ')
			break;
		--i;
	}

	if (i == 0)
	{
		if (logerr)
		{
			DPRINTF("cram: invalid base64 encoding");
		}
		errno=EACCES;
		return (-1);
	}
	craminfo->response[i-1]=0;
	craminfo->user = craminfo->response;
	craminfo->response += i;
	response_l -= i;

	/* Since base64decoded data is always lesser in size (at least),
	** we can do the following:
	*/
	craminfo->challenge[challenge_l]=0;
	craminfo->response[response_l]=0;

	if (logerr)
	{
		/* we rely on DPRINTF doing a "safe" print here */
		DPRINTF("cram: decoded challenge/response, username '%s'",
			craminfo->user);
	}
	return (0);
}

int auth_cram_callback(struct authinfo *a, void *vp)
{
struct cram_callback_info *cci=(struct cram_callback_info *)vp;
unsigned char *hashbuf;
unsigned char *p;
unsigned i;
static const char hex[]="0123456789abcdef";
int	rc;

	if (!a->clearpasswd)
		return (-1);

	/*
		hmac->hh_L*2 will be the size of the binary hash.

		hmac->hh_L*4+1 will therefore be size of the binary hash,
		as a hexadecimal string.
	*/

	if ((hashbuf=malloc(cci->h->hh_L*6+1)) == 0)
		return (1);

	hmac_hashkey(cci->h, a->clearpasswd, strlen(a->clearpasswd),
		hashbuf, hashbuf+cci->h->hh_L);

	p=hashbuf+cci->h->hh_L*2;

	for (i=0; i<cci->h->hh_L*2; i++)
	{
	char	c;

		c = hex[ (hashbuf[i] >> 4) & 0x0F];
		*p++=c;

		c = hex[ hashbuf[i] & 0x0F];
		*p++=c;

		*p=0;
	}

	rc=auth_verify_cram(cci->h, cci->challenge, cci->response,
		(const char *)hashbuf+cci->h->hh_L*2);
	free(hashbuf);

	if (rc)	return (rc);

	return (*cci->callback_func)(a, cci->callback_arg);
}

int auth_get_cram(const char *authtype, char *authdata,
		  struct cram_callback_info *craminfo)
{
	return do_auth_get_cram(authtype, authdata, craminfo, 1);
}

int auth_get_cram_silent(const char *authtype, char *authdata,
			 struct cram_callback_info *craminfo)
{
	return do_auth_get_cram(authtype, authdata, craminfo, 0);
}
