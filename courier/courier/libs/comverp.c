/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"comverp.h"
#include	"courier.h"
#include	"comctlfile.h"
#include	<stdlib.h>
#include	<string.h>

static const char xdigit[]="0123456789ABCDEF";

/*
** Encode the recipient address.  We have to handle UUCP addresses as well,
** that do not contain a @, so there's a slight deviation from the draft
** here...
*/

size_t verp_encode(const char *receipient, char *buf)
{
size_t	i=1;
unsigned j;

	while (*receipient)
	{
		if (*receipient == '@' && strchr(receipient+1, '@') == 0)
		{
			if (buf)	*buf++ = '=';
			++i;
			++receipient;
			continue;
		}

		switch (*receipient)	{
		case '=':
			if (strchr(receipient, '@'))
			{
				if (buf)	*buf++ = *receipient;

				++receipient;
				++i;
				break;
			}

			/* UUCP, no @, must encode it */

		case '@':
		case ':':
		case '%':
		case '!':
		case '+':
		case '-':
		case '[':
		case ']':
			j=(unsigned)(unsigned char)*receipient++;

			if (buf)
			{
				*buf++='+';
				*buf++=xdigit[ j / 16 ];
				*buf++=xdigit[ j % 16 ];
			}
			i += 3;
			break;
		default:
			if (buf)	*buf++ = *receipient;

			++receipient;
			++i;
		}
	}
	if (buf)	*buf=0;
	return (i);
}

/*
** Get the return address of a message.  If the message has the VERP flag
** set, construct the VERPed address appropriately.
*/

char *verp_getsender(struct ctlfile *ctf, const char *receipient)
{
const char *p, *r;
char	*q, *s;

	if (ctf->sender[0] == '\0')
		return (strcpy(courier_malloc(1), ""));

	p=ctf->sender;

	if (ctlfile_searchfirst(ctf, COMCTLFILE_VERP) < 0 ||
		(r=strrchr(p, '@')) == 0)
		return (strcpy(courier_malloc(strlen(p)+1), p));
		/* VERP not set */

	q=courier_malloc(strlen(p)+1+verp_encode(receipient, 0));
	memcpy(q, p, r-p);
	s=q + (r-p);
	*s++='-';
	s += verp_encode(receipient, s)-1;
	strcpy(s, r);
	return (q);
}
