/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mail.H"
#include <cstring>

mail::folder::sort::sort(bool foldersFirstArg) // TRUE: sort folders first
	: foldersFirst(foldersFirstArg)
{
}

mail::folder::sort::~sort()
{
}

bool mail::folder::sort::operator()(const mail::folder *a,
				    const mail::folder *b)
{
	int an=a->hasSubFolders() ? 1:0;
	int bn=b->hasSubFolders() ? 1:0;

	if (!foldersFirst)
	{
		an= 1-an;
		bn= 1-bn;
	}

	if (an != bn)
		return an < bn;

	return (strcoll(a->getName().c_str(), b->getName().c_str()) < 0);
}
