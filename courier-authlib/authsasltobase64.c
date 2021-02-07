#include	<stdlib.h>
#include	<string.h>
#include	"auth.h"
#include	"rfc822/encode.h"

static int write_challenge(const char *p, size_t l, void *vp)
{
	char **cp=(char **)vp;

	while (l)
	{
		if (*p == '\r' || *p == '\n')
		{
			++p;
			--l;
			continue;
		}
		**cp = *p++;
		++*cp;

		--l;
	}

	return 0;
}

char *authsasl_tobase64(const char *p, int l)
{
	char *write_challenge_buf;
	char *write_challenge_ptr;

	struct libmail_encode_info encodeInfo;

	if (l < 0)	l=strlen(p);

	write_challenge_buf=malloc((l+3)/3*4+1);
	if (!write_challenge_buf)
		return (0);

	write_challenge_ptr=write_challenge_buf;

	libmail_encode_start(&encodeInfo, "base64", &write_challenge,
			     &write_challenge_ptr);

	libmail_encode(&encodeInfo, p, l);
	libmail_encode_end(&encodeInfo);
	*write_challenge_ptr=0;
	return (write_challenge_buf);
}
