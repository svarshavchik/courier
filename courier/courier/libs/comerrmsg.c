/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

/*
	Take an error number, and a possibly multiline error message text.
	Prefix each line in the error message with the error number, as in
	RFC821.

	makeerrmsgtext also ignores existing RFC821 error numbers in errtext,
	so that you can take the output of makeerrmsgtext, append some
	additional text to it, and call makeerrmsgtext again, to reformat it.
	
*/

char	*makeerrmsgtext(int errnum, const char *errtext)
{
char	errnumbuf[4];
int	i;
const char *p;
char	*buf, *q, *r;
int	lastchar;

	if (!errtext || !*errtext)	errtext="Failed.";

	errnumbuf[3]=0;
	for (i=2; i >= 0; --i)
	{
		errnumbuf[i]= (errnum % 10)+'0';
		errnum /= 10;
	}

	/*
	** Calculate size of buffer we'll need.  For each line, we
	** add four bytes for the error number.
	** One line errors may need \n appended.
	*/

	i=6+strlen(errtext);
		/*
		** First line's toll, plus optional trailing \r\n,
		** and null byte.
		*/

	for (p=errtext; *p; p++)
		if (*p == '\n')	i += 4;

	if ((buf=malloc(i)) == 0)	clog_msg_errno();
	for (q=r=buf, p=errtext; *p; )
	{
		r=q;
		strcpy(q, errnumbuf);
		q += 3;
		*q++ = '-';

		if (isdigit((int)(unsigned char)p[0]) &&
			isdigit((int)(unsigned char)p[1]) &&
			isdigit((int)(unsigned char)p[2]) &&
			(p[3] == '-' || p[3] == ' '))
			p += 4;	/* Remove old error message number */

		while (*p)
		{
			lastchar=*q++ = *p++;

			if (lastchar != '\n' && *p == '\0')
			{
				*q++='\n';
			}
			if (lastchar == '\n')	break;
		}
	}
	r[3]=' ';
	*q=0;
	return (buf);
}
