
/*
** Copyright 2000-2008 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	"courierauthsasl.h"
#include	"libhmac/hmac.h"
#include	"authsaslclient.h"
#include	<stdlib.h>
#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<errno.h>

int authsaslclient_cram(const struct authsaslclientinfo *info,
			const char *challenge,
			const struct hmac_hashinfo *hashinfo)
{
char	*base64buf=malloc(strlen(challenge)+1);
unsigned char	*keybuf;
char *p;
const	char *userid=info->userid ? info->userid:"";
const	char *password=info->password ? info->password:"";
int	i;

	if (!base64buf)
	{
		perror("malloc");
		return (AUTHSASL_ERROR);
	}
	strcpy(base64buf, challenge);

	if ( (i=authsasl_frombase64(base64buf))<0 ||
	     (keybuf=(unsigned char *)malloc(hashinfo->hh_L*3)) == 0)
	{
		free(base64buf);
		perror("malloc");
		return (AUTHSASL_ERROR);
	}

	hmac_hashkey( hashinfo, password, strlen(password),
                        keybuf, keybuf+hashinfo->hh_L );

	hmac_hashtext( hashinfo, base64buf, i,
                keybuf, keybuf+hashinfo->hh_L,
                keybuf+hashinfo->hh_L*2);

	free(base64buf);
	base64buf=malloc(strlen(userid)+2+hashinfo->hh_L*2);
	if (!base64buf)
	{
		perror("malloc");
		free(keybuf);
		return (AUTHSASL_ERROR);
	}
	strcat(strcpy(base64buf, userid), " ");
	p=base64buf+strlen(base64buf);
	for (i=0; i<hashinfo->hh_L; i++)
	{
	static const char xdigit[]="0123456789abcdef";
	int	c=keybuf[hashinfo->hh_L*2+i];

		*p++ = xdigit[ (c >> 4) & 0x0F ];
		*p++ = xdigit[c & 0x0F];
        }
	*p=0;
	free(keybuf);
	keybuf=(unsigned char *)authsasl_tobase64(base64buf, -1);
	free(base64buf);

	if (!keybuf)
	{
		perror("malloc");
		free(keybuf);
		return (AUTHSASL_ERROR);
	}
	i= (*info->final_conv_func)((char *)keybuf, info->conv_func_arg);
	free(keybuf);
	return (i);
}
