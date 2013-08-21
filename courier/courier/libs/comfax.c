/*
** Copyright 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "courier_lib_config.h"
#include "comfax.h"
#include <stdio.h>
#include <string.h>


/*
**   @fax is the domain for the courierfax module.
**
**   @fax may be followed by:
**
**     -lowres    - send a low-resolution fax.
**     -ignore    - ignore unknown MIME types.
**     -cover     - generate a cover page only.
*/

int comgetfaxopts(const char *s, int *flags)
{
	return (comgetfaxoptsn(s, strlen(s), flags));
}

/*
**   Return 0 if s/sl specifies a valid domain for the courierfax module.
*/

int comgetfaxoptsn(const char *s, int sl, int *flags)
{
	*flags=0;

	if (sl < 3)
		return (-1);

	if (strncasecmp(s, "fax", 3))
		return (-1);

	s += 3;
	sl -= 3;

	while (sl)
	{
		if (*s != '-')
			return (-1);

		++s;
		--sl;
		if (!sl)
			return (-1);

		if (sl >= 6 && strncasecmp(s, "lowres", 6) == 0)
		{
			s += 6;
			sl -= 6;
			*flags |= FAX_LOWRES;
		}
		else if (sl >= 6 && strncasecmp(s, "ignore", 6) == 0)
		{
			s += 6;
			sl -= 6;
			*flags |= FAX_OKUNKNOWN;
		}
		else if (sl >= 5 && strncasecmp(s, "cover", 5) == 0)
		{
			s += 5;
			sl -= 5;
			*flags |= FAX_COVERONLY;
		}
		else
			return (-1);
	}

	return (0);
}
