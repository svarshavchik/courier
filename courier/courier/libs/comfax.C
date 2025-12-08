/*
** Copyright 2002 S. Varshavchik.
** See COPYING for distribution information.
*/

#include "libs/courier_lib_config.h"
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

bool comgetfaxopts(std::string_view s, int *flags)
{
	*flags=0;

	if (s.size() < 3)
		return false;

	if (s.substr(0, 3) != "fax")
		return false;

	s=s.substr(3);

	while (s.size())
	{
		if (s[0] != '-')
			return false;

		s=s.substr(1);
		if (s.empty())
			return false;

		if (s.size() >= 6 && s.substr(0, 6) == "lowres")
		{
			s=s.substr(6);
			*flags |= FAX_LOWRES;
		}
		else if (s.size() >= 6 && s.substr(0, 6) == "ignore")
		{
			s=s.substr(6);
			*flags |= FAX_OKUNKNOWN;
		}
		else if (s.size() >= 5 && s.substr(0, 5) == "cover")
		{
			s=s.substr(5);
			*flags |= FAX_COVERONLY;
		}
		else
			return false;
	}

	return true;
}
