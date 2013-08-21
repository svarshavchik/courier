/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"aliases.h"

AliasRecord::~AliasRecord()
{
}

void AliasRecord::Init(std::string n)
{
	listname=n;
	recnum=0;
	feptr=0;
}


int AliasRecord::Init()
{
	feptr=0;
	recnum=0;
	if (fetch(0) == 0)	return (0);
	return (1);
}

void AliasRecord::mkrecname()
{
	recname=listname;
	if (recnum == 0)	return;

	recname += '\n';

unsigned	n=recnum;

	do
	{
		recname += "0123456789ABCDEF"[n % 16];
		n /= 16;
	} while (n);
}

int AliasRecord::fetch(int keep) // TODO: trace who sets the keep arg
{
	mkrecname();

	std::string value=gdbm ? gdbm->Fetch(recname, std::string("")):"";

	if (value.size() > 0)
	{
		list=value;
		return (0);
	}
	if (!keep)
		list="";
	return (1);
}
