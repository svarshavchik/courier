/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"aliases.h"

//
// Iterate through all the addresses in this alias.
//

void	AliasRecord::StartForEach()
{
	Init();
	feptr=list.c_str();
}

std::string AliasRecord::NextForEach()
{
	std::string buf;
	int	l;

	if (!feptr || !*feptr)
	{
		++recnum;
		if (fetch(1))
		{
			--recnum;
			return (buf);
		}
		feptr=list.c_str();
	}

	for (l=0; feptr[l] && feptr[l] != '\n'; l++)
		;

	buf=std::string(feptr, feptr+l);
	feptr += l;
	if (*feptr)	++feptr;
	return (buf);
}
