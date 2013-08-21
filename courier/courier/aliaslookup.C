/*
** Copyright 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"aliases.h"

#include	<string.h>
#include	<iostream>

class MyAliasHandler : public AliasHandler {
public:
	void Alias(const char *ptr)
	{
		std::cout << ptr << std::endl;
	}
};

int main(int argc, char **argv)
{
	AliasSearch srch;
	MyAliasHandler h;
	int n=1;
	const char *modname=NULL;

	while (n < argc)
	{
		if (strcmp(argv[n], "-m") == 0 && n+1 < argc)
		{
			modname=argv[++n];
			++n;
			continue;
		}
		break;
	}

	srch.Open(modname);

	while (n < argc)
	{
		srch.Search(argv[n++], h);
	}
	exit(0);
}
